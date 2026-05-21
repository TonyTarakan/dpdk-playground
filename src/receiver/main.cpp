#include "ecpri.hpp"
#include "dpdk_port.hpp"

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
#include <print>
#include <getopt.h>

// Global stop flag
static std::atomic<bool> g_stop{false};

static void sig_handler(int) {
    g_stop = true;
}

// TODO: Receiver parameters


int main(int argc, char* argv[]) {

    auto* logger = quill::simple_logger();

    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);

    // EAL init
    const int eal_ret = rte_eal_init(argc, argv);
    if (eal_ret < 0) {
        throw std::runtime_error("rte_eal_init failed");
    }
    // argc -= eal_ret;
    // argv += eal_ret;
    // TODO: add user args


    rte_mempool* mp = rte_pktmbuf_pool_create(
        "rx_mp", dpdk::MBUF_POOL_SIZE, dpdk::MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, static_cast<int>(rte_socket_id()));
    if (!mp) {
        throw std::runtime_error("mbuf pool creation failed");
    }

    // Use first available port (should be the TAP vdev)
    constexpr uint16_t port_id = 0;
    auto link = dpdk::port_init(port_id, mp);
    quill::info(logger, "[dpdk] port {}  link={}  speed={} Mbps\n",
                port_id,
                link.link_status ? "UP" : "DOWN",
                link.link_speed);

    quill::info(logger, "starting RX loop on port {}  (Ctrl-C to stop)\n", port_id);

    unsigned long long rx_total = 0;

    rte_mbuf* burst[dpdk::BURST_SIZE];

    while (!g_stop) {
        const uint16_t buf_cnt = rte_eth_rx_burst(port_id, 0, burst, dpdk::BURST_SIZE);
        if (buf_cnt > 0) {
            quill::info(logger, "Burst of {} buffers\r", buf_cnt);
        }

        for (uint16_t i = 0; i < buf_cnt; ++i) {
            rte_mbuf* mbuf = burst[i];
            const auto pkt_data = rte_pktmbuf_mtod(mbuf, const char*);

            if (rte_pktmbuf_pkt_len(mbuf) < sizeof(dpdk::EthHeader) +
                                            sizeof(ecpri::CommonHeader) +
                                            sizeof(ecpri::IQDataHeader)) {
                quill::warning(logger, "Wrong packet size\n");
                rte_pktmbuf_free(mbuf);
                continue;
            }

            auto* eth_hdr = reinterpret_cast<const dpdk::EthHeader*>(pkt_data);
            if (rte_be_to_cpu_16(eth_hdr->ether_type) != dpdk::ECPRI_ETHERTYPE) {
                quill::warning(logger, "Wrong packet type\n");
                rte_pktmbuf_free(mbuf);
                continue;
            }

            // TODO: process...

            ++rx_total;
            rte_pktmbuf_free(mbuf);
        }
    }

    quill::info(logger, "\n RX Stopped. Received {} buffers\n", rx_total);

    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();
    return 0;
}
