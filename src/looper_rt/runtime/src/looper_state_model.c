/*
 * A state model on the simulated clock: catch up at a read, one call per catch-up.
 */
#include "looper_state_model.h"

#include <math.h>
#include <string.h>

/*
 * The most steps one read catches up, as for a linear system: a model left
 * unread through a long pause jumps to the present instead of stalling the
 * firmware while it works off the backlog.
 */
#define MOST_STEPS_PER_READ 100000u

#define MICROSECONDS_PER_SECOND 1e6

static const char *const NOT_FINITE =
    "the model's step produced NaN or infinity; it holds its last finite outputs";

const char *looper_state_model_fault(const looper_state_model_t *model)
{
    if (model->states > LOOPER_MODEL_STATES || model->inputs > LOOPER_MODEL_PORTS
        || model->outputs > LOOPER_MODEL_PORTS || model->constant_count > LOOPER_MODEL_CONSTANTS) {
        return "a state model larger than the simulation has room for";
    }
    if (model->step_microseconds == 0u || model->outputs == 0u) {
        return "a state model without a step or without an output";
    }
    if (model->state_count != model->states
        || (model->initial_state_count != 0u && model->initial_state_count != model->states)) {
        return "a state model whose state does not match its size";
    }
    if (model->provider == LOOPER_MODEL_PROVIDER_INTERPRETED) {
        return "an interpreted state model, which needs the model worker this simulation does not have yet";
    }
    if (model->provider != LOOPER_MODEL_PROVIDER_COMPILED) {
        return "a state model that says neither where its library is nor that it is interpreted";
    }
    return NULL;
}

void looper_state_model_start(const looper_state_model_t *model, looper_model_run_t *run)
{
    memcpy(run->state, model->state, model->states * sizeof(double));
    memset(run->output, 0, sizeof(run->output));
    run->started = false;
    run->next_step = 0u;
    run->fault = NULL;
}

void looper_state_model_reset(const looper_state_model_t *model, looper_model_run_t *run)
{
    looper_state_model_start(model, run);
    if (model->initial_state_count != 0u) {
        memcpy(run->state, model->initial_state, model->states * sizeof(double));
    }
}

/* How many steps fall due up to a moment, and the clock moved past them. */
static uint32_t due_steps(const looper_state_model_t *model, looper_model_run_t *run, uint64_t microseconds)
{
    if (!run->started) {
        run->started = true;
        run->next_step = microseconds;
    }
    if (run->next_step > microseconds) {
        return 0u;
    }
    uint64_t due = (microseconds - run->next_step) / model->step_microseconds + 1u;
    if (due > MOST_STEPS_PER_READ) {
        run->next_step = microseconds + model->step_microseconds;
        return MOST_STEPS_PER_READ;
    }
    run->next_step += due * model->step_microseconds;
    return (uint32_t)due;
}

static bool all_finite(const double *values, uint32_t count)
{
    for (uint32_t index = 0u; index < count; index++) {
        if (!isfinite(values[index])) {
            return false;
        }
    }
    return true;
}

void looper_state_model_advance(
    const looper_state_model_t *model, looper_model_run_t *run, const double *input, uint64_t microseconds
)
{
    double held[LOOPER_MODEL_PORTS];

    if (run->fault != NULL || run->library.description == NULL) {
        return;
    }
    uint32_t steps = due_steps(model, run, microseconds);
    if (steps == 0u) {
        return;
    }
    double dt = model->step_seconds > 0.0
                    ? model->step_seconds
                    : (double)model->step_microseconds / MICROSECONDS_PER_SECOND;
    memcpy(held, run->output, sizeof(held));
    run->library.description->advance(run->state, model->constants, input, run->output, dt, steps);
    if (!all_finite(run->output, model->outputs)) {
        memcpy(run->output, held, sizeof(held));
        run->fault = NOT_FINITE;
    }
}
