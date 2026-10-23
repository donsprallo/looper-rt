/*
 * An AS8510 current and voltage front end.
 *
 * The firmware reaches the device over SPI with a one byte command followed
 * by data. The command carries the register address in its low seven bits
 * and the direction in bit seven: set to read, clear to write. The address
 * auto increments across the remaining bytes, which is how a driver reads a
 * whole measurement frame in one transfer.
 *
 * Reads answer with what was written, which is what a write and verify
 * start up needs. On top of that the device converts: once the firmware has
 * set the run bit, a conversion happens every interval the decimation
 * registers ask for, the data registers are filled from the inputs, the
 * ready bits are set and the board is told.
 *
 * Which gain and which rate are in force is read out of the registers the
 * firmware itself wrote, so the model follows the configuration rather than
 * assuming one, and an autorange moves the model the way it moves the chip.
 */
#include "looper_as8510.h"

#include <string.h>

#include "looper_analog.h"
#include "looper_as8510_conversion.h"
#include "looper_bus.h"
#include "looper_spi.h"

#define MODEL_NAME "as8510"

#define ADDRESS_MASK 0x7Fu
#define READ_FLAG 0x80u
#define COMMAND_LENGTH 1u

/* Register addresses, as the data sheet lays the device out. */
#define REGISTER_STATUS 0x04u
#define REGISTER_MOD_CTL 0x0Au

/* Bit positions within those registers, from the least significant bit. */
#define MOD_CTL_RUN_BIT 0u




static bool bit_of(uint8_t value, uint8_t position)
{
    return ((value >> position) & 1u) != 0u;
}

double looper_as8510_input(const looper_as8510_t *self, looper_as8510_input_t input)
{
    const uint32_t offset = (input == LOOPER_AS8510_INPUT_VOLTAGE) ? 1u : 0u;

    return looper_analog_read(self->config.first_property_channel + offset);
}

bool looper_as8510_init(looper_as8510_t *self, const looper_as8510_config_t *config)
{
    if ((self == NULL) || (config == NULL)) {
        return false;
    }
    if (!(config->reference_voltage_v > 0.0)) {
        return false;
    }
    memset(self, 0, sizeof(*self));
    self->config = *config;
    return true;
}

bool looper_as8510_is_running(const looper_as8510_t *self)
{
    return (self != NULL) && bit_of(self->registers[REGISTER_MOD_CTL], MOD_CTL_RUN_BIT);
}

static void run(void *instance, uint64_t microseconds)
{
    looper_as8510_t *self = instance;
    const uint64_t interval = looper_as8510_interval_us(self);

    if (!looper_as8510_is_running(self) || (interval == 0u)) {
        self->next_conversion_us = microseconds + interval;
        return;
    }
    if (microseconds < self->next_conversion_us) {
        return;
    }
    self->next_conversion_us = microseconds + interval;
    looper_as8510_convert(self);
    if (self->config.on_conversion_ready != NULL) {
        self->config.on_conversion_ready(self->config.context);
    }
}

/** @brief The register the n-th data byte of a transfer addresses. */
static uint8_t register_at(uint8_t first_address, size_t offset)
{
    return (uint8_t)((first_address + offset) & ADDRESS_MASK);
}

static void read_registers(
    looper_as8510_t *self, uint8_t first_address, uint8_t *rx, size_t data_length
)
{
    for (size_t index = 0u; index < data_length; index++) {
        const uint8_t address = register_at(first_address, index);

        rx[COMMAND_LENGTH + index] = self->registers[address];
        /*
         * Reading the status register clears what it reported. Leaving the
         * ready bits set would make every later frame look like a fresh
         * sample, and a driver would queue the same measurement again.
         */
        if (address == REGISTER_STATUS) {
            self->registers[REGISTER_STATUS] = 0u;
        }
    }
}

static void write_registers(
    looper_as8510_t *self, uint8_t first_address, const uint8_t *tx, size_t data_length
)
{
    for (size_t index = 0u; index < data_length; index++) {
        self->registers[register_at(first_address, index)] = tx[COMMAND_LENGTH + index];
    }
}

static bool transfer(void *instance, const uint8_t *tx, uint8_t *rx, size_t length)
{
    looper_as8510_t *self = instance;
    uint8_t command;
    size_t data_length;

    if ((tx == NULL) || (rx == NULL) || (length < COMMAND_LENGTH)) {
        return false;
    }
    command = tx[0];
    data_length = length - COMMAND_LENGTH;

    if ((command & READ_FLAG) == 0u) {
        write_registers(self, command & ADDRESS_MASK, tx, data_length);
        return true;
    }
    read_registers(self, command & ADDRESS_MASK, rx, data_length);
    return true;
}

static const looper_spi_operations_t operations = {.transfer = transfer};

/** @brief Publish the two inputs so a client can drive what the device sees. */
static void publish_inputs(const looper_as8510_t *self, uint32_t bus, uint8_t chip_select)
{
    const looper_bus_address_t address = {.value = chip_select, .extended = false};
    static const char *const names[] = {"I_SENSE", "V_SENSE"};

    for (uint32_t index = 0u; index < 2u; index++) {
        (void)looper_component_publish(
            bus,
            address,
            self->config.first_property_channel + index,
            names[index],
            "V",
            LOOPER_CHANNEL_DIRECTION_INPUT
        );
    }
}

bool looper_as8510_attach(
    looper_as8510_t *self, uint32_t bus, uint8_t chip_select, const char *label
)
{
    if (self == NULL) {
        return false;
    }
    if (!looper_spi_attach(
            bus,
            chip_select,
            (label != NULL) ? label : MODEL_NAME,
            MODEL_NAME,
            self,
            &operations,
            run
        )) {
        return false;
    }
    publish_inputs(self, bus, chip_select);
    return true;
}
