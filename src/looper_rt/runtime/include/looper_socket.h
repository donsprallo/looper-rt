/**
 * @file
 * @brief The small part of the sockets API a simulation actually uses.
 *
 * Two things in looper reach outside the process: the control channel a
 * client attaches to, and the bridges that put a simulated bus onto the
 * network. Both need the same half dozen calls, and both need them to
 * behave the same way on Windows and everywhere else, so the platform
 * difference is settled once here instead of twice in two files.
 *
 * Everything is non blocking. A simulation runs its firmware and serves
 * its sockets on one thread, and a read that waited would stop the
 * firmware until somebody typed something.
 *
 * Only IPv4 and only the loopback interface for listening. A simulation
 * is a program on your own machine for looking at firmware; it has no
 * business being reachable from the network.
 */
#ifndef LOOPER_SOCKET_H
#define LOOPER_SOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One socket, whatever the platform calls it.
 *
 * Signed and pointer wide: Windows counts sockets as unsigned handles
 * and everything else as small integers, and both fit in this with
 * their "no socket" value landing on -1.
 */
typedef intptr_t looper_socket_t;

/** @brief The value of a socket that is not open. */
#define LOOPER_SOCKET_NONE ((looper_socket_t)-1)

/** @brief Where a datagram came from, or is going to. */
typedef struct {
    /** @brief IPv4 address in host order. */
    uint32_t address;
    uint16_t port;
} looper_socket_peer_t;

/** @brief Prepare the platform's sockets. Harmless to call again. */
void looper_socket_startup(void);

/** @brief Whether a socket is open. */
bool looper_socket_is_open(looper_socket_t socket);

/** @brief Close a socket and set it to `LOOPER_SOCKET_NONE`. */
void looper_socket_close(looper_socket_t *socket);

/**
 * @brief Listen for connections on a port of the loopback interface.
 * @param port Port to listen on.
 * @param backlog Connections the system may hold before one is accepted.
 * @return The socket, or `LOOPER_SOCKET_NONE`.
 */
looper_socket_t looper_socket_listen(uint16_t port, int backlog);

/**
 * @brief Take a waiting connection, if there is one.
 * @return The accepted socket, or `LOOPER_SOCKET_NONE` when none waited.
 */
looper_socket_t looper_socket_accept(looper_socket_t listening);

/**
 * @brief Read what has arrived, without waiting.
 * @return Bytes read, 0 when nothing waited, -1 when the peer is gone.
 */
long looper_socket_receive(looper_socket_t socket, void *destination, size_t capacity);

/**
 * @brief Look at what has arrived without taking it off the socket.
 *
 * What makes a frame reader simple: a frame that has not all arrived is
 * left where it is and looked at again next time, rather than held in a
 * half read state of its own.
 *
 * @return Bytes available to look at, 0 when none, -1 when the peer is gone.
 */
long looper_socket_peek(looper_socket_t socket, void *destination, size_t capacity);

/**
 * @brief Send everything, as far as the socket will take it.
 * @return Bytes sent, or -1 when the peer is gone.
 */
long looper_socket_send(looper_socket_t socket, const void *data, size_t length);

/**
 * @brief Bind a datagram socket to a port of the loopback interface.
 * @return The socket, or `LOOPER_SOCKET_NONE`.
 */
looper_socket_t looper_socket_bind_datagram(uint16_t port);

/**
 * @brief Take one waiting datagram.
 * @param peer Receives where it came from, which is where a reply goes.
 * @return Bytes taken, 0 when none waited, -1 on a failure worth noting.
 */
long looper_socket_receive_from(
    looper_socket_t socket, void *destination, size_t capacity, looper_socket_peer_t *peer
);

/**
 * @brief Send one datagram.
 * @return Bytes sent, or -1 when it could not be sent.
 */
long looper_socket_send_to(
    looper_socket_t socket,
    const looper_socket_peer_t *peer,
    const void *data,
    size_t length
);

/**
 * @brief Read a dotted IPv4 address.
 * @param text Address such as "127.0.0.1".
 * @param address Receives the address in host order.
 * @retval false The text is not an address.
 */
bool looper_socket_parse_address(const char *text, uint32_t *address);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_SOCKET_H */
