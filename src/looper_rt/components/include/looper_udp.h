/**
 * @file
 * @brief A simulated bus on a UDP port.
 *
 * The same bridge as `looper_tcp.h` over datagrams instead of a stream,
 * which suits a bus better in one way and worse in another. Better,
 * because one datagram is one message: a CAN frame arrives whole or not
 * at all, with no stream to fall out of step with. Worse, because
 * nothing says it arrives at all.
 *
 * ```c
 * #include "looper_udp.h"
 *
 * // Answer whoever writes to us.
 * looper_udp_can(BOARD_CAN_BUS, 0x700u, "CAN gateway", 46201u, NULL, 0u);
 *
 * // Or always send to one place, whoever writes.
 * looper_udp_uart(BOARD_UART_BUS, 0u, "Telemetry", 46200u, "127.0.0.1", 46210u);
 * ```
 *
 * A UART bridge puts what the firmware wrote into one datagram per
 * write. A CAN bridge puts one frame in one datagram, in the layout
 * `looper_can_wire.h` writes down -- the same bytes the TCP bridge
 * sends, so a tool can speak to either.
 */
#ifndef LOOPER_UDP_H
#define LOOPER_UDP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Carry a UART port over a UDP port.
 * @param bus Bus the port belongs to.
 * @param port Port to bridge.
 * @param label Name a client lists the bridge under.
 * @param listen_port UDP port to bind.
 * @param peer_host Where to send, as a dotted address, or NULL to send
 *        back to whoever last wrote to us.
 * @param peer_port Port to send to, ignored when `peer_host` is NULL.
 * @retval true The bridge is bound and attached.
 */
bool looper_udp_uart(
    uint32_t bus,
    uint8_t port,
    const char *label,
    uint16_t listen_port,
    const char *peer_host,
    uint16_t peer_port
);

/**
 * @brief Carry a CAN bus over a UDP port.
 * @param bus Bus to bridge.
 * @param base_id Identifier the bridge is listed under; it filters nothing.
 * @param label Name a client lists the bridge under.
 * @param listen_port UDP port to bind.
 * @param peer_host Where to send, or NULL to answer whoever last wrote.
 * @param peer_port Port to send to, ignored when `peer_host` is NULL.
 * @retval true The bridge is bound and attached.
 */
bool looper_udp_can(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    uint16_t listen_port,
    const char *peer_host,
    uint16_t peer_port
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_UDP_H */
