/*
 * The ring of pin level changes.
 *
 * The same shape as the analog trace, for the same reason: a client
 * polls a few times a second and the firmware changes a pin whenever
 * it likes, so what is kept has to outlive the gap between two polls.
 */
#include "looper_pin_trace.h"

#include "looper_clock.h"

static looper_pin_change_t changes[LOOPER_PIN_TRACE_CAPACITY];
static size_t change_count;
static size_t next_slot;
static uint32_t next_sequence;

static size_t slot_of(size_t index)
{
    if (change_count < LOOPER_PIN_TRACE_CAPACITY) {
        return index;
    }
    return (next_slot + index) % LOOPER_PIN_TRACE_CAPACITY;
}

void looper_pin_trace_clear(void)
{
    change_count = 0u;
    next_slot = 0u;
    next_sequence = 0u;
}

bool looper_pin_trace_record(
    uint32_t pin, looper_signal_level_t level, looper_signal_level_t previous
)
{
    looper_pin_change_t *change = &changes[next_slot];

    if (level == previous) {
        return false;
    }
    change->pin = pin;
    change->level = level;
    change->microseconds = looper_clock_microseconds();
    change->sequence = next_sequence;

    next_sequence++;
    next_slot = (next_slot + 1u) % LOOPER_PIN_TRACE_CAPACITY;
    if (change_count < LOOPER_PIN_TRACE_CAPACITY) {
        change_count++;
    }
    return true;
}

size_t looper_pin_trace_count(void)
{
    return change_count;
}

const looper_pin_change_t *looper_pin_trace_at(size_t index)
{
    if (index >= change_count) {
        return NULL;
    }
    return &changes[slot_of(index)];
}

uint32_t looper_pin_trace_next_sequence(void)
{
    return next_sequence;
}

size_t looper_pin_trace_after(uint32_t after, looper_pin_change_t *destination, size_t capacity)
{
    size_t copied = 0u;

    for (size_t index = 0u; index < change_count && copied < capacity; index++) {
        const looper_pin_change_t *change = &changes[slot_of(index)];
        if (change->sequence < after) {
            continue;
        }
        destination[copied] = *change;
        copied++;
    }
    return copied;
}
