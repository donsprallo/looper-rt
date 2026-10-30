/**
 * @file
 * @brief Asking the model worker to step a model in Python.
 *
 * Private to the state model. A state model whose step cannot be compiled
 * runs in a Python process looper-core starts beside the simulation: the
 * model worker. The two share one region of memory and two named
 * semaphores. The simulation writes a request into the memory and posts the
 * first semaphore; the worker steps the model, writes the answer and posts
 * the second. One request is out at a time, which the single thread the
 * runtime runs on guarantees.
 *
 * The worker can be held at a breakpoint for as long as its author likes,
 * so there is no timeout: the simulation waits in rounds and asks after
 * each whether the worker still lives, and only a worker that ended is an
 * error. See plans/model-concept.md in looper-rs, section 14.
 */
#ifndef LOOPER_MODEL_WORKER_H
#define LOOPER_MODEL_WORKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOOPER_WORKER_MAGIC 0x574D504Cu
#define LOOPER_WORKER_VERSION 1u
#define LOOPER_WORKER_SLOTS 64u
#define LOOPER_WORKER_PORTS 8u
#define LOOPER_WORKER_STATES 64u
#define LOOPER_WORKER_FAULT 256u
#include "looper_types.h"

/** @brief Room for the worker's name, from which the names of its parts follow. */
#define LOOPER_WORKER_NAME_CAPACITY LOOPER_MODEL_WORKER_NAME_CAPACITY

/** @brief What the simulation asks of the worker. */
typedef enum {
    LOOPER_WORKER_ADVANCE = 1,
    LOOPER_WORKER_RESET = 2
} looper_worker_operation_t;

/** @brief Who serves the memory, and whether it is ready to. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t ready;
    uint32_t worker_pid;
} looper_worker_header_t;

/** @brief Written by the simulation before it posts the request semaphore. */
typedef struct {
    uint32_t operation;
    uint32_t slot;
    uint32_t steps;
    uint32_t sequence;
    double dt;
} looper_worker_request_t;

/** @brief Written by the worker before it posts the reply semaphore. */
typedef struct {
    uint32_t sequence;
    uint32_t outcome;
    char fault[LOOPER_WORKER_FAULT];
} looper_worker_reply_t;

/** @brief One installed model: its sizes and the numbers that cross over. */
typedef struct {
    uint32_t inputs;
    uint32_t outputs;
    uint32_t states;
    uint32_t installed;
    double input[LOOPER_WORKER_PORTS];
    double output[LOOPER_WORKER_PORTS];
    double state[LOOPER_WORKER_STATES];
} looper_worker_slot_t;

/** @brief The whole region. looper-core mirrors it with ctypes and checks the layout. */
typedef struct {
    looper_worker_header_t header;
    looper_worker_request_t request;
    looper_worker_reply_t reply;
    looper_worker_slot_t slots[LOOPER_WORKER_SLOTS];
} looper_worker_memory_t;

/** @brief What one request came to. */
typedef struct {
    double output[LOOPER_WORKER_PORTS];
    double state[LOOPER_WORKER_STATES];
    /** Why the model failed, empty when it did not. */
    char fault[LOOPER_WORKER_FAULT];
} looper_worker_answer_t;

/**
 * @brief Attach to the worker of that name, unless already attached to it.
 * @return Why it cannot be attached to, or NULL when it is.
 */
const char *looper_model_worker_attach(const char *name);

/** @brief Let go of the worker; nothing happens when attached to none. */
void looper_model_worker_detach(void);

/** @brief Whether a slot holds a model of these sizes, as the worker installed it. */
bool looper_model_worker_holds(uint32_t slot, uint32_t inputs, uint32_t outputs, uint32_t states);

/**
 * @brief Ask the worker to step a model, or to make it afresh, and wait for the answer.
 * @param operation Advance or reset.
 * @param slot The model.
 * @param input One value per input, held for every step.
 * @param steps How many steps; ignored for a reset.
 * @param dt The step, in seconds.
 * @param answer Receives the outputs, the state, and why the model failed.
 * @return Why the worker could not answer -- it ended, or was never there --
 *         or NULL when it did, whatever the model made of the request.
 */
const char *looper_model_worker_ask(
    looper_worker_operation_t operation,
    uint32_t slot,
    const double *input,
    uint32_t steps,
    double dt,
    looper_worker_answer_t *answer
);

#endif /* LOOPER_MODEL_WORKER_H */
