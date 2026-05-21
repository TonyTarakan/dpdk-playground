#!/usr/bin/env bash
#
#    RUN WITH SUDO
#
# 1. Allocate hugepages (DPDK requirement)
# 2. Load kernel modules
# 3. Setup interfaces between Generator and Analyzer
#

set -euo pipefail

echo "==> Allocating 1 GB hugepages (512 × 2 MB)..."
echo 512 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
grep -i huge /proc/meminfo | head -5

echo ""
echo "==> Loading kernel modules..."
modprobe uio_pci_generic  2>/dev/null || true
modprobe tap              2>/dev/null || true
modprobe veth             2>/dev/null || true

echo ""
echo "==> Creating a virtual network link"

ip link del veth0 2>/dev/null || true
ip link del veth1 2>/dev/null || true
ip link add veth0 type veth peer name veth1
ip link set veth0 up
ip link set veth1 up

# Disable checksums (DPDK virtual devices don't support HW offload)
ethtool -K veth0 tx off rx off 2>/dev/null || true
ethtool -K veth1 tx off rx off 2>/dev/null || true

echo ""
echo "==> Interfaces:"
ip link show veth0
ip link show veth1

echo ""
echo "Setup complete. Usage examples:"
echo ""
echo "Generator: sudo ./ecpri-generator --no-pci --vdev 'net_af_packet0,iface=veth0' --file-prefix=tx -- <other_args>"
echo ""
echo "Receiver:  sudo ./ecpri-receiver  --no-pci --vdev 'net_af_packet0,iface=veth1' --file-prefix=rx -- <other_args>"
echo ""

#
# TODO: check TAP and HUGEPAGES
#