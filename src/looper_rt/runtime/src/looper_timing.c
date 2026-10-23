/*
 * How long the adapter calls took.
 *
 * A fixed table keyed by the name pointer. The generated adapters pass a
 * string literal, so one function is always the same pointer and the
 * lookup is a pointer comparison rather than a string one -- which matters
 * when the thing being measured is called eight thousand times a second
 * and the measurement must not dominate what it measures.
 */
#include "looper_timing.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define NANOSECONDS_PER_SECOND 1000000000u

static looper_timing_t timings[LOOPER_TIMING_CAPACITY];
static size_t timing_count;

/** @brief The host's clock, in nanoseconds, from an arbitrary origin. */
static uint64_t host_nanoseconds(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)now.QuadPart * NANOSECONDS_PER_SECOND / (uint64_t)frequency.QuadPart;
#else
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * NANOSECONDS_PER_SECOND + (uint64_t)now.tv_nsec;
#endif
}

void looper_timing_clear(void)
{
    timing_count = 0u;
    memset(timings, 0, sizeof(timings));
}

/** @brief The entry of a function, created the first time it is seen. */
static looper_timing_t *entry_for(const char *function)
{
    for (size_t index = 0u; index < timing_count; index++) {
        if (timings[index].function == function) {
            return &timings[index];
        }
    }
    if (timing_count >= LOOPER_TIMING_CAPACITY) {
        return NULL;
    }
    timings[timing_count].function = function;
    timing_count++;
    return &timings[timing_count - 1u];
}

/**
 * @brief The band a duration belongs in.
 *
 * The position of the highest set bit, which is what makes the bands
 * powers of two: no division, no table, and the same answer the
 * client computes when it draws them.
 */
static size_t band_of(uint64_t nanoseconds)
{
    size_t band = 0u;

    while ((nanoseconds > 1u) && (band < (LOOPER_TIMING_BANDS - 1u))) {
        nanoseconds >>= 1u;
        band++;
    }
    return band;
}

uint64_t looper_timing_enter(const char *function)
{
    (void)function;

    return host_nanoseconds();
}

void looper_timing_leave(const char *function, uint64_t started, bool answered)
{
    const uint64_t finished = host_nanoseconds();
    looper_timing_t *entry = entry_for(function);
    uint64_t elapsed;

    if (entry == NULL) {
        return;
    }
    elapsed = (finished > started) ? (finished - started) : 0u;
    entry->calls++;
    if (answered) {
        entry->answered_calls++;
    }
    entry->total_nanoseconds += elapsed;
    if ((entry->calls == 1u) || (elapsed < entry->shortest_nanoseconds)) {
        entry->shortest_nanoseconds = elapsed;
    }
    if (elapsed > entry->longest_nanoseconds) {
        entry->longest_nanoseconds = elapsed;
    }
    entry->bands[band_of(elapsed)]++;
}

size_t looper_timing_count(void)
{
    return timing_count;
}

const looper_timing_t *looper_timing_at(size_t index)
{
    if (index >= timing_count) {
        return NULL;
    }
    return &timings[index];
}

/*
 * By name rather than by pointer.
 *
 * A caller outside the generated adapters has a name, not the literal the
 * adapter passed, so this one compares content. It is for a test asking
 * about one function, not for the hot path.
 */
const looper_timing_t *looper_timing_of(const char *function)
{
    if (function == NULL) {
        return NULL;
    }
    for (size_t index = 0u; index < timing_count; index++) {
        if (strcmp(timings[index].function, function) == 0) {
            return &timings[index];
        }
    }
    return NULL;
}
