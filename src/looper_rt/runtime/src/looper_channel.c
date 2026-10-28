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

#include <string.h>

#include "looper_channel_frames.h"
#include "looper_channel_pipe.h"
#include "looper_commands.h"
#include "looper_pace.h"
#include "looper_protocol.h"
#include "looper_watch.h"

static looper_frame_reader_t reader;
static bool channel_open = false;
static const char *firmware_name;
static bool advancing = false;
static uint32_t advance_id = 0u;

bool looper_channel_open(const char *firmware)
{
    firmware_name = firmware;
    looper_frame_reset(&reader);
    channel_open = looper_pipe_take_stdio();
    return channel_open;
}

/** @brief Serve one request and send the reply, using the reader's buffer. */
static bool reply(const looper_request_t *request)
{
    looper_response_t response;
    uint8_t *buffer = looper_frame_payload(&reader);
    size_t encoded = 0u;

    looper_commands_serve(request, &response, firmware_name);
    return looper_protocol_encode_response(&response, buffer, LOOPER_FRAME_CAPACITY, &encoded)
           && looper_frame_send(buffer, encoded);
}

/*
 * Answer the request in the reader.
 *
 * Letting time pass on a held firmware is the one request not answered at
 * once: the firmware has to run through the time first, and the answer says
 * where the clock got to once it has.
 */
static bool answer(size_t length)
{
    looper_request_t request;

    if (!looper_protocol_decode_request(looper_frame_payload(&reader), length, &request)) {
        return false;
    }
    if (request.command == LOOPER_COMMAND_ADVANCE_TIME && looper_pace_is_held()) {
        looper_pace_release(request.microseconds);
        advancing = true;
        advance_id = request.id;
        return true;
    }
    return reply(&request);
}

/*
 * Answer a request to let time pass, once the firmware has run through it.
 *
 * Nothing else is read meanwhile: the one client asked and is waiting for
 * this answer before it asks anything else.
 */
static bool settle_advance(void)
{
    looper_request_t request;

    if (looper_pace_may_run()) {
        return false;
    }
    memset(&request, 0, sizeof(request));
    request.id = advance_id;
    request.command = LOOPER_COMMAND_READ_TIME;
    advancing = false;
    if (!reply(&request)) {
        looper_channel_close();
        return false;
    }
    return true;
}

bool looper_channel_poll(void)
{
    size_t length = 0u;
    looper_frame_state_t state;

    looper_watch_poll();
    if (!channel_open) {
        return false;
    }
    if (advancing) {
        return settle_advance();
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
