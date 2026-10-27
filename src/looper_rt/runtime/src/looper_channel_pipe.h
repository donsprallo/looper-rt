/**
 * @file
 * @brief Standard input and output as a byte stream that never waits to read.
 *
 * The part of the control channel that differs between systems: how the
 * streams are taken over, how to ask whether anything has arrived, and
 * how bytes are written. Settled here so that the channel above can be
 * about frames instead of about Windows.
 *
 * Private to the control channel.
 */
#ifndef LOOPER_CHANNEL_PIPE_H
#define LOOPER_CHANNEL_PIPE_H

#include <stdbool.h>
#include <stddef.h>

/** @brief What a read returns when the other end is gone. */
#define LOOPER_PIPE_GONE (-1L)

/**
 * @brief Keep standard output for the channel and point fd 1 at stderr.
 *
 * Both streams are switched to binary.
 *
 * @retval false A stream could not be duplicated or redirected.
 */
bool looper_pipe_take_stdio(void);

/**
 * @brief Read what has arrived on standard input, without waiting.
 * @param destination Where the bytes go.
 * @param capacity Most bytes to read.
 * @return Bytes read, zero when nothing was waiting, or @ref LOOPER_PIPE_GONE.
 */
long looper_pipe_read_available(void *destination, size_t capacity);

/**
 * @brief Write all of the bytes to the channel's output.
 * @retval false The other end is gone.
 */
bool looper_pipe_write(const void *data, size_t length);

/** @brief Close the channel's output. Standard input is left to the process. */
void looper_pipe_release(void);

#endif /* LOOPER_CHANNEL_PIPE_H */
