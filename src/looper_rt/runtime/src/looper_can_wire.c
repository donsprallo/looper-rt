/*
 * The byte layout of a CAN frame, in both directions.
 *
 * Nothing is trusted. A length past eight or a frame that has not all
 * arrived is refused rather than clamped, because the bytes came from
 * outside the process and the only safe thing to do with a frame that
 * does not make sense is to not have one.
 */
#include "looper_can_wire.h"

#include <string.h>

#define FLAG_EXTENDED 0x01u
#define BYTE_MASK 0xFFu

#define OFFSET_IDENTIFIER 0u
#define OFFSET_FLAGS 4u
#define OFFSET_LENGTH 5u

size_t looper_can_wire_encode(const looper_can_frame_t *frame, uint8_t *buffer, size_t capacity)
{
    const size_t length = (size_t)frame->length;
    const size_t needed = LOOPER_CAN_WIRE_HEADER + length;

    if ((length > LOOPER_CAN_PAYLOAD_CAPACITY) || (capacity < needed)) {
        return 0u;
    }
    for (size_t byte = 0u; byte < 4u; byte++) {
        buffer[OFFSET_IDENTIFIER + byte] = (uint8_t)((frame->id >> (8u * byte)) & BYTE_MASK);
    }
    buffer[OFFSET_FLAGS] = frame->extended ? FLAG_EXTENDED : 0u;
    buffer[OFFSET_LENGTH] = (uint8_t)length;
    memcpy(&buffer[LOOPER_CAN_WIRE_HEADER], frame->payload, length);
    return needed;
}

size_t looper_can_wire_decode(const uint8_t *data, size_t length, looper_can_frame_t *frame)
{
    size_t payload;
    uint32_t identifier = 0u;

    if (length < LOOPER_CAN_WIRE_HEADER) {
        return 0u;
    }
    payload = (size_t)data[OFFSET_LENGTH];
    if (payload > LOOPER_CAN_PAYLOAD_CAPACITY) {
        return 0u;
    }
    if (length < LOOPER_CAN_WIRE_HEADER + payload) {
        return 0u;
    }
    for (size_t byte = 0u; byte < 4u; byte++) {
        identifier |= (uint32_t)data[OFFSET_IDENTIFIER + byte] << (8u * byte);
    }
    memset(frame, 0, sizeof(*frame));
    frame->id = identifier;
    frame->extended = (data[OFFSET_FLAGS] & FLAG_EXTENDED) != 0u;
    frame->length = (uint8_t)payload;
    memcpy(frame->payload, &data[LOOPER_CAN_WIRE_HEADER], payload);
    return LOOPER_CAN_WIRE_HEADER + payload;
}
