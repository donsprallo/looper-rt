/**
 * @file
 * @brief What a CAN frame looks like when it leaves the process.
 *
 * A bus inside the simulation carries structures. Anything outside it --
 * a bridge over a socket, a tool on the other end, a recording on disk
 * -- carries bytes, and the two have to agree on which bytes. This is
 * that agreement, written down once so that a bridge, a test and
 * whatever somebody writes next all say the same thing.
 *
 * Six bytes of header and then the payload:
 *
 * | Offset | Size | Meaning                                  |
 * | ------ | ---- | ---------------------------------------- |
 * | 0      | 4    | identifier, little endian                |
 * | 4      | 1    | flags; bit 0 set for an extended frame   |
 * | 5      | 1    | payload length, 0 to 8                   |
 * | 6      | n    | the payload                              |
 *
 * Little endian because every machine that runs a simulation is, and a
 * byte order nobody has to think about is worth more here than one that
 * matches the network convention.
 */
#ifndef LOOPER_CAN_WIRE_H
#define LOOPER_CAN_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Bytes before the payload. */
#define LOOPER_CAN_WIRE_HEADER 6u

/** @brief Largest a frame can be on the wire. */
#define LOOPER_CAN_WIRE_CAPACITY (LOOPER_CAN_WIRE_HEADER + LOOPER_CAN_PAYLOAD_CAPACITY)

/**
 * @brief Write one frame as bytes.
 * @param frame Frame to write.
 * @param buffer Where to write it.
 * @param capacity Room in that buffer.
 * @return Bytes written, or zero when there was not room.
 */
size_t looper_can_wire_encode(
    const looper_can_frame_t *frame, uint8_t *buffer, size_t capacity
);

/**
 * @brief Read one frame from bytes.
 *
 * A stream hands over whatever has arrived, which may be half a frame or
 * three of them. Reading one says how many bytes it took, so a caller
 * can keep the rest and ask again.
 *
 * @param data Bytes that have arrived.
 * @param length How many.
 * @param frame Receives the frame.
 * @return Bytes consumed, or zero when a whole frame is not there yet.
 */
size_t looper_can_wire_decode(const uint8_t *data, size_t length, looper_can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_CAN_WIRE_H */
