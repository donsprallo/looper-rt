#include "looper_commands_graph.h"

#include "looper_signal_graph.h"

static looper_signal_node_status_t status_buffer[LOOPER_SIGNAL_GRAPH_MODELS];

void looper_serve_set_signal_graph(const looper_request_t *request, looper_response_t *response)
{
    const char *reason = NULL;

    if (request->signal_graph_overflow) {
        response->status = LOOPER_STATUS_INVALID_ARGUMENT;
        response->message = "more nodes, targets, systems or models than the simulation has room for";
        return;
    }
    looper_signal_graph_t graph = {
        .nodes = request->signal_nodes,
        .node_count = request->signal_node_count,
        .targets = request->signal_targets,
        .target_count = request->signal_target_count,
        .systems = request->signal_systems,
        .system_count = request->signal_system_count,
        .models = request->signal_models,
        .model_count = request->signal_model_count,
    };

    if (looper_signal_graph_set(&graph, &reason)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = reason;
}

void looper_serve_read_signal_graph(looper_response_t *response)
{
    response->signal_node_statuses = status_buffer;
    response->signal_node_status_count =
        looper_signal_graph_statuses(status_buffer, LOOPER_SIGNAL_GRAPH_MODELS);
}

void looper_serve_reset_model(const looper_request_t *request, looper_response_t *response)
{
    const char *reason = NULL;

    if (looper_signal_graph_reset_model(request->signal_node, &reason)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = reason;
}
