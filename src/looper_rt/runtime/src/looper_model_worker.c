/*
 * The conversation with the model worker: one request, one answer, in shared memory.
 */
#include "looper_model_worker.h"

#include <string.h>

#include "looper_model_worker_platform.h"

static looper_worker_link_t worker_link;
static char attached_to[LOOPER_WORKER_NAME_CAPACITY];

static looper_worker_memory_t *memory(void)
{
    return (looper_worker_memory_t *)worker_link.memory;
}

/* Whether the memory is a worker's, of this version, and served. */
static const char *header_fault(const looper_worker_header_t *header)
{
    if (header->magic != LOOPER_WORKER_MAGIC || header->version != LOOPER_WORKER_VERSION) {
        return "the model worker's memory is of another version than this simulation";
    }
    if (header->ready == 0u) {
        return "the model worker is not ready";
    }
    return NULL;
}

const char *looper_model_worker_attach(const char *name)
{
    if (worker_link.memory != NULL && strcmp(name, attached_to) == 0) {
        return NULL;
    }
    looper_model_worker_detach();
    if (strlen(name) >= LOOPER_WORKER_NAME_CAPACITY || looper_worker_map(name, &worker_link) == NULL) {
        looper_worker_unlink(&worker_link);
        return "there is no model worker of that name";
    }
    const char *fault = header_fault(&memory()->header);
    if (fault == NULL && !looper_worker_open_semaphores(name, &worker_link)) {
        fault = "the model worker's semaphores cannot be opened";
    }
    if (fault == NULL && !looper_worker_watch_process(memory()->header.worker_pid, &worker_link)) {
        fault = "the model worker's process cannot be watched";
    }
    if (fault != NULL) {
        looper_worker_unlink(&worker_link);
        return fault;
    }
    strcpy(attached_to, name);
    return NULL;
}

void looper_model_worker_detach(void)
{
    looper_worker_unlink(&worker_link);
    attached_to[0] = '\0';
}

bool looper_model_worker_holds(uint32_t slot, uint32_t inputs, uint32_t outputs, uint32_t states)
{
    if (worker_link.memory == NULL || slot >= LOOPER_WORKER_SLOTS) {
        return false;
    }
    const looper_worker_slot_t *held = &memory()->slots[slot];
    return held->installed != 0u && held->inputs == inputs && held->outputs == outputs
           && held->states == states;
}

/* Wait in rounds, so that a worker held at a breakpoint is waited for and an ended one is not. */
static const char *wait_for_answer(void)
{
    for (;;) {
        looper_wait_t waited = looper_worker_wait_round(&worker_link);
        if (waited == LOOPER_WAIT_ANSWERED) {
            return NULL;
        }
        if (waited == LOOPER_WAIT_FAILED || looper_worker_ended(&worker_link)) {
            return "the model worker ended";
        }
    }
}

static void take_answer(const looper_worker_slot_t *slot, looper_worker_answer_t *answer)
{
    const looper_worker_reply_t *reply = &memory()->reply;

    memcpy(answer->output, slot->output, sizeof(answer->output));
    memcpy(answer->state, slot->state, sizeof(answer->state));
    answer->fault[0] = '\0';
    if (reply->outcome != 0u) {
        memcpy(answer->fault, reply->fault, sizeof(answer->fault));
        answer->fault[LOOPER_WORKER_FAULT - 1u] = '\0';
    }
}

const char *looper_model_worker_ask(
    looper_worker_operation_t operation,
    uint32_t slot,
    const double *input,
    uint32_t steps,
    double dt,
    looper_worker_answer_t *answer
)
{
    if (worker_link.memory == NULL || slot >= LOOPER_WORKER_SLOTS) {
        return "there is no model worker";
    }
    looper_worker_slot_t *asked = &memory()->slots[slot];
    looper_worker_request_t *request = &memory()->request;
    memcpy(asked->input, input, asked->inputs * sizeof(double));
    request->operation = (uint32_t)operation;
    request->slot = slot;
    request->steps = steps;
    request->dt = dt;
    request->sequence++;
    looper_worker_post(&worker_link);
    const char *broken = wait_for_answer();
    if (broken != NULL) {
        return broken;
    }
    if (memory()->reply.sequence != request->sequence) {
        return "the model worker answered another request";
    }
    take_answer(asked, answer);
    return NULL;
}
