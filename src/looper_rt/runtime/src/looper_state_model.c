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
    if (model->provider == LOOPER_MODEL_PROVIDER_INTERPRETED && model->slot >= LOOPER_WORKER_SLOTS) {
        return "an interpreted state model in a slot the model worker does not have";
    }
    if (model->provider != LOOPER_MODEL_PROVIDER_COMPILED && model->provider != LOOPER_MODEL_PROVIDER_INTERPRETED) {
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

/* What the worker answered, taken into the run; false when the model or the worker failed. */
static bool take(const looper_state_model_t *model, looper_model_run_t *run, const char *broken,
                 const looper_worker_answer_t *answer)
{
    if (broken != NULL) {
        run->fault = broken;
        return false;
    }
    if (answer->fault[0] != '\0') {
        memcpy(run->fault_text, answer->fault, sizeof(run->fault_text));
        run->fault = run->fault_text;
        return false;
    }
    memcpy(run->output, answer->output, model->outputs * sizeof(double));
    memcpy(run->state, answer->state, model->states * sizeof(double));
    return true;
}

void looper_state_model_reset(const looper_state_model_t *model, looper_model_run_t *run)
{
    static looper_worker_answer_t answer;
    const double nothing[LOOPER_MODEL_PORTS] = {0.0};

    looper_state_model_start(model, run);
    if (model->provider == LOOPER_MODEL_PROVIDER_INTERPRETED) {
        const char *broken = looper_model_worker_ask(LOOPER_WORKER_RESET, model->slot, nothing, 0u, 0.0, &answer);
        if (take(model, run, broken, &answer)) {
            memset(run->output, 0, sizeof(run->output));
        }
        return;
    }
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

/* The steps, in Python: one request to the worker, whatever their number. */
static void advance_interpreted(
    const looper_state_model_t *model, looper_model_run_t *run, const double *input, uint32_t steps, double dt
)
{
    static looper_worker_answer_t answer;
    const char *broken = looper_model_worker_ask(LOOPER_WORKER_ADVANCE, model->slot, input, steps, dt, &answer);
    (void)take(model, run, broken, &answer);
}

void looper_state_model_advance(
    const looper_state_model_t *model, looper_model_run_t *run, const double *input, uint64_t microseconds
)
{
    double held[LOOPER_MODEL_PORTS];
    bool interpreted = model->provider == LOOPER_MODEL_PROVIDER_INTERPRETED;

    if (run->fault != NULL || (!interpreted && run->library.description == NULL)) {
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
    if (interpreted) {
        advance_interpreted(model, run, input, steps, dt);
    } else {
        run->library.description->advance(run->state, model->constants, input, run->output, dt, steps);
    }
    if (run->fault == NULL && !all_finite(run->output, model->outputs)) {
        memcpy(run->output, held, sizeof(held));
        run->fault = NOT_FINITE;
    }
}
