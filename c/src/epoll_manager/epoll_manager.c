#include "one-xfs-sync/epoll_manager/epoll_manager.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_EPOLL_EVENTS 128

struct clients_data 
{
    void* data;
    size_t len_data;
    size_t cap_data;
    size_t sent_data;
};

struct net_loop 
{
    int ep_fd;
    int wakeup_fd;
    struct epoll_event events[MAX_EPOLL_EVENTS];
    struct clients_data* cl_d;
    size_t size_cl_d;
};

static bool append_client_data(net_loop_t* net_loop, int client_fd, const void* data, size_t len_data)
{
    if (client_fd < 0)
    {
        return false;
    }

    if (net_loop->cl_d == NULL)
    {
        net_loop->size_cl_d = 64;
        net_loop->cl_d = calloc(net_loop->size_cl_d, sizeof(struct clients_data));
    }

    /**
     * TODO:
     * Реализовать добавление данных путем удвоения исходного массива data
     */

}

static bool remove_client_data(net_loop_t* net_loop, int client_fd, size_t remove_bytes)
{
    /**
     * TODO:
     * Реализовать удаление оптравлденных байт
     */
}

net_loop_t* net_loop_create(void)
{
    net_loop_t* net_loop = malloc(sizeof(net_loop_t));
    if (net_loop == NULL)
    {
        return NULL;
    }
    net_loop->cl_d = NULL;
    net_loop->size_cl_d = 0;

    net_loop->ep_fd = epoll_create1(EPOLL_CLOEXEC);
    if (net_loop->ep_fd == -1)
    {
        free(net_loop);
        return NULL;
    }

    net_loop->wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(net_loop->wakeup_fd == -1)
    {
        close(net_loop->ep_fd);
        free(net_loop);
        return NULL;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = net_loop->wakeup_fd;

    if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_ADD, net_loop->wakeup_fd, &ev) == -1)
    {
        close(net_loop->wakeup_fd);   
        close(net_loop->ep_fd);
        free(net_loop);        
        return NULL;
    }

    return net_loop;
}

void net_loop_destroy(net_loop_t* net_loop)
{
    if (net_loop == NULL)
    {
        return;
    }

    close(net_loop->wakeup_fd);   
    close(net_loop->ep_fd);
    free(net_loop);   
}

void net_loop_wakeup(net_loop_t* net_loop)
{
    if (net_loop == NULL)
    {
        return;
    }

    uint64_t val = 1;
    ssize_t bytes = write(net_loop->wakeup_fd, &val, sizeof(val));
    (void)bytes;
}

int net_loop_create_listen_socket(net_loop_t* net_loop, uint16_t port)
{
    if (net_loop == NULL)
    {
        return -1;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (socket_fd < 0)
    {
        return -1;
    }

    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(socket_fd);
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(socket_fd, SOMAXCONN) < 0)
    {
        close(socket_fd);
        return -1;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = socket_fd;

    if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_ADD, socket_fd, &ev) < 0)
    {
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int net_loop_accept(net_loop_t* net_loop, int listen_fd) 
{
    if (net_loop == NULL || listen_fd < 0)
    {
        return -1;
    }

    int client_fd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0)
    {
        return -1;
    }

    struct epoll_event ev = {0};

    if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0)
    {
        close(client_fd);
        return -1;
    }

    return client_fd;
}

int net_loop_send(net_loop_t* net_loop, int fd, const void* data, size_t len)
{
    int n = send(fd, data, len, MSG_NOSIGNAL);
    if (n <= 0)
    {
        return -1;
    }

    /**
     * TODO:
     * Сохранять все данные которые не были отправлены и доотправить их в net_loop_wait
     */

    if (n != len)
    {
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLIN;
        ev.data.fd = fd;

        if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
        {
            return -1;
        }
    }

    return n;
}

bool net_loop_add_fd(net_loop_t* net_loop, int fd)
{
    if (net_loop == NULL || fd < 0)
    {
        return false;
    }

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = fd;

    if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        return false;
    }

    return true;
}

bool net_loop_remove_fd(net_loop_t* net_loop, int fd)
{
    if (net_loop == NULL || fd < 0)
    {
        return false;
    }

    if (epoll_ctl(net_loop->ep_fd, EPOLL_CTL_DEL, fd, NULL) == -1)
    {
        return false;
    }

    return true;
}

int net_loop_wait(net_loop_t* net_loop, net_event_t* out_events, size_t max_events, int timeout)
{
    if (net_loop == NULL || out_events == NULL || max_events == 0)
    {
        return -1;
    }

    int events_limit = max_events > MAX_EPOLL_EVENTS ? MAX_EPOLL_EVENTS : max_events;

    int n = epoll_wait(net_loop->ep_fd, net_loop->events, events_limit, timeout);
    if (n <= 0)
    {
        return n;
    }

    for (size_t i = 0; i < n; ++i)
    {
        int fd = net_loop->events[i].data.fd;
        uint32_t ev_flags = net_loop->events[i].events;

        if (fd == net_loop->wakeup_fd)
        {
            uint64_t val = 0;
            ssize_t bytes = read(net_loop->wakeup_fd, &val, sizeof(val));
            (void)bytes;

            out_events[i].fd = -1;
            out_events[i].event_type = NET_EVENT_WAKEUP;
        }        
        else if (ev_flags & (EPOLLERR | EPOLLHUP))
        {
            out_events[i].fd = fd;
            out_events[i].event_type = NET_EVENT_ERROR;
        }
        else
        {
            out_events[i].fd = fd;
            out_events[i].event_type = NET_EVENT_READ;
        }
    }

    return n;
}