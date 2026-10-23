/*
 * The two halves of a bridge, and the buffer between them.
 *
 * Outward is easy: the firmware wrote something, send it. Inward is
 * where the work is, because a stream hands over whatever has arrived
 * -- half a frame, or three of them -- and a bus takes whole frames.
 * So a CAN bridge keeps a small buffer, decodes what it can out of it
 * each pass, and keeps the remainder for next time. A UART bridge keeps
 * nothing: a serial line has no frames to be half of.
 */
#include "looper_bridge.h"

#include <string.h>

#include "looper_can.h"
#include "looper_can_wire.h"
#include "looper_uart.h"

#ifndef LOOPER_BRIDGE_CAPACITY
/** @brief Bridges that may exist at once. */
#define LOOPER_BRIDGE_CAPACITY 8u
#endif

/** @brief Bytes taken from a transport in one pass. */
#define CHUNK 256u

/** @brief Room for what a stream has delivered but not yet completed. */
#define PENDING_CAPACITY (LOOPER_CAN_WIRE_CAPACITY * 4u)

/** @brief One bridge, between one bus endpoint and one transport. */
typedef struct {
    bool used;
    bool is_can;
    uint32_t bus;
    uint8_t port;
    void *transport;
    const looper_transport_t *operations;
    uint8_t pending[PENDING_CAPACITY];
    size_t pending_length;
} bridge_t;

static bridge_t bridges[LOOPER_BRIDGE_CAPACITY];

void looper_bridge_clear(void)
{
    memset(bridges, 0, sizeof(bridges));
}

/** @brief A free place in the table, or NULL when there is none. */
static bridge_t *free_bridge(void)
{
    for (size_t index = 0u; index < LOOPER_BRIDGE_CAPACITY; index++) {
        if (!bridges[index].used) {
            return &bridges[index];
        }
    }
    return NULL;
}

/** @brief The firmware wrote to the port: send it on. */
static void uart_written(void *instance, uint8_t port, const uint8_t *data, size_t length)
{
    bridge_t *self = instance;

    (void)port;
    (void)self->operations->send(self->transport, data, length);
}

/** @brief The firmware put a frame on the bus: send it on. */
static void can_sent(void *instance, const looper_can_frame_t *frame)
{
    bridge_t *self = instance;
    uint8_t encoded[LOOPER_CAN_WIRE_CAPACITY];
    const size_t length = looper_can_wire_encode(frame, encoded, sizeof(encoded));

    if (length > 0u) {
        (void)self->operations->send(self->transport, encoded, length);
    }
}

/** @brief Hand whole frames to the bus and keep what is left over. */
static void drain_frames(bridge_t *self)
{
    size_t consumed = 0u;

    for (;;) {
        looper_can_frame_t frame;
        const size_t taken = looper_can_wire_decode(
            &self->pending[consumed], self->pending_length - consumed, &frame
        );

        if (taken == 0u) {
            break;
        }
        consumed += taken;
        (void)looper_can_inject(self->bus, &frame);
    }
    self->pending_length -= consumed;
    memmove(self->pending, &self->pending[consumed], self->pending_length);
}

/*
 * Take what has arrived and give it to the bus.
 *
 * A CAN stream that has fallen out of step would otherwise sit in the
 * buffer forever, so a buffer holding more than one frame's worth
 * without a frame coming out of it is thrown away. Losing a few bytes
 * of nonsense beats never reading the socket again.
 */
static void receive_can(bridge_t *self)
{
    const size_t room = PENDING_CAPACITY - self->pending_length;
    const size_t taken = (room == 0u)
                             ? 0u
                             : self->operations->receive(
                                   self->transport, &self->pending[self->pending_length], room
                               );

    self->pending_length += taken;
    drain_frames(self);
    if (self->pending_length >= LOOPER_CAN_WIRE_CAPACITY) {
        self->pending_length = 0u;
    }
}

/** @brief Take what has arrived and deliver it to the port. */
static void receive_uart(bridge_t *self)
{
    uint8_t arrived[CHUNK];
    const size_t taken = self->operations->receive(self->transport, arrived, sizeof(arrived));

    if (taken > 0u) {
        (void)looper_uart_deliver(self->bus, self->port, arrived, taken);
    }
}

/** @brief Once per pass of the firmware loop, whichever kind this is. */
static void run(void *instance, uint64_t microseconds)
{
    bridge_t *self = instance;

    (void)microseconds;
    if (self->is_can) {
        receive_can(self);
        return;
    }
    receive_uart(self);
}

static const looper_uart_operations_t uart_operations = {.receive = uart_written};
static const looper_can_operations_t can_operations = {.receive = can_sent};

/** @brief Fill in what both kinds of bridge share. */
static bridge_t *prepared(
    uint32_t bus, void *transport, const looper_transport_t *operations, bool is_can
)
{
    bridge_t *self = free_bridge();

    if ((self == NULL) || (operations == NULL) || (operations->send == NULL)
        || (operations->receive == NULL)) {
        return NULL;
    }
    memset(self, 0, sizeof(*self));
    self->used = true;
    self->is_can = is_can;
    self->bus = bus;
    self->transport = transport;
    self->operations = operations;
    return self;
}

bool looper_bridge_uart(
    uint32_t bus,
    uint8_t port,
    const char *label,
    const char *model,
    void *transport,
    const looper_transport_t *operations
)
{
    bridge_t *self = prepared(bus, transport, operations, false);

    if (self == NULL) {
        return false;
    }
    self->port = port;
    if (looper_uart_attach(bus, port, label, model, self, &uart_operations, run)) {
        return true;
    }
    self->used = false;
    return false;
}

bool looper_bridge_can(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    const char *model,
    void *transport,
    const looper_transport_t *operations
)
{
    bridge_t *self = prepared(bus, transport, operations, true);

    if (self == NULL) {
        return false;
    }
    if (looper_can_attach(bus, base_id, label, model, self, &can_operations, run)) {
        return true;
    }
    self->used = false;
    return false;
}
