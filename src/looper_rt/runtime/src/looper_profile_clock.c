/*
 * Turning the processor's ticks into nanoseconds.
 *
 * The expensive clock is read exactly twice per window: once when the
 * window begins and once when its first measurement is read. Everything
 * else in the profiler reads the cheap one.
 */
#include "looper_profile_clock.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static uint64_t window_start_ticks;
static uint64_t window_start_nanoseconds;
static double tick_nanoseconds;

/** @brief The host's clock in nanoseconds, from an arbitrary origin. */
__attribute__((no_instrument_function))
static uint64_t host_nanoseconds(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)now.QuadPart * LOOPER_NANOSECONDS_PER_SECOND
           / (uint64_t)frequency.QuadPart;
#else
    return looper_profile_ticks();
#endif
}

void looper_profile_clock_restart(void)
{
    tick_nanoseconds = 0.0;
    window_start_nanoseconds = host_nanoseconds();
    window_start_ticks = looper_profile_ticks();
}

double looper_profile_nanoseconds_per_tick(void)
{
    uint64_t ticks;
    uint64_t nanoseconds;

    if (tick_nanoseconds > 0.0) {
        return tick_nanoseconds;
    }
    ticks = looper_profile_ticks() - window_start_ticks;
    nanoseconds = host_nanoseconds() - window_start_nanoseconds;
    if ((ticks == 0u) || (nanoseconds == 0u)) {
        return 1.0;
    }
    tick_nanoseconds = (double)nanoseconds / (double)ticks;
    return tick_nanoseconds;
}
