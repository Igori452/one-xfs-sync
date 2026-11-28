#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <span>

enum class net_message_t : uint8_t
{
    ALLOC_THREADS_REQ   = 1,
    ALLOC_THREADS_RESP  = 2,
    OK                  = 3,
    STOP_WORK           = 4
};

struct net_message 
{   
    net_message_t net_message_t_;
    uint64_t session_id_;

    std::optional<uint16_t> thread_count_;
};

namespace net_codec 
{
    std::optional<std::size_t> expected_frame_size(const uint8_t msg_header);
    std::vector<uint8_t> encode(const net_message& msg);
    std::optional<net_message> decode(std::span<const uint8_t> data);
}