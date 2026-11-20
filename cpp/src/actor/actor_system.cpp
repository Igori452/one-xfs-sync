#include "one-xfs-sync/actor/actor_system.hpp"

actor_system::~actor_system() { shutdown(); }

void actor_system::shutdown() 
{
    if (is_shutdown_) return;
    is_shutdown_ = true;

    for (const auto& actor : actors_)
        actor->stop();
}