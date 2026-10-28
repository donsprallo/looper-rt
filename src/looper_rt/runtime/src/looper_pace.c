/*
 * The firmware's allowance of simulated time, when a client holds it.
 */
#include "looper_pace.h"

#include "looper_clock.h"

static bool held = false;
static uint64_t run_until = 0u;

void looper_pace_hold(void)
{
    held = true;
    run_until = looper_clock_microseconds();
}

bool looper_pace_is_held(void)
{
    return held;
}

void looper_pace_release(uint64_t microseconds)
{
    run_until = looper_clock_microseconds() + microseconds;
}

bool looper_pace_may_run(void)
{
    return !held || looper_clock_microseconds() < run_until;
}
