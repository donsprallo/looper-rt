/*
 * Length prefixed frames over a pipe that never waits to be read.
 */
#include "looper_channel_frames.h"

#include "looper_channel_pipe.h"

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

/** @brief Read up to `wanted` bytes of the frame in total. */
static bool fill_to(looper_frame_reader_t *reader, size_t wanted)
{
    const long read = looper_pipe_read_available(
        &reader->buffer[reader->received], wanted - reader->received
    );

    if (read == LOOPER_PIPE_GONE) {
        return false;
    }
    reader->received += (size_t)read;
    return true;
}

void looper_frame_reset(looper_frame_reader_t *reader)
{
    reader->received = 0u;
}

looper_frame_state_t looper_frame_read(looper_frame_reader_t *reader, size_t *length)
{
    size_t whole;

    if (reader->received < LOOPER_FRAME_HEADER_SIZE
        && !fill_to(reader, LOOPER_FRAME_HEADER_SIZE)) {
        return LOOPER_FRAME_BROKEN;
    }
    if (reader->received < LOOPER_FRAME_HEADER_SIZE) {
        return LOOPER_FRAME_PENDING;
    }
    *length = decode_length(reader->buffer);
    if (*length > LOOPER_FRAME_CAPACITY) {
        return LOOPER_FRAME_BROKEN;
    }
    whole = LOOPER_FRAME_HEADER_SIZE + *length;
    if (reader->received < whole && !fill_to(reader, whole)) {
        return LOOPER_FRAME_BROKEN;
    }
    return (reader->received == whole) ? LOOPER_FRAME_COMPLETE : LOOPER_FRAME_PENDING;
}

uint8_t *looper_frame_payload(looper_frame_reader_t *reader)
{
    return &reader->buffer[LOOPER_FRAME_HEADER_SIZE];
}

bool looper_frame_send(const uint8_t *payload, size_t length)
{
    uint8_t header[LOOPER_FRAME_HEADER_SIZE];

    encode_length(header, length);
    return looper_pipe_write(header, LOOPER_FRAME_HEADER_SIZE)
           && looper_pipe_write(payload, length);
}
