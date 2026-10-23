/*
 * The datagram half of the sockets.
 *
 * One send is one message, which is what makes a bridge over UDP
 * simpler than one over a stream: a frame arrives whole or not at all.
 * Whether it arrives is the other matter, and not one this can help
 * with.
 */
#include "looper_socket_platform.h"

looper_socket_t looper_socket_bind_datagram(uint16_t port)
{
    return looper_socket_bind(SOCK_DGRAM, port);
}

long looper_socket_receive_from(
    looper_socket_t socket, void *destination, size_t capacity, looper_socket_peer_t *peer
)
{
    struct sockaddr_in from;
    looper_socket_length_t length = (looper_socket_length_t)sizeof(from);
    const int received = recvfrom(
        (LOOPER_SOCKET_CAST)socket,
        LOOPER_BUFFER(destination),
        (int)capacity,
        0,
        (struct sockaddr *)&from,
        &length
    );

    if (received < 0) {
        return looper_socket_would_have_waited() ? LOOPER_NOTHING_WAITING : LOOPER_PEER_GONE;
    }
    if (peer != NULL) {
        peer->address = ntohl(from.sin_addr.s_addr);
        peer->port = ntohs(from.sin_port);
    }
    return (long)received;
}

long looper_socket_send_to(
    looper_socket_t socket, const looper_socket_peer_t *peer, const void *data, size_t length
)
{
    struct sockaddr_in to;
    int sent;

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(peer->port);
    to.sin_addr.s_addr = htonl(peer->address);
    sent = sendto(
        (LOOPER_SOCKET_CAST)socket,
        LOOPER_CONST_BUFFER(data),
        (int)length,
        0,
        (const struct sockaddr *)&to,
        (int)sizeof(to)
    );
    return (sent < 0) ? LOOPER_PEER_GONE : (long)sent;
}

bool looper_socket_parse_address(const char *text, uint32_t *address)
{
    struct in_addr parsed;

    if ((text == NULL) || (address == NULL)) {
        return false;
    }
    looper_socket_startup();
    if (inet_pton(AF_INET, text, &parsed) != 1) {
        return false;
    }
    *address = ntohl(parsed.s_addr);
    return true;
}
