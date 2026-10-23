/*
 * The control channel, and the several clients that may be on it.
 *
 * More than one is the ordinary case rather than the exception: a window
 * open on the buses while another is open on the scope, a test driving
 * the firmware while someone watches it. So the server holds a small
 * table of clients, accepts whatever is waiting, and gives each of them
 * at most one frame per pass of the firmware loop.
 *
 * One frame each, not all of them: a client that asked faster than the
 * firmware runs would otherwise be served over and over while the
 * firmware stood still, and the others would wait behind it.
 *
 * The simulation has one state and every client acts on that one state.
 * A client that clears the trace clears it for everybody. That is the
 * honest arrangement -- they are all looking at the same firmware -- and
 * the alternative, a first client that may write and others that may
 * not, answers a question nobody asked.
 *
 * Nothing here ever waits. A frame that has not completely arrived is
 * left on the socket and looked at again next pass.
 */
#include "looper_server.h"

#include "looper_commands.h"
#include "looper_protocol.h"
#include "looper_server_frames.h"
#include "looper_socket.h"
#include "looper_watch.h"

#ifndef LOOPER_SERVER_CLIENTS
/**
 * @brief Clients that may be attached at once.
 *
 * Two windows and a test is three; the rest is room to be wrong in.
 */
#define LOOPER_SERVER_CLIENTS 8u
#endif

static looper_socket_t listening_socket = LOOPER_SOCKET_NONE;
static looper_socket_t clients[LOOPER_SERVER_CLIENTS];
static const char *firmware_name;

/** @brief Forget a client, closing its socket. */
static void drop(size_t index)
{
    looper_socket_close(&clients[index]);
}

bool looper_server_start(uint16_t port, const char *firmware)
{
    firmware_name = firmware;
    for (size_t index = 0u; index < LOOPER_SERVER_CLIENTS; index++) {
        clients[index] = LOOPER_SOCKET_NONE;
    }
    listening_socket = looper_socket_listen(port, (int)LOOPER_SERVER_CLIENTS);
    return looper_socket_is_open(listening_socket);
}

/** @brief The first free place in the table, or the size when there is none. */
static size_t free_place(void)
{
    for (size_t index = 0u; index < LOOPER_SERVER_CLIENTS; index++) {
        if (!looper_socket_is_open(clients[index])) {
            return index;
        }
    }
    return LOOPER_SERVER_CLIENTS;
}

/*
 * Take everyone who is waiting.
 *
 * A connection that arrives when the table is full is accepted and
 * closed again rather than left in the backlog, so that a client which
 * will never be served finds out at once instead of hanging.
 */
static void accept_waiting(void)
{
    for (;;) {
        const looper_socket_t accepted = looper_socket_accept(listening_socket);
        const size_t place = free_place();

        if (!looper_socket_is_open(accepted)) {
            return;
        }
        if (place >= LOOPER_SERVER_CLIENTS) {
            looper_socket_t refused = accepted;

            looper_socket_close(&refused);
            return;
        }
        clients[place] = accepted;
    }
}

/** @brief Serve one client's next frame, if it has sent one. */
static bool serve(size_t index, uint8_t *buffer)
{
    looper_request_t request;
    looper_response_t response;
    bool gone = false;
    const size_t length = looper_frame_waiting(clients[index], buffer, &gone);
    size_t encoded = 0u;

    if (gone) {
        drop(index);
        return false;
    }
    if (length == 0u) {
        return false;
    }
    if (!looper_frame_take(clients[index], buffer, LOOPER_FRAME_HEADER_SIZE + length)
        || !looper_protocol_decode_request(
            &buffer[LOOPER_FRAME_HEADER_SIZE], length, &request
        )) {
        drop(index);
        return false;
    }
    looper_commands_serve(&request, &response, firmware_name);
    if (!looper_protocol_encode_response(&response, buffer, LOOPER_FRAME_CAPACITY, &encoded)
        || !looper_frame_send(clients[index], buffer, encoded)) {
        drop(index);
        return false;
    }
    return true;
}

bool looper_server_poll(void)
{
    static uint8_t buffer[LOOPER_FRAME_HEADER_SIZE + LOOPER_FRAME_CAPACITY];
    bool served = false;

    looper_watch_poll();
    if (!looper_socket_is_open(listening_socket)) {
        return false;
    }
    accept_waiting();
    for (size_t index = 0u; index < LOOPER_SERVER_CLIENTS; index++) {
        if (looper_socket_is_open(clients[index])) {
            served = serve(index, buffer) || served;
        }
    }
    return served;
}

size_t looper_server_client_count(void)
{
    size_t attached = 0u;

    for (size_t index = 0u; index < LOOPER_SERVER_CLIENTS; index++) {
        if (looper_socket_is_open(clients[index])) {
            attached++;
        }
    }
    return attached;
}

bool looper_server_has_client(void)
{
    return looper_server_client_count() > 0u;
}

void looper_server_stop(void)
{
    for (size_t index = 0u; index < LOOPER_SERVER_CLIENTS; index++) {
        drop(index);
    }
    looper_socket_close(&listening_socket);
}
