#include "looper_trace.h"

#include "looper_clock.h"

static looper_sample_t samples[LOOPER_TRACE_CAPACITY];
static size_t sample_count;
static size_t next_slot;
static uint32_t next_sequence;

static size_t slot_of(size_t index)
{
    if (sample_count < LOOPER_TRACE_CAPACITY) {
        return index;
    }
    return (next_slot + index) % LOOPER_TRACE_CAPACITY;
}

void looper_trace_clear(void)
{
    sample_count = 0u;
    next_slot = 0u;
    next_sequence = 0u;
}

void looper_trace_record(uint32_t channel, double value)
{
    looper_sample_t *sample = &samples[next_slot];

    sample->channel = channel;
    sample->microseconds = looper_clock_microseconds();
    sample->value = value;
    sample->sequence = next_sequence;

    next_sequence++;
    next_slot = (next_slot + 1u) % LOOPER_TRACE_CAPACITY;
    if (sample_count < LOOPER_TRACE_CAPACITY) {
        sample_count++;
    }
}

size_t looper_trace_count(void)
{
    return sample_count;
}

const looper_sample_t *looper_trace_at(size_t index)
{
    if (index >= sample_count) {
        return NULL;
    }
    return &samples[slot_of(index)];
}

uint32_t looper_trace_next_sequence(void)
{
    return next_sequence;
}

size_t looper_trace_after(uint32_t after, looper_sample_t *destination, size_t capacity)
{
    size_t copied = 0u;

    for (size_t index = 0u; index < sample_count && copied < capacity; index++) {
        const looper_sample_t *sample = &samples[slot_of(index)];
        if (sample->sequence < after) {
            continue;
        }
        destination[copied] = *sample;
        copied++;
    }
    return copied;
}
