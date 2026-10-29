/**
 * @file
 * @brief Stepping a state model on the simulated clock.
 *
 * Private to the signal graph, and the counterpart of the linear system: a
 * model steps at its own fixed rate, not at every read, and when it is read
 * it catches up to the moment of the read with its inputs held. Unlike a
 * linear system, all the steps of one catch-up are a single call into the
 * model's library.
 *
 * A step that produces NaN or infinity stops the model. It then holds its
 * last finite outputs, so that the firmware keeps reading numbers, and says
 * why it stopped until it is reset.
 */
#ifndef LOOPER_STATE_MODEL_H
#define LOOPER_STATE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "looper_model_library.h"
#include "looper_types.h"

/** @brief What one state model remembers between reads. */
typedef struct {
    looper_model_library_t library;
    double state[LOOPER_MODEL_STATES];
    double output[LOOPER_MODEL_PORTS];
    /** When the next step falls due, once the model has started. */
    uint64_t next_step;
    bool started;
    /** Why it stopped, or NULL while it runs. */
    const char *fault;
} looper_model_run_t;

/**
 * @brief Why a model cannot be installed, or NULL when it can.
 *
 * Too large, a step of zero, lists that do not match the sizes, or a model
 * this simulation has nothing to step with.
 */
const char *looper_state_model_fault(const looper_state_model_t *model);

/**
 * @brief Start a model: its state as installed, its outputs zero, not yet stepped.
 * @param model The model.
 * @param run What it remembers; its library is kept.
 */
void looper_state_model_start(const looper_state_model_t *model, looper_model_run_t *run);

/** @brief Start a model over from the state it is reset to. */
void looper_state_model_reset(const looper_state_model_t *model, looper_model_run_t *run);

/**
 * @brief Step a model up to a moment.
 * @param model The model.
 * @param run What it remembers; updated.
 * @param input One value per input, held for every step caught up.
 * @param microseconds The moment of the read.
 */
void looper_state_model_advance(
    const looper_state_model_t *model, looper_model_run_t *run, const double *input, uint64_t microseconds
);

#endif /* LOOPER_STATE_MODEL_H */
