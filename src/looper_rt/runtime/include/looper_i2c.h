/**
 * @file
 * @brief I2C buses and the components on them.
 *
 * Every I2C transaction is the same shape underneath: the master addresses
 * a device, writes some bytes, then reads some bytes, and either half may
 * be empty. A plain write is a transaction with nothing read, a plain read
 * one with nothing written, and the register read a HAL usually offers is
 * both halves at once. So there is one operation rather than three, and a
 * component implements it once.
 */
#ifndef LOOPER_I2C_H
#define LOOPER_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What a device on an I2C bus has to be able to do. */
typedef struct {
    /**
     * @brief Answer one transaction.
     * @param instance Component state.
     * @param tx Bytes written to the device, may be NULL when nothing is.
     * @param tx_length Number of those bytes.
     * @param rx Bytes read back, may be NULL when nothing is read.
     * @param rx_length Number of bytes to read.
     * @retval true The device acknowledged.
     */
    bool (*exchange)(
        void *instance, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length
    );
} looper_i2c_operations_t;

/**
 * @brief Attach a device to an I2C bus.
 * @param address Seven bit device address.
 * @retval true The device is attached.
 */
bool looper_i2c_attach(
    uint32_t bus,
    uint8_t address,
    const char *label,
    const char *model,
    void *instance,
    const looper_i2c_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
);

/**
 * @brief Carry out one transaction with the device at an address.
 *
 * The transaction is recorded either way. One that nothing acknowledges is
 * recorded as refused and leaves the read buffer zeroed, which is what an
 * address with no device at it answers.
 *
 * @retval true A device acknowledged.
 */
bool looper_i2c_exchange(
    uint32_t bus,
    uint8_t address,
    const uint8_t *tx,
    size_t tx_length,
    uint8_t *rx,
    size_t rx_length
);

/**
 * @brief Write bytes into a device's register, which is what a HAL offers.
 *
 * The register number goes out first and the data after it, which is one
 * transaction rather than two. Here rather than in a caller because every
 * caller would otherwise assemble the same buffer.
 *
 * @retval true A device acknowledged.
 */
bool looper_i2c_write_register(
    uint32_t bus, uint8_t address, uint8_t reg, const uint8_t *data, size_t length
);

/**
 * @brief Read bytes out of a device's register.
 * @retval true A device acknowledged.
 */
bool looper_i2c_read_register(
    uint32_t bus, uint8_t address, uint8_t reg, uint8_t *data, size_t length
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_I2C_H */
