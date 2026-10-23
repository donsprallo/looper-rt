/**
 * @file
 * @brief CAN buses, what is waiting on them, and the nodes listening.
 *
 * CAN differs from the other three in who is in charge. There is no master
 * clocking a device: every node may speak, and what reaches the firmware is
 * whatever its controller's acceptance filters let through.
 *
 * So the bus keeps a queue of frames waiting for the firmware, and the
 * device model that simulates the controller decides what happens to the
 * head of it: taken for a receive FIFO, or discarded as filtered. Filters
 * belong to the controller, not to the bus, and the controller is the one
 * thing here that is always project specific.
 *
 * A component on a CAN bus is a node rather than a slave. It sees every
 * frame the firmware sends and answers, when it has something to say, by
 * putting a frame back on the bus.
 */
#ifndef LOOPER_CAN_H
#define LOOPER_CAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifndef LOOPER_CAN_PENDING_CAPACITY
/** @brief Frames one bus holds for the firmware before the oldest is lost. */
#define LOOPER_CAN_PENDING_CAPACITY 32u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What a node on a CAN bus has to be able to do. */
typedef struct {
    /**
     * @brief Take a frame the firmware sent.
     *
     * A reply is given by calling looper_can_inject, not by returning one:
     * a node answers when it is ready, which is not necessarily inside
     * this call.
     */
    void (*receive)(void *instance, const looper_can_frame_t *frame);
} looper_can_operations_t;

/**
 * @brief Attach a node to a CAN bus.
 *
 * @param base_id Identifier the node is listed under, which is how a client
 *        tells two nodes on one bus apart. It does not filter anything: a
 *        node sees every frame, the way a node on a bus does.
 * @retval true The node is attached.
 */
bool looper_can_attach(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    const char *model,
    void *instance,
    const looper_can_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
);

/** @brief Forget what is waiting on every bus. */
void looper_can_clear(void);

/**
 * @brief Record a frame the firmware sent, and show it to every node.
 * @retval true The bus is known and the frame was recorded.
 */
bool looper_can_send(uint32_t bus, const looper_can_frame_t *frame);

/**
 * @brief Put a frame on a bus for the firmware to receive.
 *
 * The frame waits until the controller's model takes or discards it, so
 * nothing is recorded here: whether it reached the firmware is not yet
 * known.
 *
 * @retval true The frame is queued.
 * @retval false The bus is unknown or its queue is full.
 */
bool looper_can_inject(uint32_t bus, const looper_can_frame_t *frame);

/**
 * @brief The frame at the head of a bus's queue, without taking it.
 * @return The frame, or NULL when nothing is waiting.
 */
const looper_can_frame_t *looper_can_peek(uint32_t bus);

/**
 * @brief Take the head of the queue, recording it as received.
 * @param frame Destination for the frame, may be NULL.
 * @retval true A frame was taken.
 */
bool looper_can_take(uint32_t bus, looper_can_frame_t *frame);

/**
 * @brief Drop the head of the queue, recording that the filters rejected it.
 * @retval true A frame was dropped.
 */
bool looper_can_discard(uint32_t bus);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_CAN_H */
