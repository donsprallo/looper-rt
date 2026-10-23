/**
 * @file
 * @brief Recorded history of analog channel values.
 *
 * The trace is what a plotting client draws. Samples are recorded by the
 * simulation whenever the firmware reads or writes a channel, so the history
 * has the resolution of the firmware rather than of the client polling it.
 *
 * A client reads incrementally: it remembers the sequence number it last saw
 * and asks for what came after, which keeps a continuous plot possible over a
 * request and response protocol.
 */
#ifndef LOOPER_TRACE_H
#define LOOPER_TRACE_H

#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_TRACE_CAPACITY
/**
 * @brief Samples kept before the oldest is discarded.
 *
 * Sized by how long a client may be away rather than by how many
 * points a plot shows. A firmware publishing sixteen thousand samples
 * a second fills four thousand of them in a quarter of a second,
 * which is less than one pass of a client that is also reading a
 * profile and a memory report -- and what overflows in between is
 * gone, which on a plot looks like a signal that cannot be drawn.
 *
 * Sixty four thousand is four seconds at that rate and two
 * megabytes. A simulation is a program on a desktop; the memory is
 * worth more as headroom than as a saving, and the static memory
 * report counts it apart from the firmware's own.
 */
#define LOOPER_TRACE_CAPACITY 65536u
#endif

/** @brief Discard every recorded sample and restart the numbering. */
void looper_trace_clear(void);

/**
 * @brief Record one value at the current simulated time.
 * @param channel Channel the value belongs to.
 * @param value Value that was read or written.
 */
void looper_trace_record(uint32_t channel, double value);

/** @brief Number of samples currently held. */
size_t looper_trace_count(void);

/**
 * @brief Read one held sample by position, oldest first.
 * @return The sample, or NULL when the index is out of range.
 */
const looper_sample_t *looper_trace_at(size_t index);

/**
 * @brief Copy the samples recorded after a sequence number.
 * @param after Sequence number the caller has already seen.
 * @param destination Buffer receiving the samples, oldest first.
 * @param capacity Maximum number of samples to copy.
 * @return The number of samples copied.
 * @note When more samples were dropped than the trace can hold, the caller
 *       silently receives the oldest still available rather than a gap error.
 */
size_t looper_trace_after(uint32_t after, looper_sample_t *destination, size_t capacity);

/** @brief Sequence number the next recorded sample will carry. */
uint32_t looper_trace_next_sequence(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_TRACE_H */
