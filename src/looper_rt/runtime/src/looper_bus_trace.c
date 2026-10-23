/*
 * The recording of what crossed the buses.
 *
 * One ring for all four kinds, read the way the analog trace is read: a
 * client remembers the sequence number it last saw and asks for what came
 * after, so a continuous view survives a request and response protocol.
 *
 * Only the first bytes of a long exchange are kept. A recording is there to
 * show what happened, not to be a second copy of the data, and a device
 * model that wants every byte of a bulk transfer has the bytes already.
 */
#include <string.h>

#include "looper_bus.h"
#include "looper_clock.h"

static looper_bus_record_t records[LOOPER_BUS_TRACE_CAPACITY];
static size_t record_count;
static size_t next_slot;
static uint32_t next_sequence;

static size_t slot_of(size_t index)
{
    if (record_count < LOOPER_BUS_TRACE_CAPACITY) {
        return index;
    }
    return (next_slot + index) % LOOPER_BUS_TRACE_CAPACITY;
}

void looper_bus_trace_clear(void)
{
    record_count = 0u;
    next_slot = 0u;
    next_sequence = 0u;
}

/** @brief Copy one half of an exchange, keeping what fits. */
static uint8_t copy_half(uint8_t *destination, const uint8_t *source, size_t length)
{
    size_t kept = length;

    if (source == NULL) {
        return 0u;
    }
    if (kept > LOOPER_BUS_PAYLOAD_CAPACITY) {
        kept = LOOPER_BUS_PAYLOAD_CAPACITY;
    }
    memcpy(destination, source, kept);
    return (uint8_t)kept;
}

void looper_bus_record(
    uint32_t bus,
    looper_bus_direction_t direction,
    looper_bus_address_t address,
    const uint8_t *payload,
    size_t payload_length,
    const uint8_t *response,
    size_t response_length,
    bool accepted
)
{
    looper_bus_record_t *record = &records[next_slot];

    memset(record, 0, sizeof(*record));
    record->bus = bus;
    record->kind = looper_bus_kind_of(bus);
    record->direction = direction;
    record->address = address;
    record->payload_length = copy_half(record->payload, payload, payload_length);
    record->response_length = copy_half(record->response, response, response_length);
    record->accepted = accepted;
    record->microseconds = looper_clock_microseconds();
    record->sequence = next_sequence;

    next_sequence++;
    next_slot = (next_slot + 1u) % LOOPER_BUS_TRACE_CAPACITY;
    if (record_count < LOOPER_BUS_TRACE_CAPACITY) {
        record_count++;
    }
}

size_t looper_bus_trace_count(void)
{
    return record_count;
}

const looper_bus_record_t *looper_bus_trace_at(size_t index)
{
    if (index >= record_count) {
        return NULL;
    }
    return &records[slot_of(index)];
}

uint32_t looper_bus_trace_next_sequence(void)
{
    return next_sequence;
}

size_t looper_bus_trace_after(
    uint32_t after, looper_bus_record_t *destination, size_t capacity
)
{
    size_t copied = 0u;

    for (size_t index = 0u; index < record_count && copied < capacity; index++) {
        const looper_bus_record_t *record = &records[slot_of(index)];

        if (record->sequence < after) {
            continue;
        }
        destination[copied] = *record;
        copied++;
    }
    return copied;
}
