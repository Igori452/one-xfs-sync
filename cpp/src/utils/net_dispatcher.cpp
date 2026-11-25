#include "one-xfs-sync/utils/net_dispatcher.hpp"

net_dispatcher::net_dispatcher(net_loop_t* net_loop) : net_loop_(net_loop) {}

bool net_dispatcher::send(int fd, std::span<const uint8_t> data)
{
    if (net_loop_ == nullptr || fd < 0 || data.empty())
    {
        return false;
    }

    auto it = out_queue_.find(fd);
    if (it != out_queue_.end() && !it->second.buffer.empty())
    {
        it->second.buffer.insert(it->second.buffer.end(), data.begin(), data.end());
        return true;
    }

    std::ptrdiff_t sent = net_loop_try_send(net_loop_, fd, data.data(), data.size());
    if (sent < 0)
    {
        return false;
    }

    if (static_cast<std::size_t>(sent) < data.size())
    {
        auto data_tail = data.subspan(static_cast<std::size_t>(sent));
        if (it != out_queue_.end())
        {
            it->second.buffer.assign(data_tail.begin(), data_tail.end());
            it->second.offset = 0;
        }
        else 
        {
            out_queue_.try_emplace(fd, std::vector<uint8_t>(data_tail.begin(), data_tail.end()));
        }
    }

    return true;
}

bool net_dispatcher::on_writable(int fd)
{
    auto it = out_queue_.find(fd);
    if (net_loop_ == nullptr || fd < 0 || it == out_queue_.end())
    {
        return false;
    }

    if (it->second.buffer.empty())
    {
        return true;
    }

    auto& buf = it->second;
    std::ptrdiff_t sent = net_loop_try_send(net_loop_, fd, buf.buffer.data() + buf.offset , buf.buffer.size() - buf.offset);
    if (sent < 0)
    {
        out_queue_.erase(it);
        return false;
    }

    buf.offset += static_cast<std::size_t>(sent);
    if (buf.offset == buf.buffer.size())
    {
        buf.buffer.clear();
        buf.offset = 0;
    }

    return true;
}

void net_dispatcher::clear_queue(int fd)
{
    auto it = out_queue_.find(fd);
    if (net_loop_ == nullptr || fd < 0 || it == out_queue_.end())
    {
        return;
    }

    out_queue_.erase(it);
}

bool net_dispatcher::has_pending(int fd) const
{
    auto it = out_queue_.find(fd);
    if (net_loop_ == nullptr || fd < 0 || it == out_queue_.end())
    {
        return false;
    }

    return !it->second.buffer.empty();
}