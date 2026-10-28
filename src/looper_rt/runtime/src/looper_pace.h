/**
 * @file
 * @brief How far the firmware may run before it waits for a client.
 *
 * A simulation on a virtual clock that a tool controls runs in step with
 * that tool. It stands still until the tool lets time pass, runs until the
 * clock has reached the time it was given, and stands still again. That is
 * what makes a test exact and repeatable: one second asked for is one
 * second run, however fast or loaded the host is, and the firmware does not
 * race ahead while the tool reads what happened.
 *
 * A simulation that is not held -- on the host clock, or on a virtual clock
 * nobody controls, in a debugger for instance -- runs freely as before.
 *
 * Private to the runtime.
 */
#ifndef LOOPER_PACE_H
#define LOOPER_PACE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Hold the firmware still until time is let pass. */
void looper_pace_hold(void);

/** @brief Report whether the firmware runs in step with a client. */
bool looper_pace_is_held(void);

/**
 * @brief Let the firmware run until the clock is so much further on.
 * @param microseconds Simulated time to let pass, from the clock now.
 */
void looper_pace_release(uint64_t microseconds);

/**
 * @brief Report whether the firmware may take another pass of its loop.
 *
 * Always true when it is not held. When it is, true until the clock has
 * reached the time it was last let run to.
 */
bool looper_pace_may_run(void);

#endif /* LOOPER_PACE_H */
