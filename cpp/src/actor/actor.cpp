#include "one-xfs-sync/аctor/actor.hpp"

#include <boost/asio/post.hpp>

void actor::process_next() 
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

    boost::asio::post(executor_, [self = shared_from_this()](){
        self->process_next();
    });
}

void actor::start(boost::asio::any_io_executor e) 
{
    std::size_t mailbox_size = 0;
    {
        std::lock_guard lock(mutex_);
        if (running_) return;
        executor_ = std::move(e);
        running_ = true;
        mailbox_size = mailbox_.size();
    }

    for (std::size_t i = 0; i < mailbox_size; ++i) 
    {
        boost::asio::post(executor_, [self = shared_from_this()](){
            self->process_next();
        });
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

    on_stop();
}