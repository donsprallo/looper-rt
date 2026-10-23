/**
 * @file
 * @brief SPI buses and the components on them.
 *
 * An SPI exchange clocks both ways at once: the firmware sends a run of
 * bytes and receives the same number back from whichever device its chip
 * select picked. That is the whole of the contract, and a component has to
 * answer it while the firmware is inside the call.
 *
 * Which chip select a transfer belongs to is not something the generated
 * adapter can say, because a HAL identifies its device by a context pointer
 * rather than by a line. The project's binding resolves that and calls
 * `looper_spi_transfer` with the bus and the chip select it stands for.
 */
#ifndef LOOPER_SPI_H
#define LOOPER_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What a device on an SPI bus has to be able to do. */
typedef struct {
    /**
     * @brief Answer one transfer.
     * @param instance Component state.
     * @param tx Bytes the firmware clocks out.
     * @param rx Bytes clocked back, one per byte sent.
     * @param length Transfer length.
     * @retval true The device answered.
     */
    bool (*transfer)(void *instance, const uint8_t *tx, uint8_t *rx, size_t length);
} looper_spi_operations_t;

/**
 * @brief Attach a device to an SPI bus.
 *
 * @param bus Bus to attach to, which must already be registered.
 * @param chip_select Line that picks this device.
 * @param label Readable name.
 * @param model What is being simulated, for example "as8510".
 * @param instance Component state, owned by the caller.
 * @param operations Vtable, owned by the caller.
 * @param run Called once per loop iteration, or NULL when not needed.
 * @retval true The device is attached.
 */
bool looper_spi_attach(
    uint32_t bus,
    uint8_t chip_select,
    const char *label,
    const char *model,
    void *instance,
    const looper_spi_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
);

/**
 * @brief Clock one transfer to the device a chip select picks.
 *
 * The exchange is recorded either way. A transfer no device answers is
 * recorded as refused and leaves the receive buffer zeroed, which is what
 * a bus with nothing on it does.
 *
 * @retval true A device answered.
 */
bool looper_spi_transfer(
    uint32_t bus, uint8_t chip_select, const uint8_t *tx, uint8_t *rx, size_t length
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_SPI_H */
