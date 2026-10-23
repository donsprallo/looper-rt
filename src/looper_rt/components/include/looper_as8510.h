/**
 * @file
 * @brief An AS8510 current and voltage front end on an SPI bus.
 *
 * One of the components looper ships. It is the chip and nothing around it:
 * the register file the firmware reads and writes, the amplifier in front
 * of the modulator, and a conversion every interval the decimation
 * registers ask for.
 *
 * What a count then means -- amperes, volts at a battery terminal -- is a
 * property of the probe and the calibration, which belong to the firmware
 * being tested. So what this component is given is what the converter
 * actually sees: a voltage at each of its two inputs.
 *
 * Nothing here knows about a particular board. The reference voltage is
 * configuration, the two inputs are published as channels a client drives,
 * and the ready interrupt is raised through a callback: how an interrupt
 * reaches the firmware is the project's business, not the chip's.
 */
#ifndef LOOPER_AS8510_H
#define LOOPER_AS8510_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of registers the device presents. */
#define LOOPER_AS8510_REGISTER_COUNT 128u

/** @brief Which input a read is asking about. */
typedef enum {
    LOOPER_AS8510_INPUT_CURRENT = 0,
    LOOPER_AS8510_INPUT_VOLTAGE = 1
} looper_as8510_input_t;

/** @brief What one AS8510 needs to know about the board it is on. */
typedef struct {
    /** @brief Reference voltage the converter measures against. */
    double reference_voltage_v;
    /**
     * @brief Conversion rate before the second decimation stage divides it.
     *
     * The modulator's rate depends on the clock the board gives the device
     * and on the oversampling the firmware selects, so it is configuration
     * rather than a property of the chip. Zero means the usual 8 kHz.
     */
    double conversion_base_rate_hz;
    /**
     * @brief Channel number the current input is published as.
     *
     * The voltage input takes the one after it. Both are published when
     * the device is attached, so a client drives what the converter sees
     * rather than what the firmware makes of it.
     */
    uint32_t first_property_channel;
    /**
     * @brief Say that a conversion has finished.
     *
     * The board decides what that means. On a real one it is an interrupt
     * line into the microcontroller; in a simulation it is whatever the
     * project wired that line to.
     */
    void (*on_conversion_ready)(void *context);
    /** @brief Passed back to both callbacks. */
    void *context;
} looper_as8510_config_t;

/** @brief One AS8510. Treat the contents as private. */
typedef struct {
    looper_as8510_config_t config;
    uint8_t registers[LOOPER_AS8510_REGISTER_COUNT];
    uint64_t next_conversion_us;
} looper_as8510_t;

/** @brief The voltage at one input, in volts, as a client has set it. */
double looper_as8510_input(const looper_as8510_t *self, looper_as8510_input_t input);

/**
 * @brief Prepare one AS8510 in its power on state.
 * @retval true The configuration is usable.
 */
bool looper_as8510_init(looper_as8510_t *self, const looper_as8510_config_t *config);

/**
 * @brief Attach it to an SPI bus.
 * @param bus Bus to attach to, which must be registered as an SPI bus.
 * @param chip_select Line that picks this device.
 * @param label Readable name, or NULL for the model name.
 * @retval true The device is attached.
 */
bool looper_as8510_attach(
    looper_as8510_t *self, uint32_t bus, uint8_t chip_select, const char *label
);

/** @brief Whether the firmware has started the converter. */
bool looper_as8510_is_running(const looper_as8510_t *self);

/** @brief The interval between conversions, in microseconds, or zero. */
uint64_t looper_as8510_interval_us(const looper_as8510_t *self);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_AS8510_H */
