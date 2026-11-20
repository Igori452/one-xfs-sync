#include "one-xfs-sync/аctor/actor_system.hpp"

actor_system::actor_system(std::size_t net_threads, std::size_t disk_threads) 
    : net_pool_(net_threads), disk_pool_(disk_threads) {}

actor_system::~actor_system() { shutdown(); }

void actor_system::shutdown() 
{
    if (is_shutdown_) return;
    is_shutdown_ = true;

    for (const auto& actor : actors_)
        actor->stop();

    net_pool_.join();
    disk_pool_.join();
}