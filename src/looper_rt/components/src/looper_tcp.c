/*
 * A stream socket as a transport.
 *
 * Everything here is non blocking, including the accept, because all of
 * it runs on the thread the firmware runs on. A bridge with nobody
 * connected is not an error and not a warning: it is a wire with
 * nothing plugged into it, which is what most wires are most of the
 * time.
 */
#include "looper_tcp.h"

#include <string.h>

#include "looper_bridge.h"
#include "looper_socket.h"

#ifndef LOOPER_TCP_CAPACITY
/** @brief TCP bridges that may exist at once. */
#define LOOPER_TCP_CAPACITY 4u
#endif

#define MODEL_NAME "tcp"

/** @brief One listening port and the one connection it holds. */
typedef struct {
    bool used;
    looper_socket_t listening;
    looper_socket_t client;
} tcp_transport_t;

static tcp_transport_t transports[LOOPER_TCP_CAPACITY];

/*
 * Take a new connection if one is waiting.
 *
 * A new one replaces whatever was there. A terminal closed and opened
 * again is the ordinary case, and a bridge held by a connection that is
 * gone would be a bridge nobody can use until the simulation restarts.
 */
static void accept_connection(tcp_transport_t *self)
{
    const looper_socket_t accepted = looper_socket_accept(self->listening);

    if (!looper_socket_is_open(accepted)) {
        return;
    }
    looper_socket_close(&self->client);
    self->client = accepted;
}

static bool send_out(void *transport, const uint8_t *data, size_t length)
{
    tcp_transport_t *self = transport;
    size_t sent = 0u;

    if (!looper_socket_is_open(self->client)) {
        return false;
    }
    while (sent < length) {
        const long chunk = looper_socket_send(self->client, &data[sent], length - sent);

        if (chunk < 0) {
            looper_socket_close(&self->client);
            return false;
        }
        if (chunk == 0) {
            /* The far side is not reading. Dropping what will not fit is
             * what a wire does too. */
            return false;
        }
        sent += (size_t)chunk;
    }
    return true;
}

static size_t receive_in(void *transport, uint8_t *destination, size_t capacity)
{
    tcp_transport_t *self = transport;
    long taken;

    accept_connection(self);
    if (!looper_socket_is_open(self->client)) {
        return 0u;
    }
    taken = looper_socket_receive(self->client, destination, capacity);
    if (taken < 0) {
        looper_socket_close(&self->client);
        return 0u;
    }
    return (size_t)taken;
}

static const looper_transport_t operations = {.send = send_out, .receive = receive_in};

/** @brief A transport listening on a port, or NULL when it cannot. */
static tcp_transport_t *listening_on(uint16_t listen_port)
{
    for (size_t index = 0u; index < LOOPER_TCP_CAPACITY; index++) {
        tcp_transport_t *self = &transports[index];

        if (self->used) {
            continue;
        }
        memset(self, 0, sizeof(*self));
        self->client = LOOPER_SOCKET_NONE;
        self->listening = looper_socket_listen(listen_port, 1);
        if (!looper_socket_is_open(self->listening)) {
            return NULL;
        }
        self->used = true;
        return self;
    }
    return NULL;
}

/** @brief Give a transport up when the bus would not have it. */
static bool released(tcp_transport_t *self)
{
    looper_socket_close(&self->listening);
    self->used = false;
    return false;
}

bool looper_tcp_uart(uint32_t bus, uint8_t port, const char *label, uint16_t listen_port)
{
    tcp_transport_t *self = listening_on(listen_port);

    if (self == NULL) {
        return false;
    }
    if (looper_bridge_uart(bus, port, label, MODEL_NAME, self, &operations)) {
        return true;
    }
    return released(self);
}

bool looper_tcp_can(uint32_t bus, uint32_t base_id, const char *label, uint16_t listen_port)
{
    tcp_transport_t *self = listening_on(listen_port);

    if (self == NULL) {
        return false;
    }
    if (looper_bridge_can(bus, base_id, label, MODEL_NAME, self, &operations)) {
        return true;
    }
    return released(self);
}
