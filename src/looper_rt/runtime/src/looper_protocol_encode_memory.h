/**
 * @file
 * @brief Writing what was measured into a response.
 *
 * Time, stack and heap. Kept apart from the rest of the response
 * because it is the half that exists only in a build that was asked to
 * measure, and because a reply carrying a page of five hundred
 * functions has different arithmetic from one carrying seven pins.
 *
 * Private to the protocol encoder.
 */
#ifndef LOOPER_PROTOCOL_ENCODE_MEMORY_H
#define LOOPER_PROTOCOL_ENCODE_MEMORY_H

#include <stdbool.h>

#include "looper_pb.h"
#include "looper_protocol.h"

/** @brief Write everything that was measured, in field order. */
bool looper_write_measurements(looper_pb_writer_t *writer, const looper_response_t *response);

#endif /* LOOPER_PROTOCOL_ENCODE_MEMORY_H */
