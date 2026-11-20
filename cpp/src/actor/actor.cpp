#include "one-xfs-sync/actor/actor.hpp"

void actor::run_loop() 
{
    message msg;
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] {
            return !mailbox_.empty() || !running_;
        });
        if (!running_ || mailbox_.empty()) return;
        msg = std::move(mailbox_.front());
        mailbox_.pop();
    }
    on_receive(msg);
}

actor::~actor() { stop(); }

void actor::send(message msg) 
{
    {
        std::lock_guard lock(mutex_);
        mailbox_.push(std::move(msg));
    }
    cv_.notify_one();
}

void actor::start() 
{
    std::size_t mailbox_size = 0;
    {
        std::lock_guard lock(mutex_);
        if (running_) return;
        running_ = true;
        thread_ = std::jthread([this] { run_loop(); });
    }

    on_start();
}

void actor::stop() 
{
    {
        std::lock_guard lock(mutex_);
        if (!running_) return;
        running_ = false;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    
    on_stop();
}