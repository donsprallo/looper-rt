/**
 * @file
 * @brief Analog channels and the signals that drive them.
 *
 * A pin is high or low. A measurement input is a number that moves, and a
 * firmware that measures needs something to measure. An analog channel is one
 * such value: the firmware reads it or writes it, and a client either watches
 * it or drives it with a generated signal.
 *
 * Signals are evaluated inside the simulation rather than pushed in from
 * outside. A client that sent samples over the control channel could only send
 * them as fast as it polls, and the firmware would see a staircase. Here the
 * client sets a definition once and every firmware read evaluates it against
 * the simulated clock, so the signal has the resolution of the firmware rather
 * than of the network.
 */
#ifndef LOOPER_ANALOG_H
#define LOOPER_ANALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_ANALOG_CAPACITY
/** @brief Maximum number of analog channels the model holds. */
#define LOOPER_ANALOG_CAPACITY 32u
#endif

/** @brief Forget every registered channel. */
void looper_analog_clear(void);

/**
 * @brief Register a channel so that clients can see and address it.
 *
 * Registering a channel that is already known updates its description but
 * keeps its current value and signal.
 *
 * @param channel Opaque channel number used by the firmware.
 * @param label Readable name, truncated to LOOPER_LABEL_CAPACITY - 1 bytes.
 * @param unit Unit shown next to the value, for example "A" or "mV".
 * @param direction Whether the firmware reads or writes the channel.
 * @retval true The channel is registered.
 * @retval false The model is full.
 */
bool looper_analog_register(
    uint32_t channel,
    const char *label,
    const char *unit,
    looper_channel_direction_t direction
);

/**
 * @brief Install the signal that drives a channel the firmware reads.
 * @param channel Channel to drive.
 * @param signal Signal definition to evaluate on every read.
 * @retval true The signal was installed.
 * @retval false The channel is unknown or is an output.
 */
bool looper_analog_set_signal(uint32_t channel, const looper_signal_t *signal);

/**
 * @brief Whether a channel is known and the firmware reads it.
 * @param channel Channel to look at.
 */
bool looper_analog_is_input(uint32_t channel);

/**
 * @brief Say whether the signal graph drives a channel, for its listing.
 * @param channel Channel the graph took or let go of.
 * @param driven Whether the graph drives it now.
 */
void looper_analog_set_graph_driven(uint32_t channel, bool driven);

/**
 * @brief Read a channel, evaluating its signal at the current simulated time.
 *
 * An input the signal graph drives takes its value from the graph instead.
 *
 * The returned value is recorded in the trace, so a client sees exactly what
 * the firmware saw, at the moment the firmware looked.
 *
 * @param channel Channel to read.
 * @return The value, or zero when the channel is unknown.
 */
double looper_analog_read(uint32_t channel);

/**
 * @brief Write a channel the firmware drives, recording it in the trace.
 * @param channel Channel to write.
 * @param value Value the firmware produced.
 * @retval true The value was stored.
 * @retval false The channel is unknown.
 */
bool looper_analog_write(uint32_t channel, double value);

/**
 * @brief Evaluate a signal definition at a point in time.
 *
 * Noise is the exception to "at a point in time": it draws a new value on
 * every call, from a generator that starts the same way in every run.
 * @param signal Definition to evaluate.
 * @param microseconds Time to evaluate it at.
 * @return The value of the signal.
 */
double looper_analog_evaluate(const looper_signal_t *signal, uint64_t microseconds);

/** @brief Number of registered channels. */
size_t looper_analog_count(void);

/**
 * @brief Read one registered channel by position.
 * @return The channel, or NULL when the index is out of range.
 */
const looper_channel_t *looper_analog_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_ANALOG_H */
