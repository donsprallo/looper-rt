/*
 * CAN buses.
 *
 * The bus keeps what a client put on it and hands it over one frame at a
 * time; the controller's model decides whether the firmware is given it.
 * Everything that crosses is recorded in the shared bus trace, so a frame
 * sits beside an SPI transfer on one timeline.
 */
#include "looper_can.h"

#include <string.h>

#include "looper_bus.h"

#ifndef LOOPER_CAN_BUS_CAPACITY
/** @brief Number of CAN buses the model keeps a queue for. */
#define LOOPER_CAN_BUS_CAPACITY 4u
#endif

/** @brief One bus's queue of frames waiting for the firmware. */
typedef struct {
    bool used;
    uint32_t bus;
    looper_can_frame_t pending[LOOPER_CAN_PENDING_CAPACITY];
    size_t count;
    size_t first;
} bus_queue_t;

static bus_queue_t queues[LOOPER_CAN_BUS_CAPACITY];

static looper_bus_address_t address_of(const looper_can_frame_t *frame)
{
    looper_bus_address_t address = {.value = frame->id, .extended = frame->extended};

    return address;
}

/** @brief The queue of a bus, creating it the first time it is used. */
static bus_queue_t *queue_of(uint32_t bus)
{
    bus_queue_t *free_slot = NULL;

    for (size_t index = 0u; index < LOOPER_CAN_BUS_CAPACITY; index++) {
        bus_queue_t *queue = &queues[index];

        if (queue->used && (queue->bus == bus)) {
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
    }
    return free_slot;
}

void looper_can_clear(void)
{
    memset(queues, 0, sizeof(queues));
}

bool looper_can_attach(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    const char *model,
    void *instance,
    const looper_can_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
)
{
    const looper_bus_address_t address = {.value = base_id, .extended = false};

    if ((operations == NULL) || (operations->receive == NULL)) {
        return false;
    }
    if (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_CAN) {
        return false;
    }
    return looper_component_attach(bus, address, label, model, instance, operations, run);
}

/*
 * Show a frame the firmware sent to every node on that bus.
 *
 * A node a client takes the part of is skipped: it reads the recording
 * instead, and the frame is already in it.
 */
static void offer_to_nodes(uint32_t bus, const looper_can_frame_t *frame)
{
    for (size_t index = 0u; index < looper_component_count(); index++) {
        const looper_component_t *component = looper_component_at(index);
        const looper_can_operations_t *operations;

        if ((component->bus != bus) || (component->operations == LOOPER_COMPONENT_REMOTE)) {
            continue;
        }
        operations = component->operations;
        operations->receive(component->instance, frame);
    }
}

bool looper_can_send(uint32_t bus, const looper_can_frame_t *frame)
{
    if ((frame == NULL) || (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_CAN)) {
        return false;
    }
    looper_bus_record(
        bus,
        LOOPER_BUS_DIRECTION_OUTBOUND,
        address_of(frame),
        frame->payload,
        frame->length,
        NULL,
        0u,
        true
    );
    offer_to_nodes(bus, frame);
    return true;
}

bool looper_can_inject(uint32_t bus, const looper_can_frame_t *frame)
{
    bus_queue_t *queue;
    size_t slot;

    if ((frame == NULL) || (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_CAN)) {
        return false;
    }
    queue = queue_of(bus);
    if ((queue == NULL) || (queue->count >= LOOPER_CAN_PENDING_CAPACITY)) {
        return false;
    }
    slot = (queue->first + queue->count) % LOOPER_CAN_PENDING_CAPACITY;
    queue->pending[slot] = *frame;
    queue->count++;
    return true;
}

const looper_can_frame_t *looper_can_peek(uint32_t bus)
{
    const bus_queue_t *queue = queue_of(bus);

    if ((queue == NULL) || (queue->count == 0u)) {
        return NULL;
    }
    return &queue->pending[queue->first];
}

/** @brief Drop the head of the queue into @p frame, if there is one. */
static bool pop(uint32_t bus, looper_can_frame_t *frame)
{
    bus_queue_t *queue = queue_of(bus);

    if ((queue == NULL) || (queue->count == 0u)) {
        return false;
    }
    *frame = queue->pending[queue->first];
    queue->first = (queue->first + 1u) % LOOPER_CAN_PENDING_CAPACITY;
    queue->count--;
    return true;
}

/** @brief Record one inbound frame, delivered or turned away. */
static void record_inbound(uint32_t bus, const looper_can_frame_t *frame, bool delivered)
{
    looper_bus_record(
        bus,
        LOOPER_BUS_DIRECTION_INBOUND,
        address_of(frame),
        frame->payload,
        frame->length,
        NULL,
        0u,
        delivered
    );
}

bool looper_can_take(uint32_t bus, looper_can_frame_t *frame)
{
    looper_can_frame_t taken;

    if (!pop(bus, &taken)) {
        return false;
    }
    record_inbound(bus, &taken, true);
    if (frame != NULL) {
        *frame = taken;
    }
    return true;
}

bool looper_can_discard(uint32_t bus)
{
    looper_can_frame_t dropped;

    if (!pop(bus, &dropped)) {
        return false;
    }
    record_inbound(bus, &dropped, false);
    return true;
}
