/**
 * @file
 * @brief Serving the profile, which is the one reply that has to be paged.
 *
 * A window of profiling holds every function that ran, which on a real
 * firmware is more than a single frame carries. The reply therefore
 * fills one page and says where the next one starts, the same way the
 * traces do.
 *
 * Private to the command dispatcher.
 */
#ifndef LOOPER_COMMANDS_PROFILE_H
#define LOOPER_COMMANDS_PROFILE_H

#include "looper_protocol.h"

/** @brief Fill one page of the profile, starting where the request says. */
void looper_serve_read_profile_paths(
    const looper_request_t *request, looper_response_t *response
);

void looper_serve_watch_profile_paths(
    const looper_request_t *request, looper_response_t *response
);

void looper_serve_read_profile(
    const looper_request_t *request, looper_response_t *response
);

/**
 * @brief Report what the allocator has been asked for.
 *
 * Not paged: a firmware holds a handful of places that allocate, not a
 * thousand, and the ones holding nothing are left out.
 */
void looper_serve_read_heap(looper_response_t *response);

#endif /* LOOPER_COMMANDS_PROFILE_H */
