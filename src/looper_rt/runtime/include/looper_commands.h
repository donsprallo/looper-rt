/**
 * @file
 * @brief Handling of control protocol commands.
 *
 * Separated from the socket transport so that the command behaviour can be
 * exercised without a connection: build a request, call the handler, inspect
 * the response.
 */
#ifndef LOOPER_COMMANDS_H
#define LOOPER_COMMANDS_H

#include "looper_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Answer one request against the current simulation state.
 * @param request Decoded request.
 * @param response Receives the answer. Any previous content is overwritten.
 * @param firmware Name reported for LOOPER_COMMAND_HELLO, or NULL.
 * @note Pin and journal arrays in the response point at storage owned by this
 *       module and stay valid until the next call.
 */
void looper_commands_serve(
    const looper_request_t *request, looper_response_t *response, const char *firmware
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_COMMANDS_H */
