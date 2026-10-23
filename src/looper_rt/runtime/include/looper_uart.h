/**
 * @file
 * @brief UART ports, what is wired to them, and what a client types in.
 *
 * A UART is not addressed the way the other buses are: there is one peer at
 * the other end of the wire, and the port number is the address. What makes
 * it different from SPI and I2C is that the peer may speak first, so a port
 * has a queue of bytes waiting for the firmware to read.
 *
 * Two things can fill that queue. A component wired to the port fills it by
 * answering what the firmware wrote, and a client fills it by sending to the
 * port, which is how a terminal at the other end is simulated. A port can
 * have both: a component that answers, and someone typing.
 */
#ifndef LOOPER_UART_H
#define LOOPER_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifndef LOOPER_UART_QUEUE_CAPACITY
/** @brief Bytes one port holds for the firmware before the oldest is lost. */
#define LOOPER_UART_QUEUE_CAPACITY 256u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What a device on a UART port has to be able to do. */
typedef struct {
    /**
     * @brief Take what the firmware wrote.
     *
     * A reply is given by calling looper_uart_deliver, not by returning
     * bytes: a peer that answers does so when it is ready, which is not
     * necessarily inside this call.
     *
     * @param instance Component state.
     * @param port Port the bytes were written to.
     * @param data Bytes the firmware wrote.
     * @param length Number of those bytes.
     */
    void (*receive)(void *instance, uint8_t port, const uint8_t *data, size_t length);
} looper_uart_operations_t;

/**
 * @brief Attach a peer to a UART port.
 * @param port Port the peer is wired to.
 * @retval true The peer is attached.
 */
bool looper_uart_attach(
    uint32_t bus,
    uint8_t port,
    const char *label,
    const char *model,
    void *instance,
    const looper_uart_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
);

/**
 * @brief Write bytes out of the firmware onto a port.
 *
 * Recorded either way. A port with nothing wired to it is recorded as
 * refused, which is what writing into an unconnected wire amounts to.
 *
 * @retval true A peer took the bytes.
 */
bool looper_uart_write(uint32_t bus, uint8_t port, const uint8_t *data, size_t length);

/**
 * @brief Put bytes on a port for the firmware to read.
 *
 * Called by a component that is answering, and by the control channel when
 * a client sends to the port. Recorded as inbound once the firmware reads
 * them rather than here, so the recording shows when the firmware saw them.
 *
 * @retval true The bytes are queued.
 * @retval false The port is unknown or its queue is full.
 */
bool looper_uart_deliver(uint32_t bus, uint8_t port, const uint8_t *data, size_t length);

/**
 * @brief Read what is waiting on a port.
 * @param destination Where to put the bytes.
 * @param capacity Most bytes to take.
 * @return Number of bytes taken, which is zero when nothing is waiting.
 */
size_t looper_uart_read(uint32_t bus, uint8_t port, uint8_t *destination, size_t capacity);

/** @brief Number of bytes waiting on a port. */
size_t looper_uart_pending(uint32_t bus, uint8_t port);

/** @brief Forget what is waiting on every port. */
void looper_uart_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_UART_H */
