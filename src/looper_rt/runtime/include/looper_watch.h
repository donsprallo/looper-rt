/**
 * @file
 * @brief Firmware variables the simulation samples into the trace.
 *
 * An analog channel is a value the firmware asks for. A watch is a value the
 * firmware never offered: a variable somewhere in its memory that a client
 * decided to look at. The client finds the address in the debug information
 * of the build; the simulation reads it.
 *
 * Sampling happens here rather than in the client for the same reason signals
 * are evaluated here. A client can only poll as fast as the network allows,
 * and would see whatever happened to fall on a poll. A watch is read once per
 * firmware loop iteration, so it has the resolution of the firmware.
 *
 * Only changes are recorded. A sample on an analog channel means the firmware
 * looked at it, which is worth knowing even when the value repeats. A watch
 * is read because the simulation polls, so recording an unchanged value would
 * record the polling rather than the firmware.
 */
#ifndef LOOPER_WATCH_H
#define LOOPER_WATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_WATCH_CAPACITY
/** @brief Maximum number of variables watched at once. */
#define LOOPER_WATCH_CAPACITY 64u
#endif

/**
 * @brief First trace channel number a watch is given.
 *
 * Watches share the trace with the analog channels the firmware registered,
 * so their numbers must not collide. Firmware channel numbers come from the
 * firmware and are small; starting watches at the top of the range keeps the
 * two apart without either having to know about the other.
 */
#define LOOPER_WATCH_CHANNEL_BASE 0x80000000u

/** @brief Forget every watch. Recorded samples are left alone. */
void looper_watch_clear(void);

/**
 * @brief Start sampling a firmware variable.
 *
 * @param path Name the client addresses the variable by, for display.
 * @param address Address in the running process, already relocated.
 * @param encoding What the bytes at that address mean.
 * @param byte_width Bytes to read at that address.
 * @param bit_offset First bit of the value within those bytes, for a bit
 *        field.
 * @param bit_width Bits the value occupies, or zero when it is the whole of
 *        the bytes at the address.
 * @param writable Whether a client may write it.
 * @param watch Receives the new watch, carrying its trace channel.
 * @retval true The variable is being sampled.
 * @retval false The description is not usable, the address is outside the
 *         running image, or no watch is free.
 */
bool looper_watch_add(
    const char *path,
    uint64_t address,
    looper_encoding_t encoding,
    uint32_t byte_width,
    uint32_t bit_offset,
    uint32_t bit_width,
    bool writable,
    const looper_watch_t **watch
);

/**
 * @brief Stop sampling a variable.
 * @param channel Trace channel of the watch to remove.
 * @retval true The watch was removed.
 * @retval false No watch holds that channel.
 */
bool looper_watch_remove(uint32_t channel);

/**
 * @brief Write a value into a watched variable.
 *
 * A value is converted to the variable's encoding, which for an integer
 * means truncation towards zero, as the C assignment this stands in for
 * would do.
 *
 * @param channel Trace channel of the watch to write through.
 * @param value Value to store.
 * @retval true The variable was written.
 * @retval false No watch holds that channel, the variable is read only, or
 *         the value is outside the range it can hold.
 */
bool looper_watch_write(uint32_t channel, double value);

/**
 * @brief Read every watched variable, recording the ones that changed.
 *
 * Called once per firmware loop iteration by the control server, which is
 * what gives a watch the resolution of the firmware.
 */
void looper_watch_poll(void);

/** @brief Number of variables currently watched. */
size_t looper_watch_count(void);

/**
 * @brief Read one watch by position.
 * @return The watch, or NULL when the index is out of range.
 */
const looper_watch_t *looper_watch_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_WATCH_H */
