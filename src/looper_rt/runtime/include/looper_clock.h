/**
 * @file
 * @brief The simulated firmware clock.
 *
 * Time has two sources. In @ref LOOPER_TIME_MODE_VIRTUAL the clock moves
 * only when the firmware waits -- and every pass of the firmware loop waits
 * for its idle time -- or a client advances it, which makes a run
 * deterministic and independent of host load. A tool that controls the
 * simulation also holds the firmware still between the times it lets pass,
 * so that the clock stands still while the tool looks. In
 * @ref LOOPER_TIME_MODE_REAL the clock follows the host monotonic timer, which
 * is what interactive inspection needs.
 */
#ifndef LOOPER_CLOCK_H
#define LOOPER_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the clock in the requested mode and reset it to zero.
 * @param mode Time source to use for the rest of the run.
 */
void looper_clock_init(looper_time_mode_t mode);

/** @brief Report the configured time source. */
looper_time_mode_t looper_clock_mode(void);

/** @brief Read elapsed firmware time in microseconds. */
uint64_t looper_clock_microseconds(void);

/** @brief Read elapsed firmware time in whole milliseconds. */
uint32_t looper_clock_milliseconds(void);

/**
 * @brief Advance the virtual clock.
 * @param microseconds Duration to add.
 * @retval true The clock advanced.
 * @retval false The clock runs in real time and cannot be advanced.
 */
bool looper_clock_advance(uint64_t microseconds);

/**
 * @brief Wait for a duration using the configured time source.
 *
 * In virtual mode the clock jumps forward and the call returns at once. In
 * real time mode the calling thread sleeps.
 *
 * @param microseconds Duration to wait.
 */
void looper_clock_wait(uint64_t microseconds);

/**
 * @brief Let the host rest for a while, whatever the time source.
 *
 * The clock does not move. For a loop that has nothing to do and should not
 * spin a host core while it waits for a client.
 *
 * @param microseconds How long to rest on the host.
 */
void looper_clock_rest(uint64_t microseconds);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_CLOCK_H */
