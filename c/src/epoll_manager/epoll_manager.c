#include "one-xfs-sync/epoll_manager/epoll_manager.h"

#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#define MAX_EPOLL_EVENTS 128

struct net_loop 
{
    int ep_fd;
    int wakeup_fd;
    struct epoll_event events[MAX_EPOLL_EVENTS];
};

net_loop_t* net_loop_create(void)
{
    net_loop_t* net_loop = malloc(sizeof(net_loop_t));
    if (net_loop == NULL)
    {
        return NULL;
    }

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