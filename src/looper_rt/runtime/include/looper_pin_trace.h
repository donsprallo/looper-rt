/**
 * @file
 * @brief Recorded history of when the pins changed level.
 *
 * The pin model holds what every pin is now. That answers "is the chip
 * select asserted" and never answers the question a bus actually
 * raises: did it go low *before* the first clock. For that a client
 * needs the moments the levels changed, which is what this keeps.
 *
 * Changes only. A pin the firmware reads a thousand times without
 * touching has one entry, not a thousand, because a level that did not
 * change is not an event -- and because the whole recording is worth
 * keeping only if a slow signal is still in it after a fast one has
 * been busy.
 *
 * A client reads incrementally, as it does the analog trace: it
 * remembers the sequence number it last saw and asks for what came
 * after.
 */
#ifndef LOOPER_PIN_TRACE_H
#define LOOPER_PIN_TRACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifndef LOOPER_PIN_TRACE_CAPACITY
/**
 * @brief Level changes kept before the oldest is discarded.
 *
 * Pins change far less often than a converter samples: a chip select
 * toggles twice per transfer and a status line perhaps once a second.
 * Four thousand changes is minutes of an ordinary board and a few
 * seconds of a busy one, in 64 KB.
 */
#define LOOPER_PIN_TRACE_CAPACITY 4096u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Discard every recorded change and restart the numbering. */
void looper_pin_trace_clear(void);

/**
 * @brief Record that a pin took a level, if it is not the one it had.
 *
 * @param pin Pin that changed.
 * @param level Level it took.
 * @param previous Level it had before.
 * @retval true Something was recorded.
 * @retval false The level did not change, so there was nothing to say.
 */
bool looper_pin_trace_record(
    uint32_t pin, looper_signal_level_t level, looper_signal_level_t previous
);

/** @brief Number of changes currently held. */
size_t looper_pin_trace_count(void);

/**
 * @brief Read one held change by position, oldest first.
 * @return The change, or NULL when the index is out of range.
 */
const looper_pin_change_t *looper_pin_trace_at(size_t index);

/** @brief Sequence number the next recorded change will carry. */
uint32_t looper_pin_trace_next_sequence(void);

/**
 * @brief Copy the changes from a sequence number onwards.
 * @param after Sequence number the client has already seen.
 * @param destination Destination array.
 * @param capacity Number of changes the destination holds.
 * @return Number of changes copied.
 */
size_t looper_pin_trace_after(
    uint32_t after, looper_pin_change_t *destination, size_t capacity
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_PIN_TRACE_H */
