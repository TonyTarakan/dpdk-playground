#pragma once

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <stdexcept>
#include <string>
#include <cstdio>


namespace dpdk {

constexpr uint16_t RX_RING_SIZE = 1024;
constexpr uint16_t TX_RING_SIZE = 1024;
constexpr uint32_t MBUF_POOL_SIZE = 8191;   // must be 2^n - 1
constexpr uint32_t MBUF_CACHE_SIZE = 250;
constexpr uint16_t BURST_SIZE = 32;

// Ethernet header (14 bytes) placed before eCPRI frame on the wire
struct __attribute__((packed)) EthHeader {
    std::array<uint8_t,6> dst;
    std::array<uint8_t,6> src;
    uint16_t ether_type;
};
static_assert(sizeof(EthHeader) == 14);

// eCPRI Etherype (IEEE registration)
constexpr uint16_t ECPRI_ETHERTYPE = 0xAEFE;

// Init a single DPDK port with one RX + one TX queue
inline rte_eth_link port_init(uint16_t port_id, rte_mempool* mp) {
    rte_eth_conf cfg{};
    cfg.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;

    int ret = rte_eth_dev_configure(port_id, 1, 1, &cfg);
    if (ret < 0)
        throw std::runtime_error("rte_eth_dev_configure: " + std::to_string(ret));

    uint16_t rx_desc = RX_RING_SIZE;
    uint16_t tx_desc = TX_RING_SIZE;
    rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &rx_desc, &tx_desc);

    ret = rte_eth_rx_queue_setup(port_id, 0, rx_desc, rte_eth_dev_socket_id(port_id), nullptr, mp);
    if (ret < 0)
        throw std::runtime_error("rx_queue_setup: " + std::to_string(ret));

    ret = rte_eth_tx_queue_setup(port_id, 0, tx_desc, rte_eth_dev_socket_id(port_id), nullptr);
    if (ret < 0)
        throw std::runtime_error("tx_queue_setup: " + std::to_string(ret));

    ret = rte_eth_dev_start(port_id);
    if (ret < 0)
        throw std::runtime_error("rte_eth_dev_start: " + std::to_string(ret));

    rte_eth_promiscuous_enable(port_id);

    rte_eth_link link{};
    ret = rte_eth_link_get_nowait(port_id, &link);
    if (ret < 0)
        throw std::runtime_error("rte_eth_link_get_nowait: " + std::to_string(ret));

    return link;
}

} // namespace dpdk
