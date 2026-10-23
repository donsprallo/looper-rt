/**
 * @file
 * @brief Converting between firmware memory and the double the trace holds.
 *
 * Private to the watch module. Split out because the conversion is the part
 * that has to be right about widths, signs and bit positions, and keeping it
 * apart from the registry lets each be read on its own.
 *
 * A bit width of zero means the value is the whole of the bytes at the
 * address. Anything else is a bit field, read by shifting the bytes down by
 * the offset and masking the width out of them.
 */
#ifndef LOOPER_WATCH_MEMORY_H
#define LOOPER_WATCH_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#include "looper_types.h"

/** @brief Description of where a value sits and what its bytes mean. */
typedef struct {
    uint64_t address;
    looper_encoding_t encoding;
    uint32_t byte_width;
    uint32_t bit_offset;
    uint32_t bit_width;
} looper_watch_location_t;

/**
 * @brief Whether a location describes something that can be read.
 * @param location Where the value sits and what its bytes mean.
 */
bool looper_watch_location_valid(const looper_watch_location_t *location);

/**
 * @brief Whether a value fits the range the location can hold.
 * @param location Where the value sits and what its bytes mean.
 * @param value Value a client wants to write.
 */
bool looper_watch_value_fits(const looper_watch_location_t *location, double value);

/**
 * @brief Read a variable out of firmware memory.
 * @param location Where the value sits and what its bytes mean.
 * @return The value as a double.
 */
double looper_watch_load(const looper_watch_location_t *location);

/**
 * @brief Write a variable into firmware memory.
 *
 * Writing a bit field leaves every bit around it as it was, which is what
 * the C assignment this stands in for would do.
 *
 * @param location Where the value sits and what its bytes mean.
 * @param value Value to store, truncated towards zero for an integer.
 */
void looper_watch_store(const looper_watch_location_t *location, double value);

#endif /* LOOPER_WATCH_MEMORY_H */
