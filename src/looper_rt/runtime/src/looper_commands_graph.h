/**
 * @file
 * @brief The commands that install the signal graph and answer about it.
 *
 * Private to the command dispatcher. Kept apart because the graph has grown
 * three commands of its own: install it, read how its models are doing, and
 * reset one of them.
 */
#ifndef LOOPER_COMMANDS_GRAPH_H
#define LOOPER_COMMANDS_GRAPH_H

#include "looper_protocol.h"

/** @brief Replace the graph with the request's, or say why it is refused. */
void looper_serve_set_signal_graph(const looper_request_t *request, looper_response_t *response);

/** @brief Answer with how every model node of the graph is doing. */
void looper_serve_read_signal_graph(looper_response_t *response);

/** @brief Put the request's model node back to the state it was installed with. */
void looper_serve_reset_model(const looper_request_t *request, looper_response_t *response);

#endif /* LOOPER_COMMANDS_GRAPH_H */
