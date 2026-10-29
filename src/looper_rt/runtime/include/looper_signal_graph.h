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

#ifndef LOOPER_SIGNAL_GRAPH_SYSTEMS
/** @brief Most linear systems one graph holds. */
#define LOOPER_SIGNAL_GRAPH_SYSTEMS 8u
#endif

#ifndef LOOPER_SIGNAL_GRAPH_MODELS
/** @brief Most state models one graph holds. */
#define LOOPER_SIGNAL_GRAPH_MODELS 8u
#endif

/** @brief The graph a request carries, as one argument. */
typedef struct {
    const looper_signal_node_t *nodes;
    size_t node_count;
    const looper_signal_target_t *targets;
    size_t target_count;
    const looper_linear_system_t *systems;
    size_t system_count;
    const looper_state_model_t *models;
    size_t model_count;
} looper_signal_graph_t;

/** @brief How one model node of the running graph is doing. */
typedef struct {
    /** Its position in the graph. */
    uint32_t node;
    /** Why it stopped, or NULL while it runs. */
    const char *fault;
    /** Its state now, as many values as it has. */
    const double *state;
    uint32_t state_count;
} looper_signal_node_status_t;

/**
 * @brief Replace the graph and the inputs it drives.
 *
 * Nothing is replaced when the graph is refused: the inputs keep what drove
 * them before. Inputs the old graph drove and the new one does not go back
 * to their own signal.
 *
 * A linear system at a position where the old graph had one of the same
 * size keeps its state. A graph is replaced whenever a parameter moves,
 * and a filter that forgot its state on every drag of a field would jump.
 * A state model starts from the state the request gives it instead: only
 * looper-core knows which of its values mean the same in the new model.
 *
 * Every model library is loaded and checked before anything is replaced,
 * so that a library that cannot be loaded leaves the old graph running.
 * The old graph's libraries are let go once the new graph is in place.
 *
 * @param graph Nodes in order -- a node names only earlier positions --
 *        the targets, and the linear systems the nodes refer to.
 * @param reason Receives why the graph was refused, when it is.
 * @retval true The graph is in place.
 * @retval false It was refused; @p reason says why.
 */
bool looper_signal_graph_set(const looper_signal_graph_t *graph, const char **reason);

/** @brief Forget the graph; every input goes back to its own signal. */
void looper_signal_graph_clear(void);

/**
 * @brief How every model node of the graph is doing, in the order of the graph.
 * @param statuses Receives one status per model node.
 * @param capacity How many there is room for.
 * @return How many were written.
 */
size_t looper_signal_graph_statuses(looper_signal_node_status_t *statuses, size_t capacity);

/**
 * @brief Put one model node back to the state it was installed with.
 *
 * Its clock starts again at the next read, and a node that had stopped on
 * a fault runs again.
 *
 * @param position The node, by its position in the graph.
 * @param reason Receives why it cannot be reset, when it cannot.
 * @retval true It was reset.
 * @retval false There is no model node at that position.
 */
bool looper_signal_graph_reset_model(uint32_t position, const char **reason);

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
