#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <boost/asio/any_io_executor.hpp>

#include "one-xfs-sync/аctor/message.hpp"

class actor;
using actor_ref_t = std::shared_ptr<actor>;

class actor : public std::enable_shared_from_this<actor> 
{
    private:
        void process_next();

        boost::asio::any_io_executor executor_;

        std::queue<message> mailbox_;
        std::mutex mutex_;
        std::condition_variable cv_;
        bool running_ = false;

    protected:
        virtual void on_receive(const message& msg) = 0;
        virtual void on_start() {}
        virtual void on_stop() {}

    public:
        virtual ~actor();

        void send(message msg_);

        void start(boost::asio::any_io_executor e);
        void stop();
};
