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




int main(int argc, char* argv[]) {

    auto* logger = quill::simple_logger();

    quill::info(logger, "[rcv] \n");

    return 0;
}
