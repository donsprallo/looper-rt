#include "looper_i2c.h"

#include <string.h>

#include "looper_bus.h"

/** @brief Widest address a seven bit I2C bus has. */
#define MAXIMUM_I2C_ADDRESS 0x7Fu

static looper_bus_address_t address_of(uint8_t address)
{
    looper_bus_address_t resolved = {.value = address, .extended = false};

    return resolved;
}

bool looper_i2c_attach(
    uint32_t bus,
    uint8_t address,
    const char *label,
    const char *model,
    void *instance,
    const looper_i2c_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
)
{
    if ((operations == NULL) || (operations->exchange == NULL)) {
        return false;
    }
    if ((address > MAXIMUM_I2C_ADDRESS) || (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_I2C)) {
        return false;
    }
    return looper_component_attach(
        bus, address_of(address), label, model, instance, operations, run
    );
}

bool looper_i2c_exchange(
    uint32_t bus,
    uint8_t address,
    const uint8_t *tx,
    size_t tx_length,
    uint8_t *rx,
    size_t rx_length
)
{
    const looper_bus_address_t resolved = address_of(address);
    const looper_component_t *component = looper_component_at_address(bus, resolved);
    const looper_i2c_operations_t *operations;
    bool acknowledged;

    if (rx != NULL) {
        memset(rx, 0, rx_length);
    }
    if (component == NULL) {
        looper_bus_record(
            bus, LOOPER_BUS_DIRECTION_OUTBOUND, resolved, tx, tx_length, rx, rx_length, false
        );
        return false;
    }
    operations = component->operations;
    acknowledged = operations->exchange(component->instance, tx, tx_length, rx, rx_length);
    looper_bus_record(
        bus,
        LOOPER_BUS_DIRECTION_OUTBOUND,
        resolved,
        tx,
        tx_length,
        rx,
        rx_length,
        acknowledged
    );
    return acknowledged;
}

/** @brief Longest register write this assembles in one go. */
#define REGISTER_WRITE_CAPACITY 64u

bool looper_i2c_write_register(
    uint32_t bus, uint8_t address, uint8_t reg, const uint8_t *data, size_t length
)
{
    uint8_t buffer[REGISTER_WRITE_CAPACITY];
    size_t carried = length;

    if (carried > (sizeof(buffer) - 1u)) {
        carried = sizeof(buffer) - 1u;
    }
    buffer[0] = reg;
    if ((data != NULL) && (carried > 0u)) {
        memcpy(&buffer[1], data, carried);
    }
    return looper_i2c_exchange(bus, address, buffer, carried + 1u, NULL, 0u);
}

bool looper_i2c_read_register(
    uint32_t bus, uint8_t address, uint8_t reg, uint8_t *data, size_t length
)
{
    return looper_i2c_exchange(bus, address, &reg, sizeof(reg), data, length);
}
