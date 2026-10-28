/*
 * The signal graph: nodes in order, evaluated where an input is read.
 *
 * Each node keeps the value it last gave and the moment it gave it. Two
 * inputs read at the same moment through one node see one value -- which
 * matters for noise, where evaluating twice would give two -- and a node
 * shared by several paths is evaluated once per moment.
 */
#include "looper_signal_graph.h"

#include <string.h>

#include "looper_analog.h"
#include "looper_gpio_model.h"
#include "looper_watch.h"

/* One past any moment, so that a fresh node never looks evaluated. */
#define NEVER UINT64_MAX

static looper_signal_node_t nodes[LOOPER_SIGNAL_GRAPH_CAPACITY];
static size_t node_count;
static looper_signal_target_t targets[LOOPER_ANALOG_CAPACITY];
static size_t target_count;
static double values[LOOPER_SIGNAL_GRAPH_CAPACITY];
static uint64_t evaluated_at[LOOPER_SIGNAL_GRAPH_CAPACITY];

static bool is_operation(looper_signal_node_kind_t kind)
{
    return kind >= LOOPER_SIGNAL_NODE_ADD && kind <= LOOPER_SIGNAL_NODE_DIVIDE;
}

static const char *node_fault(const looper_signal_node_t *node, size_t position)
{
    if (node->kind <= LOOPER_SIGNAL_NODE_UNSPECIFIED || node->kind > LOOPER_SIGNAL_NODE_DIVIDE) {
        return "a node of a kind this simulation does not know";
    }
    if (is_operation(node->kind) && (node->left >= position || node->right >= position)) {
        return "an operation whose input is not an earlier node";
    }
    if (node->kind == LOOPER_SIGNAL_NODE_GENERATOR && node->signal.waveform > LOOPER_WAVEFORM_NOISE) {
        return "a generator with a waveform this simulation does not know";
    }
    return NULL;
}

static const char *graph_fault(
    const looper_signal_node_t *candidate,
    size_t candidate_count,
    const looper_signal_target_t *drives,
    size_t drive_count
)
{
    if (candidate_count > LOOPER_SIGNAL_GRAPH_CAPACITY) {
        return "more nodes than the simulation has room for";
    }
    if (drive_count > LOOPER_ANALOG_CAPACITY) {
        return "more targets than the simulation has inputs";
    }
    for (size_t position = 0u; position < candidate_count; position++) {
        const char *fault = node_fault(&candidate[position], position);
        if (fault != NULL) {
            return fault;
        }
    }
    for (size_t index = 0u; index < drive_count; index++) {
        if (drives[index].node >= candidate_count) {
            return "a target driven by a node the graph does not have";
        }
        if (!looper_analog_is_input(drives[index].channel)) {
            return "a target that is not an analog input";
        }
    }
    return NULL;
}

static void mark_targets(bool driven)
{
    for (size_t index = 0u; index < target_count; index++) {
        looper_analog_set_graph_driven(targets[index].channel, driven);
    }
}

bool looper_signal_graph_set(
    const looper_signal_node_t *candidate,
    size_t candidate_count,
    const looper_signal_target_t *drives,
    size_t drive_count,
    const char **reason
)
{
    const char *fault = graph_fault(candidate, candidate_count, drives, drive_count);

    if (fault != NULL) {
        if (reason != NULL) {
            *reason = fault;
        }
        return false;
    }
    looper_signal_graph_clear();
    if (candidate_count > 0u) {
        memcpy(nodes, candidate, candidate_count * sizeof(*candidate));
    }
    if (drive_count > 0u) {
        memcpy(targets, drives, drive_count * sizeof(*drives));
    }
    node_count = candidate_count;
    target_count = drive_count;
    mark_targets(true);
    return true;
}

void looper_signal_graph_clear(void)
{
    mark_targets(false);
    node_count = 0u;
    target_count = 0u;
    for (size_t index = 0u; index < LOOPER_SIGNAL_GRAPH_CAPACITY; index++) {
        evaluated_at[index] = NEVER;
    }
}

void looper_signal_graph_release(uint32_t channel)
{
    size_t kept = 0u;

    for (size_t index = 0u; index < target_count; index++) {
        if (targets[index].channel == channel) {
            looper_analog_set_graph_driven(channel, false);
            continue;
        }
        targets[kept] = targets[index];
        kept++;
    }
    target_count = kept;
}

static double channel_value(uint32_t channel)
{
    for (size_t index = 0u; index < looper_analog_count(); index++) {
        const looper_channel_t *entry = looper_analog_at(index);
        if (entry->id == channel) {
            return entry->value;
        }
    }
    return 0.0;
}

static double watch_value(uint32_t channel)
{
    for (size_t index = 0u; index < looper_watch_count(); index++) {
        const looper_watch_t *watch = looper_watch_at(index);
        if (watch->channel == channel) {
            return watch->value;
        }
    }
    return 0.0;
}

static double operate(looper_signal_node_kind_t kind, double left, double right)
{
    switch (kind) {
        case LOOPER_SIGNAL_NODE_ADD:
            return left + right;
        case LOOPER_SIGNAL_NODE_SUBTRACT:
            return left - right;
        case LOOPER_SIGNAL_NODE_MULTIPLY:
            return left * right;
        case LOOPER_SIGNAL_NODE_DIVIDE:
            /* A firmware fed infinity misbehaves in ways that hide the cause. */
            return (right == 0.0) ? 0.0 : left / right;
        default:
            return 0.0;
    }
}

static double evaluate(size_t position, uint64_t microseconds)
{
    const looper_signal_node_t *node = &nodes[position];
    double value = 0.0;

    if (position >= node_count) {
        return 0.0;
    }
    if (evaluated_at[position] == microseconds) {
        return values[position];
    }
    switch (node->kind) {
        case LOOPER_SIGNAL_NODE_GENERATOR:
            value = looper_analog_evaluate(&node->signal, microseconds);
            break;
        case LOOPER_SIGNAL_NODE_CHANNEL:
            value = channel_value(node->source);
            break;
        case LOOPER_SIGNAL_NODE_PIN:
            value = (looper_gpio_read(node->source) == LOOPER_SIGNAL_LEVEL_HIGH) ? 1.0 : 0.0;
            break;
        case LOOPER_SIGNAL_NODE_WATCH:
            value = watch_value(node->source);
            break;
        default:
            value = operate(
                node->kind, evaluate(node->left, microseconds), evaluate(node->right, microseconds)
            );
            break;
    }
    values[position] = value;
    evaluated_at[position] = microseconds;
    return value;
}

bool looper_signal_graph_value(uint32_t channel, uint64_t microseconds, double *value)
{
    for (size_t index = 0u; index < target_count; index++) {
        if (targets[index].channel == channel) {
            *value = evaluate(targets[index].node, microseconds);
            return true;
        }
    }
    return false;
}
