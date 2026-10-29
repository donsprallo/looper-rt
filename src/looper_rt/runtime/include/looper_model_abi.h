/**
 * @file
 * @brief The interface between the runtime and a compiled state model.
 *
 * looper-core translates a model's Python step to C and compiles it into a
 * shared library of its own. The library exports one function, named in
 * the request that installs it, which returns this description. The
 * generated source repeats these declarations rather than including this
 * header, so that it compiles with nothing but a C compiler; the abi number
 * and the sizes are what make sure the two still agree.
 *
 * The library does all the stepping in one call. A read that catches up a
 * hundred steps costs one call across the library boundary, not a hundred,
 * and the compiler can inline the step into its own loop.
 */
#ifndef LOOPER_MODEL_ABI_H
#define LOOPER_MODEL_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The version of this interface a library has to be built for. */
#define LOOPER_MODEL_ABI 1u

/**
 * @brief Step a model some steps with its inputs held.
 * @param state The model's state; updated.
 * @param constants What the model reads besides its state.
 * @param input One value per input, held for every step.
 * @param output Receives the outputs of the last step; left alone for none.
 * @param dt The step, in seconds, as the model's own step_seconds.
 * @param steps How many steps to take.
 */
typedef void (*looper_model_advance_t)(
    double *state, const double *constants, const double *input, double *output, double dt, uint32_t steps
);

/** @brief What a library says about the model it holds. */
typedef struct {
    uint32_t abi;
    uint32_t states;
    uint32_t inputs;
    uint32_t outputs;
    uint32_t constants;
    looper_model_advance_t advance;
} looper_model_abi_t;

/** @brief The function a model library exports. */
typedef const looper_model_abi_t *(*looper_model_describe_t)(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_MODEL_ABI_H */
