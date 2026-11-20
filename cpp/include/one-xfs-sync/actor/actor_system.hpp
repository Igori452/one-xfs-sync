#pragma once

#include "one-xfs-sync/аctor/actor.hpp"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/strand.hpp>

enum class pool_t { NET_P, DISK_P };

class actor_system {
    private:
        std::vector<actor_ref_t> actors_;

        boost::asio::thread_pool net_pool_;
        boost::asio::thread_pool disk_pool_;

        bool is_shutdown_ = false;

    public:

        actor_system(std::size_t net_threads = 4, std::size_t disk_threads = 8);
        ~actor_system();

        actor_system(const actor_system&) = delete;
        actor_system& operator=(const actor_system&) = delete; 

        template<typename actor_t, typename... args>
        actor_ref_t spawn(pool_t type, args&&...);

        void shutdown();
};

template<typename actor_t, typename... args>
actor_ref_t actor_system::spawn(pool_t type, args&&... args_)
{
    if (is_shutdown_) return nullptr;

    auto act = std::make_shared<actor_t>(std::forward<args>(args_)...);

    auto& pool = (type == pool_t::DISK_P) ? disk_pool_ : net_pool_;
    auto strand = boost::asio::make_strand(pool.get_executor());

    act->start(std::move(strand));
    actors_.push_back(act);
    return act;
}