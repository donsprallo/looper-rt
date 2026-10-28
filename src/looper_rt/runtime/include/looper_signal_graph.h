/**
 * @file
 * @brief Signals built from other signals, evaluated where the firmware reads.
 *
 * A single generator per input answers "what does this sensor read". A
 * bench answers more: a supply that sags when the firmware switches a load,
 * a current that follows the duty cycle the firmware set, a sine with noise
 * on it. Those are signals made of other signals -- generators, what the
 * firmware itself drove, a pin, a variable -- added, subtracted, multiplied
 * and divided.
 *
 * The graph is evaluated here for the same reason a single signal is: a
 * client could only feed it in as fast as it polls. Each analog input the
 * graph drives is evaluated at the moment the firmware reads it.
 *
 * A node names its inputs by position, and only earlier positions. That is
 * the whole of the protection against cycles, and it is enough: a value
 * the firmware wrote is read as it was left, so feeding an output back into
 * an input is a source, not a loop.
 */
#ifndef LOOPER_SIGNAL_GRAPH_H
#define LOOPER_SIGNAL_GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_SIGNAL_GRAPH_CAPACITY
/** @brief Most nodes one graph holds. */
#define LOOPER_SIGNAL_GRAPH_CAPACITY 64u
#endif

/**
 * @brief Replace the graph and the inputs it drives.
 *
 * Nothing is replaced when the graph is refused: the inputs keep what drove
 * them before. Inputs the old graph drove and the new one does not go back
 * to their own signal.
 *
 * @param nodes Nodes in order; an operation names only earlier positions.
 * @param node_count How many there are, at most LOOPER_SIGNAL_GRAPH_CAPACITY.
 * @param targets Which node drives which analog input.
 * @param target_count How many there are.
 * @param reason Receives why the graph was refused, when it is.
 * @retval true The graph is in place.
 * @retval false It was refused; @p reason says why.
 */
bool looper_signal_graph_set(
    const looper_signal_node_t *nodes,
    size_t node_count,
    const looper_signal_target_t *targets,
    size_t target_count,
    const char **reason
);

/** @brief Forget the graph; every input goes back to its own signal. */
void looper_signal_graph_clear(void);

/**
 * @brief Take one input out of the graph, for a client that drives it alone.
 * @param channel The input.
 */
void looper_signal_graph_release(uint32_t channel);

/**
 * @brief The value the graph gives an input at a moment, if it drives it.
 * @param channel The input being read.
 * @param microseconds The simulated time of the read.
 * @param value Receives the value.
 * @retval true The graph drives the input and @p value holds what it gives.
 * @retval false It does not; the input's own signal applies.
 */
bool looper_signal_graph_value(uint32_t channel, uint64_t microseconds, double *value);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_SIGNAL_GRAPH_H */
