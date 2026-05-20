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
    argc -= eal_ret;
    argv += eal_ret;


    rte_mempool* mp = rte_pktmbuf_pool_create(
        "rx_mp", dpdk::MBUF_POOL_SIZE, dpdk::MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, static_cast<int>(rte_socket_id()));
    if (!mp) {
        throw std::runtime_error("mbuf pool creation failed");
    }

    // Use first available port (should be the TAP vdev)
    constexpr uint16_t port_id = 0;
    auto link = dpdk::port_init(port_id, mp);

    quill::info(logger, "[rcv] starting RX loop on port {}  (Ctrl-C to stop)\n", port_id);

    unsigned long long rx_total = 0;

    rte_mbuf* burst[dpdk::BURST_SIZE];

    while (!g_stop) {
        uint16_t n = rte_eth_rx_burst(port_id, 0, burst, dpdk::BURST_SIZE);

        // TODO
    }

    rte_eth_dev_stop(port_id);
    rte_eal_cleanup();
    return 0;
}
