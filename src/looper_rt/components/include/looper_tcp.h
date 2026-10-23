/**
 * @file
 * @brief A simulated bus on a TCP port.
 *
 * The bridge listens on the loopback interface and carries everything
 * that crosses the bus to whatever connects, and everything that
 * connection sends back onto the bus. What is on the other end is not
 * looper's business: a terminal, a tool of your own, a second
 * simulation, a test in another language.
 *
 * ```c
 * #include "looper_tcp.h"
 *
 * looper_tcp_uart(BOARD_UART_BUS, 0u, "Console", 46100u);
 * looper_tcp_can(BOARD_CAN_BUS, 0x700u, "CAN gateway", 46101u);
 * ```
 *
 * Then, from a shell:
 *
 * ```console
 * $ nc 127.0.0.1 46100
 * ```
 *
 * A UART bridge carries bytes as they are. A CAN bridge carries frames
 * in the layout `looper_can_wire.h` writes down, because a stream has
 * no frames of its own to borrow.
 *
 * One connection at a time, and a new one replaces the old: a terminal
 * that was closed and opened again is the ordinary case, and refusing
 * it would leave the bridge held by something that is gone.
 */
#ifndef LOOPER_TCP_H
#define LOOPER_TCP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Carry a UART port over a TCP port.
 * @param bus Bus the port belongs to.
 * @param port Port to bridge.
 * @param label Name a client lists the bridge under.
 * @param listen_port TCP port to listen on.
 * @retval true The bridge is listening and attached.
 */
bool looper_tcp_uart(uint32_t bus, uint8_t port, const char *label, uint16_t listen_port);

/**
 * @brief Carry a CAN bus over a TCP port.
 * @param bus Bus to bridge.
 * @param base_id Identifier the bridge is listed under; it filters nothing.
 * @param label Name a client lists the bridge under.
 * @param listen_port TCP port to listen on.
 * @retval true The bridge is listening and attached.
 */
bool looper_tcp_can(uint32_t bus, uint32_t base_id, const char *label, uint16_t listen_port);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_TCP_H */
