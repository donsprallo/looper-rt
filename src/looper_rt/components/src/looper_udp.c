/*
 * A datagram socket as a transport.
 *
 * One datagram per send, which for CAN means one frame per datagram and
 * no stream to fall out of step with. The bridge above still runs its
 * decoder over what arrives, which costs nothing and means a sender
 * that puts two frames in one datagram is understood as well.
 *
 * Where to send is either fixed at attach time or learnt from whoever
 * wrote to us last. The second is what makes `nc -u` work without
 * configuring anything.
 */
#include "looper_udp.h"

#include <string.h>

#include "looper_bridge.h"
#include "looper_socket.h"

#ifndef LOOPER_UDP_CAPACITY
/** @brief UDP bridges that may exist at once. */
#define LOOPER_UDP_CAPACITY 4u
#endif

#define MODEL_NAME "udp"

/** @brief One bound port and the peer it talks to. */
typedef struct {
    bool used;
    looper_socket_t socket;
    looper_socket_peer_t peer;
    bool peer_is_fixed;
    bool peer_is_known;
} udp_transport_t;

static udp_transport_t transports[LOOPER_UDP_CAPACITY];

static bool send_out(void *transport, const uint8_t *data, size_t length)
{
    udp_transport_t *self = transport;

    if (!self->peer_is_known) {
        return false;
    }
    return looper_socket_send_to(self->socket, &self->peer, data, length) >= 0;
}

static size_t receive_in(void *transport, uint8_t *destination, size_t capacity)
{
    udp_transport_t *self = transport;
    looper_socket_peer_t from;
    const long taken = looper_socket_receive_from(self->socket, destination, capacity, &from);

    if (taken <= 0) {
        return 0u;
    }
    if (!self->peer_is_fixed) {
        self->peer = from;
        self->peer_is_known = true;
    }
    return (size_t)taken;
}

static const looper_transport_t operations = {.send = send_out, .receive = receive_in};

/** @brief Work out where to send, from what the caller asked for. */
static bool resolve_peer(udp_transport_t *self, const char *peer_host, uint16_t peer_port)
{
    uint32_t address = 0u;

    if ((peer_host == NULL) || (peer_host[0] == '\0')) {
        return true;
    }
    if (!looper_socket_parse_address(peer_host, &address)) {
        return false;
    }
    self->peer.address = address;
    self->peer.port = peer_port;
    self->peer_is_fixed = true;
    self->peer_is_known = true;
    return true;
}

/** @brief A transport bound to a port, or NULL when it cannot be had. */
static udp_transport_t *bound_to(
    uint16_t listen_port, const char *peer_host, uint16_t peer_port
)
{
    for (size_t index = 0u; index < LOOPER_UDP_CAPACITY; index++) {
        udp_transport_t *self = &transports[index];

        if (self->used) {
            continue;
        }
        memset(self, 0, sizeof(*self));
        if (!resolve_peer(self, peer_host, peer_port)) {
            return NULL;
        }
        self->socket = looper_socket_bind_datagram(listen_port);
        if (!looper_socket_is_open(self->socket)) {
            return NULL;
        }
        self->used = true;
        return self;
    }
    return NULL;
}

/** @brief Give a transport up when the bus would not have it. */
static bool released(udp_transport_t *self)
{
    looper_socket_close(&self->socket);
    self->used = false;
    return false;
}

bool looper_udp_uart(
    uint32_t bus,
    uint8_t port,
    const char *label,
    uint16_t listen_port,
    const char *peer_host,
    uint16_t peer_port
)
{
    udp_transport_t *self = bound_to(listen_port, peer_host, peer_port);

    if (self == NULL) {
        return false;
    }
    if (looper_bridge_uart(bus, port, label, MODEL_NAME, self, &operations)) {
        return true;
    }
    return released(self);
}

bool looper_udp_can(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    uint16_t listen_port,
    const char *peer_host,
    uint16_t peer_port
)
{
    udp_transport_t *self = bound_to(listen_port, peer_host, peer_port);

    if (self == NULL) {
        return false;
    }
    if (looper_bridge_can(bus, base_id, label, MODEL_NAME, self, &operations)) {
        return true;
    }
    return released(self);
}
