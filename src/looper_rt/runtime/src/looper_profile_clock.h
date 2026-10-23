/**
 * @file
 * @brief The two clocks a profiler needs, and the bridge between them.
 *
 * One has to be cheap, because it is read twice per instrumented call.
 * The other has to be in a unit a person understands. No clock is both,
 * so both are read: the cheap one on every call, the meaningful one
 * twice per window, and the ratio of the two turns one into the other
 * afterwards.
 *
 * Private to the profiler.
 */
#ifndef LOOPER_PROFILE_CLOCK_H
#define LOOPER_PROFILE_CLOCK_H

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <x86intrin.h>
#define LOOPER_PROFILE_USES_TICKS 1
#else
#include <time.h>
#define LOOPER_PROFILE_USES_TICKS 0
#endif

#define LOOPER_NANOSECONDS_PER_SECOND 1000000000u

/**
 * @brief A cheap, monotonic counter with no defined unit.
 *
 * Inline on purpose: at two reads per call this is the hot path, and a
 * call into another unit to fetch a register would cost more than the
 * register does.
 */
__attribute__((no_instrument_function))
static inline uint64_t looper_profile_ticks(void)
{
#if LOOPER_PROFILE_USES_TICKS
    return (uint64_t)__rdtsc();
#else
    struct timespec now;

    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * LOOPER_NANOSECONDS_PER_SECOND + (uint64_t)now.tv_nsec;
#endif
}

/** @brief Begin a window: note where both clocks stand. */
void looper_profile_clock_restart(void);

/**
 * @brief How long one tick lasted over the window just measured.
 *
 * Worked out from the window itself rather than from a calibration at
 * start up, which costs no delay and stays right on a processor whose
 * counter does not run at its nominal rate. Fixed at the first reading
 * so that every measurement of one window is converted alike.
 */
double looper_profile_nanoseconds_per_tick(void);

#endif /* LOOPER_PROFILE_CLOCK_H */
