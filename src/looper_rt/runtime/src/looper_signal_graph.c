/*
 * The signal graph: nodes in order, evaluated where an input is read.
 *
 * Each node keeps the value it last gave and the moment it gave it. Two
 * inputs read at the same moment through one node see one value -- which
 * matters for noise, where evaluating twice would give two -- and a node
 * shared by several paths is evaluated once per moment. A linear system or
 * a state model is stepped once per moment for the same reason, however
 * many picks read it.
 */
#include "looper_signal_graph.h"

#include <string.h>

#include "looper_analog.h"
#include "looper_gpio_model.h"
#include "looper_linear_system.h"
#include "looper_signal_graph_check.h"
#include "looper_model_worker.h"
#include "looper_state_model.h"
#include "looper_watch.h"

/* One past any moment, so that a fresh node never looks evaluated. */
#define NEVER UINT64_MAX

static looper_signal_node_t nodes[LOOPER_SIGNAL_GRAPH_CAPACITY];
static size_t node_count;
static looper_signal_target_t targets[LOOPER_ANALOG_CAPACITY];
static size_t target_count;
static looper_linear_system_t systems[LOOPER_SIGNAL_GRAPH_SYSTEMS];
static looper_linear_state_t system_states[LOOPER_SIGNAL_GRAPH_SYSTEMS];
static size_t system_count;
static looper_state_model_t models[LOOPER_SIGNAL_GRAPH_MODELS];
static looper_model_run_t model_runs[LOOPER_SIGNAL_GRAPH_MODELS];
static size_t model_count;
static double values[LOOPER_SIGNAL_GRAPH_CAPACITY];
static uint64_t evaluated_at[LOOPER_SIGNAL_GRAPH_CAPACITY];

static void mark_targets(bool driven)
{
    for (size_t index = 0u; index < target_count; index++) {
        looper_analog_set_graph_driven(targets[index].channel, driven);
    }
}

/* Whether the old graph had a system of the same size at a position. */
static bool same_system_was_at(size_t position, const looper_linear_system_t *system)
{
    if (position >= node_count || nodes[position].kind != LOOPER_SIGNAL_NODE_LINEAR) {
        return false;
    }
    const looper_linear_system_t *old = &systems[nodes[position].source];
    return old->states == system->states && old->inputs == system->inputs
           && old->outputs == system->outputs;
}

/* The states the new graph starts with: kept where a system stayed, fresh elsewhere. */
static void carry_states(const looper_signal_graph_t *graph, looper_linear_state_t *carried)
{
    for (size_t index = 0u; index < graph->system_count; index++) {
        looper_linear_reset(&carried[index]);
    }
    for (size_t position = 0u; position < graph->node_count; position++) {
        const looper_signal_node_t *node = &graph->nodes[position];
        if (node->kind == LOOPER_SIGNAL_NODE_LINEAR
            && same_system_was_at(position, &graph->systems[node->source])) {
            carried[node->source] = system_states[nodes[position].source];
        }
    }
}

/* Let go of every library in a list of runs. */
static void close_runs(looper_model_run_t *runs, size_t count)
{
    for (size_t index = 0u; index < count; index++) {
        looper_model_library_close(&runs[index].library);
    }
}

/*
 * Load every model library of a new graph into runs of its own. All or
 * nothing: one that cannot be loaded lets go of those loaded before it,
 * and the old graph keeps running.
 */
/* Whether the worker holds every interpreted model as the graph describes it. */
static const char *worker_fault(const looper_signal_graph_t *graph)
{
    bool needed = false;
    for (size_t index = 0u; index < graph->model_count; index++) {
        needed |= graph->models[index].provider == LOOPER_MODEL_PROVIDER_INTERPRETED;
    }
    if (!needed) {
        return NULL;
    }
    const char *fault = looper_model_worker_attach(graph->model_worker != NULL ? graph->model_worker : "");
    for (size_t index = 0u; fault == NULL && index < graph->model_count; index++) {
        const looper_state_model_t *model = &graph->models[index];
        if (model->provider == LOOPER_MODEL_PROVIDER_INTERPRETED
            && !looper_model_worker_holds(model->slot, model->inputs, model->outputs, model->states)) {
            fault = "the model worker does not hold the model the graph names in that slot";
        }
    }
    return fault;
}

static const char *open_models(const looper_signal_graph_t *graph, looper_model_run_t *opened)
{
    const char *fault = worker_fault(graph);
    if (fault != NULL) {
        return fault;
    }
    for (size_t index = 0u; index < graph->model_count; index++) {
        looper_state_model_start(&graph->models[index], &opened[index]);
        if (graph->models[index].provider == LOOPER_MODEL_PROVIDER_INTERPRETED) {
            continue;
        }
        fault = looper_model_library_open(&graph->models[index], &opened[index].library);
        if (fault != NULL) {
            close_runs(opened, index);
            return fault;
        }
    }
    return NULL;
}

static bool refuse(const char *fault, const char **reason)
{
    if (reason != NULL) {
        *reason = fault;
    }
    return false;
}

bool looper_signal_graph_set(const looper_signal_graph_t *graph, const char **reason)
{
    static looper_linear_state_t carried[LOOPER_SIGNAL_GRAPH_SYSTEMS];
    static looper_model_run_t opened[LOOPER_SIGNAL_GRAPH_MODELS];
    const char *fault = looper_signal_graph_fault(graph);

    if (fault == NULL) {
        fault = open_models(graph, opened);
    }
    if (fault != NULL) {
        return refuse(fault, reason);
    }
    carry_states(graph, carried);
    looper_signal_graph_clear();
    memcpy(nodes, graph->nodes, graph->node_count * sizeof(*graph->nodes));
    memcpy(targets, graph->targets, graph->target_count * sizeof(*graph->targets));
    memcpy(systems, graph->systems, graph->system_count * sizeof(*graph->systems));
    memcpy(system_states, carried, graph->system_count * sizeof(*carried));
    memcpy(models, graph->models, graph->model_count * sizeof(*graph->models));
    memcpy(model_runs, opened, graph->model_count * sizeof(*opened));
    node_count = graph->node_count;
    target_count = graph->target_count;
    system_count = graph->system_count;
    model_count = graph->model_count;
    mark_targets(true);
    return true;
}

void looper_signal_graph_clear(void)
{
    mark_targets(false);
    close_runs(model_runs, model_count);
    node_count = 0u;
    target_count = 0u;
    system_count = 0u;
    model_count = 0u;
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

static double evaluate(size_t position, uint64_t microseconds);

/* Step a linear system to the moment and give its first output. */
static double run_linear(const looper_signal_node_t *node, uint64_t microseconds)
{
    double input[LOOPER_LINEAR_PORTS] = {0.0};

    if (node->source >= system_count) {
        return 0.0;
    }
    for (uint32_t index = 0u; index < node->input_count; index++) {
        input[index] = evaluate(node->inputs[index], microseconds);
    }
    looper_linear_advance(&systems[node->source], &system_states[node->source], input, microseconds);
    return system_states[node->source].output[0];
}

/* Step a state model to the moment and give its first output. */
static double run_model(const looper_signal_node_t *node, uint64_t microseconds)
{
    double input[LOOPER_MODEL_PORTS] = {0.0};

    if (node->source >= model_count) {
        return 0.0;
    }
    for (uint32_t index = 0u; index < node->input_count; index++) {
        input[index] = evaluate(node->inputs[index], microseconds);
    }
    looper_state_model_advance(&models[node->source], &model_runs[node->source], input, microseconds);
    return model_runs[node->source].output[0];
}

/* One output of a linear system or a model, once it has been stepped to the moment. */
static double pick(const looper_signal_node_t *node, uint64_t microseconds)
{
    const looper_signal_node_t *picked = &nodes[node->left];

    (void)evaluate(node->left, microseconds);
    if (picked->kind == LOOPER_SIGNAL_NODE_MODEL) {
        return model_runs[picked->source].output[node->source];
    }
    return system_states[picked->source].output[node->source];
}

static double produce(const looper_signal_node_t *node, uint64_t microseconds)
{
    switch (node->kind) {
        case LOOPER_SIGNAL_NODE_GENERATOR:
            return looper_analog_evaluate(&node->signal, microseconds);
        case LOOPER_SIGNAL_NODE_CHANNEL:
            return channel_value(node->source);
        case LOOPER_SIGNAL_NODE_PIN:
            return (looper_gpio_read(node->source) == LOOPER_SIGNAL_LEVEL_HIGH) ? 1.0 : 0.0;
        case LOOPER_SIGNAL_NODE_WATCH:
            return watch_value(node->source);
        case LOOPER_SIGNAL_NODE_LINEAR:
            return run_linear(node, microseconds);
        case LOOPER_SIGNAL_NODE_PICK:
            return pick(node, microseconds);
        case LOOPER_SIGNAL_NODE_MODEL:
            return run_model(node, microseconds);
        default:
            return operate(
                node->kind, evaluate(node->left, microseconds), evaluate(node->right, microseconds)
            );
    }
}

static double evaluate(size_t position, uint64_t microseconds)
{
    if (position >= node_count) {
        return 0.0;
    }
    if (evaluated_at[position] != microseconds) {
        values[position] = produce(&nodes[position], microseconds);
        evaluated_at[position] = microseconds;
    }
    return values[position];
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

size_t looper_signal_graph_statuses(looper_signal_node_status_t *statuses, size_t capacity)
{
    size_t written = 0u;

    for (size_t position = 0u; position < node_count && written < capacity; position++) {
        const looper_signal_node_t *node = &nodes[position];
        if (node->kind != LOOPER_SIGNAL_NODE_MODEL || node->source >= model_count) {
            continue;
        }
        statuses[written].node = (uint32_t)position;
        statuses[written].fault = model_runs[node->source].fault;
        statuses[written].state = model_runs[node->source].state;
        statuses[written].state_count = models[node->source].states;
        written++;
    }
    return written;
}

bool looper_signal_graph_reset_model(uint32_t position, const char **reason)
{
    if (position >= node_count || nodes[position].kind != LOOPER_SIGNAL_NODE_MODEL) {
        return refuse("there is no model node at that position", reason);
    }
    uint32_t index = nodes[position].source;
    looper_state_model_reset(&models[index], &model_runs[index]);
    evaluated_at[position] = NEVER;
    return true;
}
