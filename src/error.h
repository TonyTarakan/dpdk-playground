#pragma once

#include <expected>

static auto dpdk_error(std::string_view msg)
-> std::unexpected<std::string>
{
    return std::unexpected(
        std::format(
            "{}: {}",
            msg,
            rte_strerror(rte_errno)));
}