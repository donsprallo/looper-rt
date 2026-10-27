/**
 * @file
 * @brief Where one request ends and the next begins.
 *
 * A stream has no messages in it, only bytes, so each one is preceded by
 * its length in four little endian bytes. That is the whole of the
 * framing, and keeping it here leaves the channel itself about requests
 * rather than about arithmetic on headers.
 *
 * Nothing waits. A pipe cannot be looked into without taking from it, so
 * a frame that has partly arrived is kept in the reader and completed on
 * a later pass. The reader only ever asks for what the current frame
 * still lacks, which spares it bytes of the next frame to keep track of.
 *
 * Private to the control channel.
 */
#ifndef LOOPER_CHANNEL_FRAMES_H
#define LOOPER_CHANNEL_FRAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Bytes of length that precede every frame. */
#define LOOPER_FRAME_HEADER_SIZE 4u

/**
 * @brief Largest frame either side will send.
 *
 * A reply holds a page of a trace rather than a whole one, so this is a
 * ceiling nothing reaches rather than a budget anybody spends.
 */
#define LOOPER_FRAME_CAPACITY 32768u

/** @brief How far the frame being read has got. */
typedef enum {
    /** @brief More of the frame has yet to arrive. */
    LOOPER_FRAME_PENDING,
    /** @brief The whole frame is in the buffer. */
    LOOPER_FRAME_COMPLETE,
    /** @brief The other end is gone or sent something impossible. */
    LOOPER_FRAME_BROKEN
} looper_frame_state_t;

/** @brief A frame on its way in. */
typedef struct {
    /** @brief Header first, payload after it. */
    uint8_t buffer[LOOPER_FRAME_HEADER_SIZE + LOOPER_FRAME_CAPACITY];
    /** @brief Bytes of the frame that have arrived. */
    size_t received;
} looper_frame_reader_t;

/** @brief Start reading a new frame. */
void looper_frame_reset(looper_frame_reader_t *reader);

/**
 * @brief Take whatever of the current frame has arrived.
 * @param reader Frame being read.
 * @param length Set to the payload length once the frame is complete.
 */
looper_frame_state_t looper_frame_read(looper_frame_reader_t *reader, size_t *length);

/** @brief Where a complete frame's payload starts. */
uint8_t *looper_frame_payload(looper_frame_reader_t *reader);

/**
 * @brief Send one frame, length first.
 * @retval false The other end is gone.
 */
bool looper_frame_send(const uint8_t *payload, size_t length);

#endif /* LOOPER_CHANNEL_FRAMES_H */
