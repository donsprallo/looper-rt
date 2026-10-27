/*
 * The control channel to the tool that started the simulation.
 *
 * There is one client and it is the parent process. Everybody else --
 * windows, tests -- talks to that tool, which is why nothing here keeps
 * a table of clients or listens for new ones.
 *
 * A frame that has not completely arrived stays in the reader and is
 * completed on a later pass. The other end going away, or sending
 * something that is not a frame, closes the channel for good: the
 * stream can no longer be trusted to be on a frame boundary.
 */
#include "looper_channel.h"

#include "looper_channel_frames.h"
#include "looper_channel_pipe.h"
#include "looper_commands.h"
#include "looper_protocol.h"
#include "looper_watch.h"

static looper_frame_reader_t reader;
static bool channel_open = false;
static const char *firmware_name;

bool looper_channel_open(const char *firmware)
{
    firmware_name = firmware;
    looper_frame_reset(&reader);
    channel_open = looper_pipe_take_stdio();
    return channel_open;
}

/** @brief Answer the request in the reader, reusing its buffer for the reply. */
static bool answer(size_t length)
{
    looper_request_t request;
    looper_response_t response;
    uint8_t *buffer = looper_frame_payload(&reader);
    size_t encoded = 0u;

    if (!looper_protocol_decode_request(buffer, length, &request)) {
        return false;
    }
    looper_commands_serve(&request, &response, firmware_name);
    return looper_protocol_encode_response(&response, buffer, LOOPER_FRAME_CAPACITY, &encoded)
           && looper_frame_send(buffer, encoded);
}

bool looper_channel_poll(void)
{
    size_t length = 0u;
    looper_frame_state_t state;

    looper_watch_poll();
    if (!channel_open) {
        return false;
    }
    state = looper_frame_read(&reader, &length);
    if (state == LOOPER_FRAME_PENDING) {
        return false;
    }
    if (state == LOOPER_FRAME_BROKEN || !answer(length)) {
        looper_channel_close();
        return false;
    }
    looper_frame_reset(&reader);
    return true;
}

bool looper_channel_is_open(void)
{
    return channel_open;
}

void looper_channel_close(void)
{
    if (!channel_open) {
        return;
    }
    looper_pipe_release();
    channel_open = false;
}
