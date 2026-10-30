/**
 * @file
 * @brief The part of reaching the model worker that differs between systems.
 *
 * Private to the model worker. Shared memory, named semaphores and asking
 * whether a process still lives are three calls on Windows and three
 * others elsewhere; settled here so that looper_model_worker.c can be about
 * the conversation.
 */
#ifndef LOOPER_MODEL_WORKER_PLATFORM_H
#define LOOPER_MODEL_WORKER_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/** @brief What a wait for the worker came to. */
typedef enum {
    LOOPER_WAIT_ANSWERED,
    LOOPER_WAIT_NOT_YET,
    LOOPER_WAIT_FAILED
} looper_wait_t;

/** @brief Handles to one worker's memory, semaphores and process. */
typedef struct {
    void *mapping;
    void *memory;
    void *request;
    void *reply;
    void *process;
    uint32_t pid;
} looper_worker_link_t;

/** @brief Map the shared memory of that name; NULL when there is none. */
void *looper_worker_map(const char *name, looper_worker_link_t *link);

/** @brief Open the two semaphores the names of which follow from the worker's. */
bool looper_worker_open_semaphores(const char *name, looper_worker_link_t *link);

/** @brief Remember the worker's process, to ask later whether it lives. */
bool looper_worker_watch_process(uint32_t pid, looper_worker_link_t *link);

/** @brief Post the request semaphore. */
void looper_worker_post(const looper_worker_link_t *link);

/** @brief Wait one round for the reply semaphore. */
looper_wait_t looper_worker_wait_round(const looper_worker_link_t *link);

/** @brief Whether the worker's process has ended. */
bool looper_worker_ended(const looper_worker_link_t *link);

/** @brief Let go of everything; the link is empty afterwards. */
void looper_worker_unlink(looper_worker_link_t *link);

#endif /* LOOPER_MODEL_WORKER_PLATFORM_H */
