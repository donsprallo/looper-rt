/**
 * @file
 * @brief The simulated state of the firmware's general purpose IO pins.
 *
 * The adapter addresses a pin by an opaque number. A client needs a readable
 * name, so the firmware registers each pin it uses with a label once at
 * startup. Outputs are written by the firmware and read by the client; inputs
 * are the other way round.
 */
#ifndef LOOPER_GPIO_MODEL_H
#define LOOPER_GPIO_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_GPIO_CAPACITY
/** @brief Maximum number of pins the model holds. */
#define LOOPER_GPIO_CAPACITY 64u
#endif

/** @brief Forget every registered pin. */
void looper_gpio_clear(void);

/**
 * @brief Register a pin so that clients can see and address it.
 *
 * A newly registered pin starts at the level its polarity calls inactive, so
 * nothing reads as asserted before the firmware or a client sets it.
 * Registering a pin that is already known updates its label, direction and
 * polarity but keeps its current level.
 *
 * @param pin Opaque pin number used by the adapter.
 * @param label Readable name, truncated to LOOPER_LABEL_CAPACITY - 1 bytes.
 * @param direction Whether the firmware drives or reads the pin.
 * @param polarity Which level the firmware treats as active.
 * @retval true The pin is registered.
 * @retval false The model is full.
 */
bool looper_gpio_register(
    uint32_t pin,
    const char *label,
    looper_pin_direction_t direction,
    looper_active_polarity_t polarity
);

/**
 * @brief Set the level of a pin the firmware drives.
 * @retval true The level was applied.
 * @retval false The pin is unknown.
 */
bool looper_gpio_write(uint32_t pin, looper_signal_level_t level);

/**
 * @brief Apply a level to a pin the firmware reads.
 * @retval true The level was applied.
 * @retval false The pin is unknown or is an output.
 */
bool looper_gpio_drive_input(uint32_t pin, looper_signal_level_t level);

/**
 * @brief Read the current level of a pin.
 * @return The level, or LOOPER_SIGNAL_LEVEL_UNSPECIFIED when unknown.
 */
looper_signal_level_t looper_gpio_read(uint32_t pin);

/** @brief Number of registered pins. */
size_t looper_gpio_count(void);

/**
 * @brief Read one registered pin by position.
 * @return The pin, or NULL when the index is out of range.
 */
const looper_pin_t *looper_gpio_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_GPIO_MODEL_H */
