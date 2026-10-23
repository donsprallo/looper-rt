/**
 * @file
 * @brief The commands that answer about buses and components.
 *
 * Private to the command dispatcher. Kept apart because the bus model is
 * its own thing and the dispatcher is long enough already.
 */
#ifndef LOOPER_COMMANDS_BUS_H
#define LOOPER_COMMANDS_BUS_H

#include "looper_protocol.h"

/** @brief Answer with every registered bus. */
void looper_serve_list_buses(looper_response_t *response);

/** @brief Answer with every attached component. */
void looper_serve_list_components(looper_response_t *response);

/** @brief Answer with the exchanges recorded after the client's sequence number. */
void looper_serve_read_bus_trace(const looper_request_t *request, looper_response_t *response);

/** @brief Take the part of a peer on a bus, or stop taking it. */
void looper_serve_attach_component(
    const looper_request_t *request, looper_response_t *response
);
void looper_serve_detach_component(
    const looper_request_t *request, looper_response_t *response
);

/** @brief Put the request's message on its bus for the firmware to receive. */
void looper_serve_send_bus_message(
    const looper_request_t *request, looper_response_t *response
);

#endif /* LOOPER_COMMANDS_BUS_H */
