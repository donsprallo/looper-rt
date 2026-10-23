/**
 * @file
 * @brief The part of the sockets API that differs between systems.
 *
 * Two headers, two names for a handle, two ways of saying "nothing was
 * waiting". Settled here so that the files above can be about streams
 * and datagrams instead of about Windows.
 *
 * Private to the socket wrapper.
 */
#ifndef LOOPER_SOCKET_PLATFORM_H
#define LOOPER_SOCKET_PLATFORM_H

#include <stdbool.h>
#include <string.h>

#include "looper_socket.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
/** @brief A buffer as this platform's calls want it. */
#define LOOPER_BUFFER(pointer) ((char *)(pointer))
#define LOOPER_CONST_BUFFER(pointer) ((const char *)(pointer))
/** @brief What the platform wants a socket handed to it as. */
#define LOOPER_SOCKET_CAST SOCKET
typedef int looper_socket_length_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define LOOPER_BUFFER(pointer) (pointer)
#define LOOPER_CONST_BUFFER(pointer) (pointer)
#define LOOPER_SOCKET_CAST int
typedef socklen_t looper_socket_length_t;
#endif

/** @brief What a transfer returns when nothing was ready. */
#define LOOPER_NOTHING_WAITING 0L

/** @brief What a transfer returns when the other end is gone. */
#define LOOPER_PEER_GONE (-1L)

/** @brief Whether the last call failed only because nothing was ready. */
bool looper_socket_would_have_waited(void);

/** @brief Take a socket out of blocking mode. */
bool looper_socket_make_non_blocking(looper_socket_t socket);

/** @brief Bind a fresh socket of one kind to a loopback port. */
looper_socket_t looper_socket_bind(int kind, uint16_t port);

#endif /* LOOPER_SOCKET_PLATFORM_H */
