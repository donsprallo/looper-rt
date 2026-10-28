/*
 * A discrete linear system: x[k+1] = A x[k] + B u[k], y = C x + D u.
 */
#include "looper_linear_system.h"

#include <string.h>

/*
 * The most steps one read catches up. A system left unread through a long
 * pause would otherwise stall the firmware working off the backlog; past
 * this it jumps to the present instead, with the state it had.
 */
#define MOST_STEPS_PER_READ 100000u

const char *looper_linear_fault(const looper_linear_system_t *system)
{
    if (system->states > LOOPER_LINEAR_STATES || system->inputs > LOOPER_LINEAR_PORTS
        || system->outputs > LOOPER_LINEAR_PORTS) {
        return "a linear system larger than the simulation has room for";
    }
    if (system->step_microseconds == 0u || system->outputs == 0u) {
        return "a linear system without a step or without an output";
    }
    if (system->a_count != system->states * system->states
        || system->b_count != system->states * system->inputs
        || system->c_count != system->outputs * system->states
        || system->d_count != system->outputs * system->inputs) {
        return "a linear system whose matrices do not match its size";
    }
    return NULL;
}

void looper_linear_reset(looper_linear_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

/* x = A x + B u, once. */
static void step(const looper_linear_system_t *system, looper_linear_state_t *state, const double *input)
{
    double next[LOOPER_LINEAR_STATES];
    const uint32_t states = system->states;

    for (uint32_t row = 0u; row < states; row++) {
        double sum = 0.0;
        for (uint32_t column = 0u; column < states; column++) {
            sum += system->a[row * states + column] * state->state[column];
        }
        for (uint32_t column = 0u; column < system->inputs; column++) {
            sum += system->b[row * system->inputs + column] * input[column];
        }
        next[row] = sum;
    }
    memcpy(state->state, next, states * sizeof(double));
}

/* y = C x + D u. */
static void observe(const looper_linear_system_t *system, looper_linear_state_t *state, const double *input)
{
    for (uint32_t row = 0u; row < system->outputs; row++) {
        double sum = 0.0;
        for (uint32_t column = 0u; column < system->states; column++) {
            sum += system->c[row * system->states + column] * state->state[column];
        }
        for (uint32_t column = 0u; column < system->inputs; column++) {
            sum += system->d[row * system->inputs + column] * input[column];
        }
        state->output[row] = sum;
    }
}

void looper_linear_advance(
    const looper_linear_system_t *system,
    looper_linear_state_t *state,
    const double *input,
    uint64_t microseconds
)
{
    uint32_t taken = 0u;

    if (!state->started) {
        state->started = true;
        state->next_step = microseconds;
    }
    while (state->next_step <= microseconds && taken < MOST_STEPS_PER_READ) {
        step(system, state, input);
        state->next_step += system->step_microseconds;
        taken++;
    }
    if (state->next_step <= microseconds) {
        state->next_step = microseconds + system->step_microseconds;
    }
    observe(system, state, input);
}
