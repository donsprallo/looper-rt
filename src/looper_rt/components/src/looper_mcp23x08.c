/*
 * An MCP23x08 eight bit port expander.
 *
 * The register file is the device. What is modelled on top of it is the one
 * thing a register file alone would not show: the pins. Each of the eight
 * is registered in the pin model, an output follows the output latch, and
 * an input reads whatever a client drives it to, which is what the firmware
 * then sees in the GPIO register.
 */
#include "looper_mcp23x08.h"

#include <stdio.h>
#include <string.h>

#include "looper_gpio_model.h"
#include "looper_i2c.h"

#define MODEL_NAME "mcp23x08"

/* Registers, in the order the device numbers them. */
#define REGISTER_IODIR 0x00u
#define REGISTER_GPIO 0x09u
#define REGISTER_OLAT 0x0Au

/** @brief Longest pin label this builds, "PREFIX_7" and a terminator. */
#define LABEL_CAPACITY 24u

static bool is_input(const looper_mcp23x08_t *self, uint8_t pin)
{
    return ((self->registers[REGISTER_IODIR] >> pin) & 1u) != 0u;
}

bool looper_mcp23x08_init(looper_mcp23x08_t *self, const looper_mcp23x08_config_t *config)
{
    if ((self == NULL) || (config == NULL)) {
        return false;
    }
    memset(self, 0, sizeof(*self));
    self->config = *config;
    /* Every pin is an input after a reset, which is what IODIR all ones means. */
    self->registers[REGISTER_IODIR] = 0xFFu;
    return true;
}

/** @brief Register the eight pins so a client sees them beside the others. */
static void register_pins(const looper_mcp23x08_t *self)
{
    const char *prefix = (self->config.pin_prefix != NULL) ? self->config.pin_prefix
                                                           : MODEL_NAME;

    for (uint8_t pin = 0u; pin < LOOPER_MCP23X08_PIN_COUNT; pin++) {
        char label[LABEL_CAPACITY];

        (void)snprintf(label, sizeof(label), "%s_%u", prefix, (unsigned)pin);
        looper_gpio_register(
            self->config.first_pin + pin,
            label,
            is_input(self, pin) ? LOOPER_PIN_DIRECTION_INPUT : LOOPER_PIN_DIRECTION_OUTPUT,
            LOOPER_ACTIVE_POLARITY_HIGH
        );
    }
}

/*
 * Carry the pins and the register file into line with each other.
 *
 * An output pin shows what the output latch holds; an input pin is whatever
 * a client drives it to, and that is what a read of GPIO reports. The
 * direction register can change at any time, so the pin model is told again
 * each pass rather than only at the start.
 */
static void run(void *instance, uint64_t microseconds)
{
    looper_mcp23x08_t *self = instance;
    uint8_t sampled = 0u;

    (void)microseconds;

    for (uint8_t pin = 0u; pin < LOOPER_MCP23X08_PIN_COUNT; pin++) {
        const uint32_t number = self->config.first_pin + pin;

        if (is_input(self, pin)) {
            if (looper_gpio_read(number) == LOOPER_SIGNAL_LEVEL_HIGH) {
                sampled |= (uint8_t)(1u << pin);
            }
            continue;
        }
        if (((self->registers[REGISTER_OLAT] >> pin) & 1u) != 0u) {
            sampled |= (uint8_t)(1u << pin);
            looper_gpio_write(number, LOOPER_SIGNAL_LEVEL_HIGH);
        } else {
            looper_gpio_write(number, LOOPER_SIGNAL_LEVEL_LOW);
        }
    }
    self->registers[REGISTER_GPIO] = sampled;
}

/** @brief Store one byte at the pointer, and advance it. */
static void write_byte(looper_mcp23x08_t *self, uint8_t value)
{
    if (self->pointer < LOOPER_MCP23X08_REGISTER_COUNT) {
        self->registers[self->pointer] = value;
        /* Writing the port writes the latch, which is what drives the pins. */
        if (self->pointer == REGISTER_GPIO) {
            self->registers[REGISTER_OLAT] = value;
        }
    }
    self->pointer++;
}

static uint8_t read_byte(looper_mcp23x08_t *self)
{
    uint8_t value = 0u;

    if (self->pointer < LOOPER_MCP23X08_REGISTER_COUNT) {
        value = self->registers[self->pointer];
    }
    self->pointer++;
    return value;
}

/*
 * One transaction.
 *
 * The first byte written is the register pointer, as the device defines it.
 * A read with nothing written carries on from where the pointer stands,
 * which is how a driver reads a block after setting the pointer once.
 */
static bool exchange(
    void *instance, const uint8_t *tx, size_t tx_length, uint8_t *rx, size_t rx_length
)
{
    looper_mcp23x08_t *self = instance;
    size_t index = 0u;

    if ((tx != NULL) && (tx_length > 0u)) {
        self->pointer = tx[0];
        self->pointer_set = true;
        for (index = 1u; index < tx_length; index++) {
            write_byte(self, tx[index]);
        }
        self->pointer = tx[0] + (uint8_t)(tx_length - 1u);
    }
    if (!self->pointer_set) {
        return false;
    }
    for (index = 0u; (rx != NULL) && (index < rx_length); index++) {
        rx[index] = read_byte(self);
    }
    return true;
}

static const looper_i2c_operations_t operations = {.exchange = exchange};

bool looper_mcp23x08_attach(
    looper_mcp23x08_t *self, uint32_t bus, uint8_t address, const char *label
)
{
    if (self == NULL) {
        return false;
    }
    if (!looper_i2c_attach(
            bus,
            address,
            (label != NULL) ? label : MODEL_NAME,
            MODEL_NAME,
            self,
            &operations,
            run
        )) {
        return false;
    }
    register_pins(self);
    return true;
}
