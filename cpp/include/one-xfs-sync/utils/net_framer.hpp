#pragma once

#include "one-xfs-sync/utils/net_codec.hpp"

#include <queue>

class net_framer 
{
    private:
        std::queue<net_message> net_messages_;
        std::vector<uint8_t> buffer_in_;
    
    public:
        net_framer() = default;

        bool feed(std::span<const uint8_t> data);
        bool recv_from(int fd);
        std::optional<net_message> pop();

        bool has_message() const noexcept;
        void clear() noexcept;
};