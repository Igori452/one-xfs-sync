#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct net_loop net_loop_t;

typedef enum 
{
    NET_EVENT_WAKEUP,
    NET_EVENT_READ,
    NET_EVENT_ERROR
} net_event_type_t;

typedef struct
{
    net_event_type_t event_type;
    int fd;
} net_event_t;

net_loop_t* net_loop_create(void);
void net_loop_destroy(net_loop_t* net_loop);

void net_loop_wakeup(net_loop_t* net_loop);

int net_loop_create_listen_socket(net_loop_t* net_loop, uint16_t port);
int net_loop_accept(net_loop_t* net_loop, int listen_fd);

int net_loop_send(net_loop_t* net_loop, int fd, const void* data, size_t len);

bool net_loop_add_fd(net_loop_t* net_loop, int fd);
bool net_loop_remove_fd(net_loop_t* net_loop, int fd);

int net_loop_wait(net_loop_t* net_loop, net_event_t* out_events, size_t max_events, int timeout);