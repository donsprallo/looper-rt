/**
 * @file
 * @brief An MCP23x08 eight bit port expander on an I2C bus.
 *
 * One of the components looper ships. The firmware addresses it the way it
 * addresses any I2C device with a register file: write the register number,
 * then either more bytes to store or a read to fetch them. The pointer
 * advances across a multi byte access, which is how a driver reads several
 * registers in one go.
 *
 * What makes an expander worth shipping is the other side of it. Its eight
 * pins are pins, so they are registered in the pin model and appear beside
 * the microcontroller's own: an output the firmware drives through the
 * expander is visible, and an input can be driven from a client.
 */
#ifndef LOOPER_MCP23X08_H
#define LOOPER_MCP23X08_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of registers the device presents. */
#define LOOPER_MCP23X08_REGISTER_COUNT 11u

/** @brief Number of pins the device has. */
#define LOOPER_MCP23X08_PIN_COUNT 8u

/** @brief What one expander needs to know about the board it is on. */
typedef struct {
    /**
     * @brief Pin number the expander's pin zero is registered as.
     *
     * The seven after it follow. Choose a range the microcontroller's own
     * pins do not use, or the two will be the same pin in the pin model.
     */
    uint32_t first_pin;
    /** @brief Prefix for the pin labels, or NULL for the model name. */
    const char *pin_prefix;
} looper_mcp23x08_config_t;

/** @brief One MCP23x08. Treat the contents as private. */
typedef struct {
    looper_mcp23x08_config_t config;
    uint8_t registers[LOOPER_MCP23X08_REGISTER_COUNT];
    uint8_t pointer;
    bool pointer_set;
} looper_mcp23x08_t;

/**
 * @brief Prepare one expander in its power on state, with every pin an input.
 * @retval true The configuration is usable.
 */
bool looper_mcp23x08_init(looper_mcp23x08_t *self, const looper_mcp23x08_config_t *config);

/**
 * @brief Attach it to an I2C bus and register its pins.
 * @param address Seven bit device address, which the A0..A2 strapping sets.
 * @param label Readable name, or NULL for the model name.
 * @retval true The device is attached.
 */
bool looper_mcp23x08_attach(
    looper_mcp23x08_t *self, uint32_t bus, uint8_t address, const char *label
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_MCP23X08_H */
