/**
 * @file
 * @brief Putting a simulated bus onto something outside the process.
 *
 * A component answers the firmware from inside the simulation. A bridge
 * is a component that does not answer at all: it carries what crosses a
 * bus out to something else -- a socket, and in principle anything that
 * can take bytes -- and carries back whatever that something says.
 *
 * What the far side then is, is somebody else's business. A terminal on
 * a UART, a tool watching CAN, a second simulation, a test harness in
 * another language. `looper_tcp.h` and `looper_udp.h` are the two that
 * ship; a third needs nothing but the two functions below.
 *
 * ## Only UART and CAN
 *
 * On SPI and I2C the firmware is inside the call, waiting for the
 * answer. Nothing outside the process can answer in that time, and a
 * bridge that waited would stop the firmware. UART and CAN have no such
 * rule: a peer there replies when it is ready, which is exactly what a
 * socket can do.
 */
#ifndef LOOPER_BRIDGE_H
#define LOOPER_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What a bridge needs of whatever it carries bytes over. */
typedef struct {
    /**
     * @brief Send bytes outward.
     *
     * Called from inside the firmware's call, so it must not wait. What
     * cannot be sent now is dropped: a bus has no memory of what the
     * wire did not take either.
     *
     * @param transport The transport's own state.
     * @retval false The far side is gone, which is not an error.
     */
    bool (*send)(void *transport, const uint8_t *data, size_t length);

    /**
     * @brief Take whatever has arrived, without waiting.
     *
     * Called once per pass of the firmware loop.
     *
     * @return Bytes taken, zero when none were waiting.
     */
    size_t (*receive)(void *transport, uint8_t *destination, size_t capacity);
} looper_transport_t;

/**
 * @brief Carry a UART port out over a transport.
 *
 * Bytes the firmware writes go out as they are; bytes that come back are
 * delivered to the port as if a device had sent them. No framing in
 * either direction, because a serial line has none.
 *
 * @param bus Bus the port belongs to.
 * @param port Port to bridge.
 * @param label Name a client lists the bridge under.
 * @param model What kind of bridge it is, for the same listing.
 * @param transport The transport's own state, which it keeps.
 * @param operations How to send and receive over it.
 * @retval true The bridge is attached.
 */
bool looper_bridge_uart(
    uint32_t bus,
    uint8_t port,
    const char *label,
    const char *model,
    void *transport,
    const looper_transport_t *operations
);

/**
 * @brief Carry a CAN bus out over a transport.
 *
 * Frames are written and read in the layout `looper_can_wire.h`
 * describes. A stream that falls out of step with that layout is
 * dropped rather than guessed at: the bridge resets its buffer once
 * more bytes have arrived than the longest frame can hold.
 *
 * @param bus Bus to bridge.
 * @param base_id Identifier the bridge is listed under. It filters
 *        nothing: a bridge sees every frame, the way a node does.
 * @retval true The bridge is attached.
 */
bool looper_bridge_can(
    uint32_t bus,
    uint32_t base_id,
    const char *label,
    const char *model,
    void *transport,
    const looper_transport_t *operations
);

/** @brief Forget every bridge, so that a new simulation starts clean. */
void looper_bridge_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_BRIDGE_H */
