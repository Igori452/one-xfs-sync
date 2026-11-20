#pragma once

#include "one-xfs-sync/actor/actor.hpp"

class actor_system {
    private:
        std::vector<actor_ref_t> actors_;
        bool is_shutdown_ = false;

    public:
        actor_system(const actor_system&) = delete;
        actor_system& operator=(const actor_system&) = delete; 

        template<typename actor_t, typename... args>
        actor_ref_t spawn(args&&... args_);

        void shutdown();
};

template<typename actor_t, typename... args>
actor_ref_t actor_system::spawn(args&&... args_)
{
    if (is_shutdown_) return nullptr;
    auto act = std::make_shared<actor_t>(std::forward<args>(args_)...);
    act->start(std::move(strand));
    actors_.push_back(act);
    return act;
}