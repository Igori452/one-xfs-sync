#pragma once

#include "one-xfs-sync/actor/message.hpp"

#include <map>
#include <vector>
#include <memory>

class client_session 
{
    private:
        std::vector<int> connection_pool;

    public:
        client_session() = delete;
        client_session(std::size_t connection_pool_size);
        ~client_session();

        void add_new_connection(int connection_fd);

        bool is_ready() const;
};

class connection_core
{
    private:
        int listen_client_fd_ {-1};
        int listen_connections_fd_ {-1};
        bool is_listen = false;

        std::map<std::size_t, std::unique_ptr<client_session>> sessions_;
        std::size_t next_session_id_ {1};

    public:
        ~connection_core();

        void up_fd(uint16_t client_port, uint16_t connections_port);
        void down_fd();

        void handle_new_connection(const message_new_connection& msg);
        void handle_client_request(const message_client_request& msg);
        /**
         * TODO:
         * Другие хендлеры по мере необходимости.
         */
};