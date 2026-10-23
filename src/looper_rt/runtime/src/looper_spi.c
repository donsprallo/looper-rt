#include "looper_spi.h"

#include <string.h>

#include "looper_bus.h"

static looper_bus_address_t address_of(uint8_t chip_select)
{
    looper_bus_address_t address = {.value = chip_select, .extended = false};

    return address;
}

bool looper_spi_attach(
    uint32_t bus,
    uint8_t chip_select,
    const char *label,
    const char *model,
    void *instance,
    const looper_spi_operations_t *operations,
    void (*run)(void *instance, uint64_t microseconds)
)
{
    if ((operations == NULL) || (operations->transfer == NULL)) {
        return false;
    }
    if (looper_bus_kind_of(bus) != LOOPER_BUS_KIND_SPI) {
        return false;
    }
    return looper_component_attach(
        bus, address_of(chip_select), label, model, instance, operations, run
    );
}

bool looper_spi_transfer(
    uint32_t bus, uint8_t chip_select, const uint8_t *tx, uint8_t *rx, size_t length
)
{
    const looper_bus_address_t address = address_of(chip_select);
    const looper_component_t *component = looper_component_at_address(bus, address);
    const looper_spi_operations_t *operations;
    bool answered;

    if ((tx == NULL) || (rx == NULL)) {
        return false;
    }
    /*
     * A bus with nothing on the selected line clocks ones or zeroes back,
     * depending on what holds the line. Zeroes are the choice here because
     * the alternative, leaving the buffer as it was, would show the caller
     * the previous answer and look like a device that is still there.
     */
    memset(rx, 0, length);
    if (component == NULL) {
        looper_bus_record(
            bus, LOOPER_BUS_DIRECTION_OUTBOUND, address, tx, length, rx, length, false
        );
        return false;
    }
    operations = component->operations;
    answered = operations->transfer(component->instance, tx, rx, length);
    looper_bus_record(
        bus, LOOPER_BUS_DIRECTION_OUTBOUND, address, tx, length, rx, length, answered
    );
    return answered;
}
