#include "one-xfs-sync/core/connection_core.hpp"

client_session::client_session(std::size_t connection_pool_size)
{
    connection_pool.reserve(connection_pool_size);
}

void client_session::add_new_connection(int connection_fd) 
{
    connection_pool.emplace_back(connection_fd);
}

void connection_core::handle_new_connection(const message_new_connection& msg)
{
    sessions_.try_emplace(next_session_id_++, std::make_unique<client_session>(msg.connection_pool_size));
}

void connection_core::up_fd(std::size_t client_port, std::size_t connections_port) 
{
    /**
     * TODO:
     * Вызов сисколов которые создают необходимые сокеты.
     */
}

void connection_core::down_fd()
{
    /**
     * TODO:
     * Закрытие ранее выделенных сокетов.
     */
}

connection_core::~connection_core() { down_fd(); }