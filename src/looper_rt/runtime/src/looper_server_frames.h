/**
 * @file
 * @brief Where one request ends and the next begins.
 *
 * A stream has no messages in it, only bytes, so each one is preceded by
 * its length in four little endian bytes. That is the whole of the
 * framing, and keeping it here leaves the server itself about clients
 * rather than about arithmetic on headers.
 *
 * Nothing waits. A frame that has not all arrived is left on the socket
 * and looked at again on the next pass of the firmware loop, which is
 * what spares the server a half read frame to keep track of.
 *
 * Private to the control channel.
 */
#ifndef LOOPER_SERVER_FRAMES_H
#define LOOPER_SERVER_FRAMES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_socket.h"

/** @brief Bytes of length that precede every frame. */
#define LOOPER_FRAME_HEADER_SIZE 4u

/**
 * @brief Largest frame either side will send.
 *
 * A reply holds a page of a trace rather than a whole one, so this is a
 * ceiling nothing reaches rather than a budget anybody spends.
 */
#define LOOPER_FRAME_CAPACITY 32768u

/**
 * @brief Length of the whole frame waiting on a socket, if one is.
 *
 * The frame is looked at rather than taken, so a caller that is given a
 * length may then take exactly that many bytes.
 *
 * @param socket Client to look at.
 * @param buffer Where the frame is looked at, header included. It holds
 *        `LOOPER_FRAME_HEADER_SIZE + LOOPER_FRAME_CAPACITY` bytes.
 * @param gone Set when the client is gone or sent something impossible.
 * @return The payload length, or zero when no whole frame is waiting.
 */
size_t looper_frame_waiting(looper_socket_t socket, uint8_t *buffer, bool *gone);

/**
 * @brief Take bytes that have already arrived.
 * @retval false The client went away mid frame.
 */
bool looper_frame_take(looper_socket_t socket, uint8_t *buffer, size_t count);

/**
 * @brief Send one frame, length first.
 * @retval false The client went away.
 */
bool looper_frame_send(looper_socket_t socket, const uint8_t *payload, size_t length);

#endif /* LOOPER_SERVER_FRAMES_H */
