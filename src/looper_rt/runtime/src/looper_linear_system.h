/**
 * @file
 * @brief Stepping a discrete linear system on the simulated clock.
 *
 * Private to the signal graph. A system steps at its own fixed rate, not at
 * every read: a firmware that reads a hundred thousand times a second does
 * not make a filter at a millisecond step do a hundred times the work. When
 * it is read, it catches up to the moment of the read, holding its inputs
 * at what they are now, and answers with the outputs of its latest step.
 */
#ifndef LOOPER_LINEAR_SYSTEM_H
#define LOOPER_LINEAR_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "looper_types.h"

/** @brief What one linear system remembers between reads. */
typedef struct {
    double state[LOOPER_LINEAR_STATES];
    double output[LOOPER_LINEAR_PORTS];
    /** When the next step falls due, once the system has started. */
    uint64_t next_step;
    bool started;
} looper_linear_state_t;

/**
 * @brief Why a system cannot be stepped, or NULL when it can.
 *
 * Too large, a step of zero, or matrices whose counts do not match the size.
 */
const char *looper_linear_fault(const looper_linear_system_t *system);

/** @brief Start a system over: zero state, not yet started. */
void looper_linear_reset(looper_linear_state_t *state);

/**
 * @brief Step a system up to a moment and work out its outputs there.
 * @param system The system.
 * @param state What it remembers; updated.
 * @param input One value per input, held for every step caught up.
 * @param microseconds The moment of the read.
 */
void looper_linear_advance(
    const looper_linear_system_t *system,
    looper_linear_state_t *state,
    const double *input,
    uint64_t microseconds
);

#endif /* LOOPER_LINEAR_SYSTEM_H */
