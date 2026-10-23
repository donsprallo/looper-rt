/**
 * @file
 * @brief Which bus and which address an adapter instance stands for.
 *
 * A HAL does not say where a device is. It hands its adapter a context
 * pointer -- an SPI instance, an I2C handle, a UART port object -- and that
 * pointer is the only thing distinguishing one device from another. A
 * generated binding therefore has to turn a pointer into a bus and an
 * address, and only the project knows how.
 *
 * It says so once per device:
 *
 *     looper_binding_bind(&g_spi_device, BOARD_SPI_BUS, BOARD_AS8510_CS);
 *
 * A board with one bus of a kind and one device on it need not say even
 * that: an instance nobody bound resolves to the only bus of the kind the
 * caller asked for, and to the only component on it. That is a guess, but
 * a narrow one -- it is wrong only where a board has two of something, and
 * there the bind is one line.
 */
#ifndef LOOPER_BINDING_H
#define LOOPER_BINDING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifndef LOOPER_BINDING_CAPACITY
/** @brief Number of adapter instances the model can place. */
#define LOOPER_BINDING_CAPACITY 16u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Forget every placement. */
void looper_binding_clear(void);

/**
 * @brief Say where an adapter instance sits.
 *
 * @param instance Context pointer the HAL passes its adapter.
 * @param bus Bus it is on.
 * @param address Chip select, device address or port it answers at.
 * @retval true The placement is recorded.
 * @retval false The model is full.
 */
bool looper_binding_bind(const void *instance, uint32_t bus, uint32_t address);

/**
 * @brief Work out where an adapter instance sits.
 *
 * Used by generated bindings. An instance that was bound resolves to what
 * it was bound to; one that was not resolves to the only bus of the given
 * kind and the only component on it, which is what a board with one of
 * something means without having to say it.
 *
 * @param instance Context pointer, which may be NULL.
 * @param kind Kind of bus the caller is about to use.
 * @param bus Receives the bus number.
 * @param address Receives the address.
 * @retval true A place was found.
 * @retval false Nothing was bound and there is not exactly one such bus.
 */
bool looper_binding_resolve(
    const void *instance, looper_bus_kind_t kind, uint32_t *bus, uint32_t *address
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_BINDING_H */
