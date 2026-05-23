//
// sudo ./ecpri_generator --vdev net_tap0,iface=tap0 -- --freq 1000 --fs 30720 --amplitude 16000 --rate-hz 1000
//
// test opts: --no-huge --no-pci --vdev=net_tap0,iface=dtap0 -- --freq 1000 --fs 30720 --amplitude 16000 --rate-hz 1000
//
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
#include <getopt.h>

// Global stop flag
static std::atomic<bool> g_stop{false};

static void sig_handler(int) {
    g_stop = true;
}

// Generator parameters
struct GenParams final {
    double signal_freq_hz = 1'000.0;  // baseband tone frequency
    double sample_rate_hz = 30'720.0; // IQ sample rate (30.72 MSPS typical in 5G)
    int16_t amplitude = 16'000;       // FS ≈ 32767
    uint32_t packet_rate_hz = 1'000;  // packets/sec  (controls TX pacing)
    uint16_t pc_id = 0;               // eCPRI Port/Channel ID
};

// Parse EAL and Signal Generator arguments
static GenParams parse_args(int argc, char** argv) {
    GenParams p{};
    static option long_opts[] = {
        {"freq",      required_argument, nullptr, 'f'},
        {"fs",        required_argument, nullptr, 's'},
        {"amplitude", required_argument, nullptr, 'a'},
        {"rate-hz",   required_argument, nullptr, 'r'},
        {"pc-id",     required_argument, nullptr, 'p'},
        {nullptr, 0, nullptr, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "f:s:a:r:p:", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'f':
                p.signal_freq_hz = std::stod(optarg);
                break;
            case 's':
                p.sample_rate_hz = std::stod(optarg);
                break;
            case 'a':
                p.amplitude = static_cast<int16_t>(std::stoi(optarg));
                break;
            case 'r':
                p.packet_rate_hz = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'p':
                p.pc_id = static_cast<uint16_t>(std::stoul(optarg));
                break;
            default:
                throw std::runtime_error("wrong argument");
        }
    }
    return p;
}


struct IQGenerator final {

    // Synthesise one packet worth of IQ samples
    template<std::size_t Size>
    void fill_iq_buf(ecpri::IQSample (&buf)[Size]) {
        for (long long i = 0; i < Size; ++i) {
            const ecpri::IQSample s{
                static_cast<int16_t>(params_.amplitude * std::cos(phase_)),
                static_cast<int16_t>(params_.amplitude * std::sin(phase_))
            };
            buf[i] = s.to_net();
            phase_ += phase_inc_;
            if (phase_ > 2.0 * std::numbers::pi) {
                phase_ -= 2.0 * std::numbers::pi;
            }
        }
    }

    const GenParams params_{};
    double phase_inc_ = 2.0 * std::numbers::pi * params_.signal_freq_hz / params_.sample_rate_hz;
    double phase_ = 0.0;
};

//
// main
//
int main(int argc, char* argv[]) {

    auto* logger = quill::simple_logger();

    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);

    // EAL init
    const int eal_ret = rte_eal_init(argc, argv);
    if (eal_ret < 0) {
        throw std::runtime_error("rte_eal_init failed");
    }
    argc -= eal_ret;
    argv += eal_ret;

    auto [
        signal_freq_hz,
        sample_rate_hz,
        amplitude,
        packet_rate_hz,
        pc_id
        ] = parse_args(argc, argv);

    quill::info(logger, "[gen] tone={} Hz  fs={} Hz  amp={}  pkt_rate={} pps\n",
                signal_freq_hz, sample_rate_hz, amplitude, packet_rate_hz);

    // DPDK mbuf pool
    rte_mempool* mp = rte_pktmbuf_pool_create(
        "gen_mp", dpdk::MBUF_POOL_SIZE, dpdk::MBUF_CACHE_SIZE, 0,
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

    // Timing
    const uint64_t hz = rte_get_tsc_hz();
    const uint64_t ticks_pkt = hz / packet_rate_hz;  // TSC ticks between packets
    uint64_t next_tx = rte_rdtsc() + ticks_pkt;

    // Signal synthesis state
    IQGenerator signal_gen;

    quill::info(logger, "[gen] starting TX loop on port {}  (Ctrl-C to stop)\n", port_id);

    unsigned long long tx_total = 0;
    unsigned long long tx_drop = 0;

    while (!g_stop) {
        // Pacing
        while (rte_rdtsc() < next_tx) { }
        next_tx += ticks_pkt;

        rte_mbuf* mbuf = rte_pktmbuf_alloc(mp);
        if (!mbuf) {
            ++tx_drop;
            continue;
        }

        constexpr uint16_t frame_len = sizeof(dpdk::EthHeader) + sizeof(ecpri::IQFrame);
        auto* pkt_data = rte_pktmbuf_append(mbuf, frame_len);
        if (!pkt_data) {
            rte_pktmbuf_free(mbuf);
            ++tx_drop;
            continue;
        }

        { // Fill ethernet header
            auto* eth_hdr = reinterpret_cast<dpdk::EthHeader*>(pkt_data);
            eth_hdr->dst.fill(0xFF); // broadcast
            eth_hdr->src.fill(0x00);
            eth_hdr->ether_type = rte_cpu_to_be_16(dpdk::ECPRI_ETHERTYPE);
        }

        { // Fill eCPRI IQ frame
            auto* eth_payload = pkt_data + sizeof(dpdk::EthHeader);
            auto* ecpri_frame = reinterpret_cast<ecpri::IQFrame*>(eth_payload);
            ecpri::IQFrame::make_hdr(*ecpri_frame, pc_id, 0);

            signal_gen.fill_iq_buf(ecpri_frame->samples);
        }

        // TX
        if (!rte_eth_tx_burst(port_id, 0, &mbuf, 1)) {
            rte_pktmbuf_free(mbuf);
            ++tx_drop;
        } else {
            ++tx_total;
        }

        if (tx_total % 4096 == 0) {
            quill::info(logger, "[gen] tx={}  drop={}\r", tx_total, tx_drop);
        }
    }

    quill::info(logger, "\n[gen] done. tx={} drop={}\n", tx_total, tx_drop);
    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();
    return 0;
}
