/**
 * @file
 * @brief The commands that answer about watched firmware variables.
 *
 * Private to the command dispatcher. Kept apart for the same reason the
 * bus commands are: the dispatcher should read as a list of what a client
 * can ask, not as the answers themselves.
 */
#ifndef LOOPER_COMMANDS_WATCH_H
#define LOOPER_COMMANDS_WATCH_H

#include "looper_protocol.h"

/** @brief Answer with every watched variable. */
void looper_serve_list_watches(looper_response_t *response);

/** @brief Start sampling a variable, answering with the watch itself. */
void looper_serve_watch_variable(
    const looper_request_t *request, looper_response_t *response
);

/** @brief Stop sampling a watched variable. */
void looper_serve_remove_watch(const looper_request_t *request, looper_response_t *response);

/** @brief Write a value into a watched variable. */
void looper_serve_write_variable(const looper_request_t *request, looper_response_t *response);

#endif /* LOOPER_COMMANDS_WATCH_H */
