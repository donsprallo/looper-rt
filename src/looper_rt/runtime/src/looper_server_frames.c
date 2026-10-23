/*
 * Length prefixed frames over a socket that never waits.
 */
#include "looper_server_frames.h"

#define BYTE_MASK 0xFFu

static size_t decode_length(const uint8_t *header)
{
    return (size_t)header[0] | ((size_t)header[1] << 8u) | ((size_t)header[2] << 16u)
           | ((size_t)header[3] << 24u);
}

static void encode_length(uint8_t *header, size_t length)
{
    header[0] = (uint8_t)(length & BYTE_MASK);
    header[1] = (uint8_t)((length >> 8u) & BYTE_MASK);
    header[2] = (uint8_t)((length >> 16u) & BYTE_MASK);
    header[3] = (uint8_t)((length >> 24u) & BYTE_MASK);
}

size_t looper_frame_waiting(looper_socket_t socket, uint8_t *buffer, bool *gone)
{
    const long header = looper_socket_peek(socket, buffer, LOOPER_FRAME_HEADER_SIZE);
    size_t length;
    long whole;

    *gone = header < 0;
    if (header < (long)LOOPER_FRAME_HEADER_SIZE) {
        return 0u;
    }
    length = decode_length(buffer);
    if (length > LOOPER_FRAME_CAPACITY) {
        *gone = true;
        return 0u;
    }
    whole = looper_socket_peek(socket, buffer, LOOPER_FRAME_HEADER_SIZE + length);
    *gone = whole < 0;
    return (whole == (long)(LOOPER_FRAME_HEADER_SIZE + length)) ? length : 0u;
}

bool looper_frame_take(looper_socket_t socket, uint8_t *buffer, size_t count)
{
    size_t taken = 0u;

    while (taken < count) {
        const long chunk = looper_socket_receive(socket, &buffer[taken], count - taken);

        if (chunk < 0) {
            return false;
        }
        taken += (size_t)chunk;
    }
    return true;
}

bool looper_frame_send(looper_socket_t socket, const uint8_t *payload, size_t length)
{
    uint8_t header[LOOPER_FRAME_HEADER_SIZE];
    size_t sent = 0u;

    encode_length(header, length);
    if (looper_socket_send(socket, header, LOOPER_FRAME_HEADER_SIZE)
        < (long)LOOPER_FRAME_HEADER_SIZE) {
        return false;
    }
    while (sent < length) {
        const long chunk = looper_socket_send(socket, &payload[sent], length - sent);

        if (chunk < 0) {
            return false;
        }
        sent += (size_t)chunk;
    }
    return true;
}
