#include "ecpri.hpp"
#include "dpdk_port.hpp"
#include "dsp.hpp"
#include "gui.hpp"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_lcore.h>

#include <quill/SimpleSetup.h>
#include <quill/LogFunctions.h>

#include <cstdio>
#include <cmath>
#include <csignal>
#include <atomic>
#include <string>
#include <stdexcept>
#include <getopt.h>

// Global stop flag
static std::atomic<bool> g_stop{false};

static void sig_handler(int) {
    g_stop = true;
}

// TODO: improve logs(cores, marks, ...)
auto* g_logger = quill::simple_logger();

// TODO: Receiver parameters

//
//  RX loop
//
static int rx_lcore_task(void* arg) {
    auto* ring = static_cast<rte_ring*>(arg);

    constexpr uint16_t PORT_ID = 0;
    rte_mbuf* burst[dpdk::BURST_SIZE];
    unsigned long long total_cnt = 0;
    unsigned long long drop_cnt = 0;

    quill::info(g_logger, "RX lcore {} started\n", rte_lcore_id());

    while (!g_stop.load(std::memory_order_relaxed)) {
        const auto buf_cnt = rte_eth_rx_burst(PORT_ID, 0, burst, dpdk::BURST_SIZE);

        uint16_t valid_cnt = 0;
        for (uint16_t i = 0; i < buf_cnt; ++i) {
            rte_mbuf* mbuf = burst[i];

            if (rte_pktmbuf_pkt_len(mbuf) < sizeof(dpdk::EthHeader) +
                                            sizeof(ecpri::CommonHeader) +
                                            sizeof(ecpri::IQDataHeader)) {
                quill::warning(g_logger, "Wrong packet size\n");
                rte_pktmbuf_free(mbuf);
                continue;
                                            }

            const auto pkt_data = rte_pktmbuf_mtod(mbuf, const char*);

            // Packet validation
            auto* eth_hdr = reinterpret_cast<const dpdk::EthHeader*>(pkt_data);
            if (rte_be_to_cpu_16(eth_hdr->ether_type) != dpdk::ECPRI_ETHERTYPE) {
                quill::warning(g_logger, "Wrong packet type\n");
                rte_pktmbuf_free(mbuf);
                continue;
            }

            const auto* cmn_hdr = reinterpret_cast<const ecpri::CommonHeader*>(pkt_data + sizeof(dpdk::EthHeader));
            if (cmn_hdr->msg_type != 0x00) {
                rte_pktmbuf_free(mbuf);
                continue;
            }

            burst[valid_cnt++] = mbuf;  // compact valid mbufs
        }

        const auto enqueued_cnt = rte_ring_enqueue_burst(
            ring,
            reinterpret_cast<void**>(burst),
            valid_cnt,
            nullptr);

        // Didn't fit - just drop
        for (uint16_t i = enqueued_cnt; i < valid_cnt; ++i) {
            rte_pktmbuf_free(burst[i]);
            ++drop_cnt;
        }

        total_cnt += enqueued_cnt;
    }

    quill::info(g_logger, "RX lcore {} stopped. Received {}  Dropped {}\n",
        rte_lcore_id(), total_cnt, drop_cnt);

    return 0;
}

//
// DSP(FFT) loop
//
static int dsp_lcore_task(void* arg) {
    quill::info(g_logger, "DSP lcore {} started\n", rte_lcore_id());
    return dsp::DspCore::lcore_entry(arg);
}

//
// main
//
int main(int argc, char* argv[]) {

    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);

    // EAL init
    const int eal_ret = rte_eal_init(argc, argv);
    if (eal_ret < 0) {
        throw std::runtime_error("rte_eal_init failed");
    }
    if (rte_lcore_count() < 3) throw std::runtime_error("Нужно 3 lcore: --lcores 0-2");


    // TODO: add user args

    rte_mempool* mp = rte_pktmbuf_pool_create(
        "rx_mp", dpdk::MBUF_POOL_SIZE, dpdk::MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, static_cast<int>(rte_socket_id()));
    if (!mp) throw std::runtime_error("mbuf pool creation failed");

    // Use first available port (should be the TAP vdev)
    constexpr uint16_t PORT_ID = 0;
    auto link = dpdk::port_init(PORT_ID, mp);
    quill::info(g_logger, "[dpdk] port {}  link={}  speed={} Mbps\n",
                PORT_ID,
                link.link_status ? "UP" : "DOWN",
                link.link_speed);

    quill::info(g_logger, "Starting RX on port {}  (Ctrl-C to stop)\n", PORT_ID);

    // SPSC ring with IQ data
    // RX lcore -> DSP lcore
    constexpr unsigned IQ_RING_SIZE = 4096;
    rte_ring* iq_ring = rte_ring_create("IQ_RING", IQ_RING_SIZE,
                                        rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (!iq_ring) throw std::runtime_error("IQ_RING creation failed");

    // SPSC ring with SpectrumFrame pointers
    // DSP lcore -> MAIN lcore
    constexpr unsigned FRM_RING_SIZE = 16;
    rte_ring* frame_ring = rte_ring_create("FRAME_RING", FRM_RING_SIZE,
                                            rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    if (!frame_ring) throw std::runtime_error("FRAME_RING creation failed");

    dsp::DspCore dsp_core{iq_ring, frame_ring};

    const auto dsp_lcore = rte_get_next_lcore(rte_get_main_lcore(), 1, 0);
    rte_eal_remote_launch(dsp_lcore_task, &dsp_core, dsp_lcore);

    const auto rx_lcore = rte_get_next_lcore(dsp_lcore, 1, 0);
    rte_eal_remote_launch(rx_lcore_task, iq_ring, rx_lcore);

    gui::SpectrumView view;
    while (view.is_open() && !g_stop.load(std::memory_order_relaxed)) {
        view.update(dsp_core, frame_ring);
    }

    g_stop.store(true);

    rte_eal_wait_lcore(dsp_lcore);
    rte_eal_wait_lcore(rx_lcore);
    rte_eth_dev_stop(PORT_ID);
    rte_ring_free(iq_ring);
    rte_ring_free(frame_ring);
    rte_eal_cleanup();

    quill::info(g_logger, "BYE!\n");

    return 0;
}
