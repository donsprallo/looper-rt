/*
 * The stream half of the sockets.
 *
 * Every function returns the same shape on both platforms: a count of
 * bytes, zero when nothing was waiting, and -1 when the other end is
 * gone. Telling "nothing waiting" from "gone" is the whole difficulty,
 * because the two systems say it differently, and it is the difference
 * between a quiet poll and a dropped client.
 */
#include "looper_socket_platform.h"

/** @brief Whether the last call failed only because nothing was ready. */
bool looper_socket_would_have_waited(void)
{
#if defined(_WIN32)
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR);
#endif
}

/** @brief What one transfer call returned, in this file's terms. */
static long outcome_of(int transferred)
{
    if (transferred > 0) {
        return (long)transferred;
    }
    if (transferred == 0) {
        return LOOPER_PEER_GONE;
    }
    return looper_socket_would_have_waited() ? LOOPER_NOTHING_WAITING : LOOPER_PEER_GONE;
}

void looper_socket_startup(void)
{
#if defined(_WIN32)
    static bool started;
    WSADATA data;

    if (!started) {
        WSAStartup(MAKEWORD(2, 2), &data);
        started = true;
    }
#endif
}

bool looper_socket_is_open(looper_socket_t socket)
{
    return socket != LOOPER_SOCKET_NONE;
}

void looper_socket_close(looper_socket_t *socket)
{
    if ((socket == NULL) || !looper_socket_is_open(*socket)) {
        return;
    }
#if defined(_WIN32)
    closesocket((SOCKET)*socket);
#else
    close((int)*socket);
#endif
    *socket = LOOPER_SOCKET_NONE;
}

/** @brief Take a socket out of blocking mode. */
bool looper_socket_make_non_blocking(looper_socket_t socket)
{
#if defined(_WIN32)
    u_long mode = 1u;

    return ioctlsocket((SOCKET)socket, (long)FIONBIO, &mode) == 0;
#else
    int flags = fcntl((int)socket, F_GETFL, 0);

    return (flags >= 0) && (fcntl((int)socket, F_SETFL, flags | O_NONBLOCK) == 0);
#endif
}

/** @brief An address on the loopback interface. */
static struct sockaddr_in loopback_address(uint16_t port)
{
    struct sockaddr_in address;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
}

/** @brief Bind a fresh socket of one kind to a loopback port. */
looper_socket_t looper_socket_bind(int kind, uint16_t port)
{
    const struct sockaddr_in address = loopback_address(port);
    looper_socket_t opened;

    looper_socket_startup();
    opened = (looper_socket_t)socket(AF_INET, kind, 0);
    if (!looper_socket_is_open(opened)) {
        return LOOPER_SOCKET_NONE;
    }
    if (bind((LOOPER_SOCKET_CAST)opened, (const struct sockaddr *)&address, sizeof(address)) != 0) {
        looper_socket_close(&opened);
        return LOOPER_SOCKET_NONE;
    }
    if (!looper_socket_make_non_blocking(opened)) {
        looper_socket_close(&opened);
        return LOOPER_SOCKET_NONE;
    }
    return opened;
}

looper_socket_t looper_socket_listen(uint16_t port, int backlog)
{
    looper_socket_t opened = looper_socket_bind(SOCK_STREAM, port);

    if (!looper_socket_is_open(opened)) {
        return LOOPER_SOCKET_NONE;
    }
    if (listen((LOOPER_SOCKET_CAST)opened, backlog) != 0) {
        looper_socket_close(&opened);
        return LOOPER_SOCKET_NONE;
    }
    return opened;
}

looper_socket_t looper_socket_accept(looper_socket_t listening)
{
    looper_socket_t accepted = (looper_socket_t)accept((LOOPER_SOCKET_CAST)listening, NULL, NULL);

    if (!looper_socket_is_open(accepted)) {
        return LOOPER_SOCKET_NONE;
    }
    /* Not inherited the same way on every platform, and a blocking
     * client socket would stop the firmware loop on the first idle
     * reader. */
    if (!looper_socket_make_non_blocking(accepted)) {
        looper_socket_close(&accepted);
        return LOOPER_SOCKET_NONE;
    }
    return accepted;
}

long looper_socket_receive(looper_socket_t socket, void *destination, size_t capacity)
{
    return outcome_of(
        recv((LOOPER_SOCKET_CAST)socket, LOOPER_BUFFER(destination), (int)capacity, 0)
    );
}

long looper_socket_peek(looper_socket_t socket, void *destination, size_t capacity)
{
    return outcome_of(
        recv((LOOPER_SOCKET_CAST)socket, LOOPER_BUFFER(destination), (int)capacity, MSG_PEEK)
    );
}

long looper_socket_send(looper_socket_t socket, const void *data, size_t length)
{
    const int sent = send((LOOPER_SOCKET_CAST)socket, LOOPER_CONST_BUFFER(data), (int)length, 0);

    if (sent >= 0) {
        return (long)sent;
    }
    return looper_socket_would_have_waited() ? LOOPER_NOTHING_WAITING : LOOPER_PEER_GONE;
}
