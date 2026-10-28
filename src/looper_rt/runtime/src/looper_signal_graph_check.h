/**
 * @file
 * @brief Whether a signal graph can be put in place.
 *
 * Private to the signal graph. Kept apart so that the graph reads as what it
 * does with a graph, and this as the list of what a graph must not be.
 */
#ifndef LOOPER_SIGNAL_GRAPH_CHECK_H
#define LOOPER_SIGNAL_GRAPH_CHECK_H

#include "looper_signal_graph.h"

/**
 * @brief Why a graph cannot be put in place, or NULL when it can.
 *
 * A node of an unknown kind, an input that is not an earlier position, a
 * linear system that does not fit or does not match its size, a pick from
 * something that is not a linear system, a target that is not an analog
 * input, or more of anything than there is room for.
 */
const char *looper_signal_graph_fault(const looper_signal_graph_t *graph);

#endif /* LOOPER_SIGNAL_GRAPH_CHECK_H */
