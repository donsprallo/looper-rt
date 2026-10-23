/*
 * UART ports.
 *
 * A port keeps a ring of bytes waiting for the firmware. The firmware's
 * writes go straight to whatever is wired to the port; what comes back
 * arrives in the ring, whether a component put it there or a client did.
 */
#include "looper_uart.h"

#include <string.h>

#include "looper_bus.h"

#ifndef LOOPER_UART_PORT_CAPACITY
/** @brief Number of ports the model keeps a queue for. */
#define LOOPER_UART_PORT_CAPACITY 4u
#endif

/** @brief One port's queue of bytes waiting to be read. */
typedef struct {
    bool used;
    uint32_t bus;
    uint8_t port;
    uint8_t bytes[LOOPER_UART_QUEUE_CAPACITY];
    size_t count;
    size_t first;
} port_queue_t;

static port_queue_t queues[LOOPER_UART_PORT_CAPACITY];

static looper_bus_address_t address_of(uint8_t port)
{
    looper_bus_address_t address = {.value = port, .extended = false};

    return address;
}

/** @brief The queue of a port, creating it the first time it is used. */
static port_queue_t *queue_of(uint32_t bus, uint8_t port)
{
    port_queue_t *free_slot = NULL;

    for (size_t index = 0u; index < LOOPER_UART_PORT_CAPACITY; index++) {
        port_queue_t *queue = &queues[index];

        if (queue->used && (queue->bus == bus) && (queue->port == port)) {
            return queue;
        }
        if (!queue->used && (free_slot == NULL)) {
            free_slot = queue;
        }
    }
    if (free_slot != NULL) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->used = true;
        free_slot->bus = bus;
        free_slot->port = port;
    }
    return free_slot;
}

void looper_uart_clear(void)
{
    memset(queues, 0, sizeof(queues));
}

bool looper_uart_attach(
    uint32_t bus,
    uint8_t port,
    const char *label,
    const char *model,
    void *instance,
    const looper_uart_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
)
{
    if ((operations == NULL) || (operations->receive == NULL)) {
        return false;
    }
    if (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_UART) {
        return false;
    }
    return looper_component_attach(
        bus, address_of(port), label, model, instance, operations, run
    );
}

bool looper_uart_write(uint32_t bus, uint8_t port, const uint8_t *data, size_t length)
{
    const looper_bus_address_t address = address_of(port);
    const looper_component_t *component = looper_component_at_address(bus, address);
    const looper_uart_operations_t *operations;

    if (data == NULL) {
        return false;
    }
    if (component == NULL) {
        looper_bus_record(
            bus, LOOPER_BUS_DIRECTION_OUTBOUND, address, data, length, NULL, 0u, false
        );
        return false;
    }
    /*
     * A peer a client takes the part of is not called: it reads the
     * recording and answers by delivering bytes back. The write still
     * counts as taken, because something is listening.
     */
    if (component->operations != LOOPER_COMPONENT_REMOTE) {
        operations = component->operations;
        operations->receive(component->instance, port, data, length);
    }
    looper_bus_record(
        bus, LOOPER_BUS_DIRECTION_OUTBOUND, address, data, length, NULL, 0u, true
    );
    return true;
}

bool looper_uart_deliver(uint32_t bus, uint8_t port, const uint8_t *data, size_t length)
{
    port_queue_t *queue = queue_of(bus, port);

    if ((data == NULL) || (queue == NULL)) {
        return false;
    }
    if ((queue->count + length) > LOOPER_UART_QUEUE_CAPACITY) {
        return false;
    }
    for (size_t index = 0u; index < length; index++) {
        const size_t slot = (queue->first + queue->count) % LOOPER_UART_QUEUE_CAPACITY;

        queue->bytes[slot] = data[index];
        queue->count++;
    }
    return true;
}

size_t looper_uart_pending(uint32_t bus, uint8_t port)
{
    const port_queue_t *queue = queue_of(bus, port);

    return (queue == NULL) ? 0u : queue->count;
}

size_t looper_uart_read(uint32_t bus, uint8_t port, uint8_t *destination, size_t capacity)
{
    port_queue_t *queue = queue_of(bus, port);
    size_t taken = 0u;

    if ((destination == NULL) || (queue == NULL)) {
        return 0u;
    }
    while ((taken < capacity) && (queue->count > 0u)) {
        destination[taken] = queue->bytes[queue->first];
        queue->first = (queue->first + 1u) % LOOPER_UART_QUEUE_CAPACITY;
        queue->count--;
        taken++;
    }
    if (taken > 0u) {
        looper_bus_record(
            bus, LOOPER_BUS_DIRECTION_INBOUND, address_of(port), NULL, 0u, destination, taken,
            true
        );
    }
    return taken;
}
