/*
 * What a signal graph must not be, checked before anything is replaced.
 */
#include "looper_signal_graph_check.h"

#include "looper_analog.h"
#include "looper_linear_system.h"

static bool is_operation(looper_signal_node_kind_t kind)
{
    return kind >= LOOPER_SIGNAL_NODE_ADD && kind <= LOOPER_SIGNAL_NODE_DIVIDE;
}

static const char *linear_fault(
    const looper_signal_graph_t *graph, const looper_signal_node_t *node, size_t position
)
{
    if (node->source >= graph->system_count) {
        return "a linear node without a system";
    }
    const looper_linear_system_t *system = &graph->systems[node->source];
    if (node->input_count != system->inputs || node->input_count > LOOPER_LINEAR_PORTS) {
        return "a linear node fed by a different number of inputs than its system takes";
    }
    for (uint32_t index = 0u; index < node->input_count; index++) {
        if (node->inputs[index] >= position) {
            return "a linear node whose input is not an earlier node";
        }
    }
    return looper_linear_fault(system);
}

static const char *pick_fault(
    const looper_signal_graph_t *graph, const looper_signal_node_t *node, size_t position
)
{
    if (node->left >= position || graph->nodes[node->left].kind != LOOPER_SIGNAL_NODE_LINEAR) {
        return "a pick from something that is not an earlier linear node";
    }
    const looper_signal_node_t *linear = &graph->nodes[node->left];
    if (linear->source >= graph->system_count
        || node->source >= graph->systems[linear->source].outputs) {
        return "a pick of an output its linear system does not have";
    }
    return NULL;
}

static const char *node_fault(const looper_signal_graph_t *graph, size_t position)
{
    const looper_signal_node_t *node = &graph->nodes[position];

    if (node->kind <= LOOPER_SIGNAL_NODE_UNSPECIFIED || node->kind > LOOPER_SIGNAL_NODE_PICK) {
        return "a node of a kind this simulation does not know";
    }
    if (is_operation(node->kind) && (node->left >= position || node->right >= position)) {
        return "an operation whose input is not an earlier node";
    }
    if (node->kind == LOOPER_SIGNAL_NODE_GENERATOR && node->signal.waveform > LOOPER_WAVEFORM_NOISE) {
        return "a generator with a waveform this simulation does not know";
    }
    if (node->kind == LOOPER_SIGNAL_NODE_LINEAR) {
        return linear_fault(graph, node, position);
    }
    if (node->kind == LOOPER_SIGNAL_NODE_PICK) {
        return pick_fault(graph, node, position);
    }
    return NULL;
}

static const char *size_fault(const looper_signal_graph_t *graph)
{
    if (graph->node_count > LOOPER_SIGNAL_GRAPH_CAPACITY) {
        return "more nodes than the simulation has room for";
    }
    if (graph->system_count > LOOPER_SIGNAL_GRAPH_SYSTEMS) {
        return "more linear systems than the simulation has room for";
    }
    if (graph->target_count > LOOPER_ANALOG_CAPACITY) {
        return "more targets than the simulation has inputs";
    }
    return NULL;
}

const char *looper_signal_graph_fault(const looper_signal_graph_t *graph)
{
    const char *fault = size_fault(graph);

    for (size_t position = 0u; fault == NULL && position < graph->node_count; position++) {
        fault = node_fault(graph, position);
    }
    for (size_t index = 0u; fault == NULL && index < graph->target_count; index++) {
        if (graph->targets[index].node >= graph->node_count) {
            fault = "a target driven by a node the graph does not have";
        } else if (!looper_analog_is_input(graph->targets[index].channel)) {
            fault = "a target that is not an analog input";
        }
    }
    return fault;
}
