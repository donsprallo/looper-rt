/**
 * @file
 * @brief Loading a compiled state model into the running simulation.
 *
 * Private to the state model. The only part of the runtime that loads code
 * at run time, and the only part that differs between Windows and the rest
 * for it: LoadLibraryW there, dlopen elsewhere.
 */
#ifndef LOOPER_MODEL_LIBRARY_H
#define LOOPER_MODEL_LIBRARY_H

#include "looper_model_abi.h"
#include "looper_types.h"

/** @brief One loaded library and what it says it holds. */
typedef struct {
    void *handle;
    const looper_model_abi_t *description;
} looper_model_library_t;

/**
 * @brief Load the library a model names and check that it is that model.
 *
 * Refused when it cannot be loaded, exports no such function, was built
 * for another interface, or holds a model of another size than the
 * request says.
 *
 * @param model The model as the request installs it.
 * @param library Receives the loaded library; left empty when refused.
 * @return Why it was refused, or NULL when it is loaded.
 */
const char *looper_model_library_open(const looper_state_model_t *model, looper_model_library_t *library);

/** @brief Let a library go; nothing happens for one that was never loaded. */
void looper_model_library_close(looper_model_library_t *library);

#endif /* LOOPER_MODEL_LIBRARY_H */
