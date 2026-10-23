/**
 * @file
 * @brief How long the adapter calls took, and how often they were made.
 *
 * The journal says that a call happened and when, in the firmware's own
 * time. This says how long it took, in the host's: a running total, the
 * shortest, the longest and the count, kept per function rather than per
 * call so that eight thousand transfers a second cost four numbers.
 *
 * What that measures is worth being exact about. It is **host** time, not
 * the time the call would take on the target, and nothing here pretends
 * otherwise. What it is good for is the simulation itself -- which model
 * is expensive, which call is unexpectedly slow, where a loop iteration
 * goes -- and for noticing that a device model has started doing far more
 * work than it did yesterday.
 *
 * The firmware's own timing, the interval between two conversions say, is
 * a different question and the journal's simulated timestamps answer it.
 */
#ifndef LOOPER_TIMING_H
#define LOOPER_TIMING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LOOPER_TIMING_CAPACITY
/** @brief Number of distinct functions timings are kept for. */
#define LOOPER_TIMING_CAPACITY 96u
#endif

/**
 * @brief Bands one function's durations are counted in.
 *
 * Band `n` holds everything from 2^n nanoseconds up to 2^(n+1), so
 * thirty two of them reach from a nanosecond to four seconds. Powers
 * of two because the band of a duration is then the position of its
 * highest set bit, which costs no division and no comparison chain.
 */
#define LOOPER_TIMING_BANDS 32u

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What was measured of one function. */
typedef struct {
    const char *function;
    uint64_t calls;
    /**
     * @brief Calls something actually answered.
     *
     * A hook, or a binding the contract's role generated. The difference
     * between this and the count is the number of times the firmware was
     * handed the zero value and believed it, which is the one number a
     * simulation most needs to show.
     */
    uint64_t answered_calls;
    uint64_t total_nanoseconds;
    uint64_t shortest_nanoseconds;
    uint64_t longest_nanoseconds;
    /**
     * @brief How many calls fell into each band of duration.
     *
     * The shape of the thing, which no average can carry: one narrow
     * hump is a function that does one thing, two humps are two paths
     * through it, and a hump with a long tail is one that is usually
     * fine and occasionally blocks. Counting them costs one shift and
     * one increment per call and 128 bytes per function.
     */
    uint32_t bands[LOOPER_TIMING_BANDS];
} looper_timing_t;

/** @brief Forget every measurement. */
void looper_timing_clear(void);

/**
 * @brief Note that a call is starting.
 * @param function Name of the function, which is not copied.
 * @return The host time it started at, to hand back on return.
 */
uint64_t looper_timing_enter(const char *function);

/**
 * @brief Note that a call has returned.
 * @param function Name of the function, which is not copied.
 * @param started What `looper_timing_enter` returned.
 * @param answered Whether anything implemented the call.
 */
void looper_timing_leave(const char *function, uint64_t started, bool answered);

/** @brief Number of functions measurements are held for. */
size_t looper_timing_count(void);

/**
 * @brief Read one function's measurements by position.
 * @return The measurement, or NULL when the index is out of range.
 */
const looper_timing_t *looper_timing_at(size_t index);

/**
 * @brief Read one function's measurements by name.
 * @return The measurement, or NULL when the function was never called.
 */
const looper_timing_t *looper_timing_of(const char *function);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_TIMING_H */
