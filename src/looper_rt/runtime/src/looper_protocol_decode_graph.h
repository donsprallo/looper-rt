/**
 * @file
 * @brief Decoding of the signal graph a request carries.
 *
 * Private to the request decoder. Kept apart because a graph is the one
 * part of a request that repeats, and its nested messages would otherwise
 * double the size of a decoder that is otherwise a flat list of fields.
 */
#ifndef LOOPER_PROTOCOL_DECODE_GRAPH_H
#define LOOPER_PROTOCOL_DECODE_GRAPH_H

#include <stdbool.h>

#include "looper_pb.h"
#include "looper_protocol.h"

/**
 * @brief Decode one node and append it to the request.
 *
 * A node beyond the capacity is read and dropped, and the request says so,
 * so that the command refuses a graph it could only hold part of.
 */
bool looper_decode_signal_node(looper_pb_reader_t *reader, looper_request_t *request);

/** @brief Decode one target and append it to the request, likewise. */
bool looper_decode_signal_target(looper_pb_reader_t *reader, looper_request_t *request);

/** @brief Decode a generated signal: waveform, amplitude, offset, frequency, phase. */
bool looper_decode_signal(looper_pb_reader_t *reader, looper_signal_t *signal);

#endif /* LOOPER_PROTOCOL_DECODE_GRAPH_H */
