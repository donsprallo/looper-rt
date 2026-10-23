/*
 * The commands that answer about watched firmware variables.
 */
#include "looper_commands_watch.h"

#include "looper_trace.h"
#include "looper_watch.h"

#define MAX_WATCHES_PER_RESPONSE LOOPER_WATCH_CAPACITY

static looper_watch_t watch_buffer[MAX_WATCHES_PER_RESPONSE];

static size_t collect_watches(void)
{
    size_t count = looper_watch_count();

    if (count > MAX_WATCHES_PER_RESPONSE) {
        count = MAX_WATCHES_PER_RESPONSE;
    }
    for (size_t index = 0u; index < count; index++) {
        watch_buffer[index] = *looper_watch_at(index);
    }
    return count;
}

void looper_serve_list_watches(looper_response_t *response)
{
    response->watches = watch_buffer;
    response->watch_count = collect_watches();
    response->next_sequence = looper_trace_next_sequence();
}

/*
 * A watch is answered with the watch itself rather than with a bare channel
 * number, so that a client learns the value the variable already had without
 * waiting for the first change to be recorded.
 */
void looper_serve_watch_variable(const looper_request_t *request, looper_response_t *response)
{
    const looper_watch_t *created = NULL;

    if (!looper_watch_add(
            request->path,
            request->address,
            request->encoding,
            request->byte_width,
            request->bit_offset,
            request->bit_width,
            request->writable,
            &created
        )) {
        response->status = LOOPER_STATUS_INVALID_ARGUMENT;
        response->message =
            "the address is outside the running image, the width does not suit the "
            "encoding, or no watch is free";
        return;
    }
    watch_buffer[0] = *created;
    response->watches = watch_buffer;
    response->watch_count = 1u;
    response->next_sequence = looper_trace_next_sequence();
}

void looper_serve_remove_watch(const looper_request_t *request, looper_response_t *response)
{
    if (looper_watch_remove(request->channel)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = "no watch holds that channel";
}

void looper_serve_write_variable(const looper_request_t *request, looper_response_t *response)
{
    if (looper_watch_write(request->channel, request->value)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message =
        "no watch holds that channel, the variable is read only, or the value is "
        "outside the range it can hold";
}

