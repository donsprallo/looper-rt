/*
 * Decoding of signals and of the signal graph made of them.
 */
#include "looper_protocol_decode_graph.h"

#include <string.h>

#include "looper_protocol_fields.h"

static void apply_signal_field(
    looper_signal_t *signal, uint32_t field, uint64_t integer, double real
)
{
    switch (field) {
        case SIGNAL_FIELD_WAVEFORM:
            signal->waveform = (looper_waveform_t)integer;
            break;
        case SIGNAL_FIELD_AMPLITUDE:
            signal->amplitude = real;
            break;
        case SIGNAL_FIELD_OFFSET:
            signal->offset = real;
            break;
        case SIGNAL_FIELD_FREQUENCY:
            signal->frequency_hz = real;
            break;
        case SIGNAL_FIELD_PHASE:
            signal->phase_degrees = real;
            break;
        default:
            break;
    }
}

bool looper_decode_signal(looper_pb_reader_t *reader, looper_signal_t *signal)
{
    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t integer = 0u;
        double real = 0.0;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (wire_type == LOOPER_PB_WIRE_VARINT) {
            if (!looper_pb_read_varint(reader, &integer)) {
                return false;
            }
        } else if (wire_type == LOOPER_PB_WIRE_FIXED64) {
            if (!looper_pb_read_double(reader, &real)) {
                return false;
            }
        } else if (!looper_pb_skip(reader, wire_type)) {
            return false;
        }
        apply_signal_field(signal, field, integer, real);
    }
    return true;
}

static void apply_node_varint(looper_signal_node_t *node, uint32_t field, uint64_t value)
{
    switch (field) {
        case SIGNAL_NODE_FIELD_KIND:
            node->kind = (looper_signal_node_kind_t)value;
            break;
        case SIGNAL_NODE_FIELD_SOURCE:
            node->source = (uint32_t)value;
            break;
        case SIGNAL_NODE_FIELD_LEFT:
            node->left = (uint32_t)value;
            break;
        case SIGNAL_NODE_FIELD_RIGHT:
            node->right = (uint32_t)value;
            break;
        default:
            break;
    }
}

/* One more input of a linear or model node; past the room there is, only counted. */
static void append_input(looper_signal_node_t *node, uint64_t position)
{
    if (node->input_count < LOOPER_SIGNAL_NODE_INPUTS) {
        node->inputs[node->input_count] = (uint32_t)position;
    }
    node->input_count++;
}

static bool decode_inputs(looper_pb_reader_t *reader, looper_signal_node_t *node, uint32_t wire_type)
{
    looper_pb_reader_t packed;
    uint64_t value = 0u;

    if (wire_type == LOOPER_PB_WIRE_VARINT) {
        if (!looper_pb_read_varint(reader, &value)) {
            return false;
        }
        append_input(node, value);
        return true;
    }
    if (wire_type != LOOPER_PB_WIRE_LENGTH_DELIMITED || !looper_pb_read_message(reader, &packed)) {
        return false;
    }
    while (!looper_pb_reader_done(&packed)) {
        if (!looper_pb_read_varint(&packed, &value)) {
            return false;
        }
        append_input(node, value);
    }
    return true;
}

/*
 * A node's linear system goes into the request's table of systems, and the
 * node keeps its place there. One past the room is decoded into a scratch
 * system and dropped, and the request says so.
 */
static bool decode_system(looper_pb_reader_t *reader, looper_request_t *request, looper_signal_node_t *node)
{
    static looper_linear_system_t scratch;
    looper_pb_reader_t nested;
    looper_linear_system_t *system = &scratch;

    if (request->signal_system_count < LOOPER_SIGNAL_GRAPH_SYSTEMS) {
        system = &request->signal_systems[request->signal_system_count];
        node->source = (uint32_t)request->signal_system_count;
        request->signal_system_count++;
    } else {
        request->signal_graph_overflow = true;
    }
    memset(system, 0, sizeof(*system));
    return looper_pb_read_message(reader, &nested) && looper_decode_linear_system(&nested, system);
}

/* A node's model goes into the request's table of models, as a system does. */
static bool decode_model(looper_pb_reader_t *reader, looper_request_t *request, looper_signal_node_t *node)
{
    static looper_state_model_t scratch;
    looper_pb_reader_t nested;
    looper_state_model_t *model = &scratch;

    if (request->signal_model_count < LOOPER_SIGNAL_GRAPH_MODELS) {
        model = &request->signal_models[request->signal_model_count];
        node->source = (uint32_t)request->signal_model_count;
        request->signal_model_count++;
    } else {
        request->signal_graph_overflow = true;
    }
    memset(model, 0, sizeof(*model));
    return looper_pb_read_message(reader, &nested) && looper_decode_state_model(&nested, model);
}

static bool decode_node_field(
    looper_pb_reader_t *reader,
    looper_request_t *request,
    looper_signal_node_t *node,
    uint32_t field,
    uint32_t wire_type
)
{
    looper_pb_reader_t nested;
    uint64_t value = 0u;

    if (field == SIGNAL_NODE_FIELD_SIGNAL && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return looper_pb_read_message(reader, &nested) && looper_decode_signal(&nested, &node->signal);
    }
    if (field == SIGNAL_NODE_FIELD_LINEAR && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return decode_system(reader, request, node);
    }
    if (field == SIGNAL_NODE_FIELD_MODEL && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return decode_model(reader, request, node);
    }
    if (field == SIGNAL_NODE_FIELD_INPUTS) {
        return decode_inputs(reader, node, wire_type);
    }
    if (wire_type != LOOPER_PB_WIRE_VARINT) {
        return looper_pb_skip(reader, wire_type);
    }
    if (!looper_pb_read_varint(reader, &value)) {
        return false;
    }
    apply_node_varint(node, field, value);
    return true;
}

bool looper_decode_signal_node(looper_pb_reader_t *reader, looper_request_t *request)
{
    looper_signal_node_t node = {0};

    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;

        if (!looper_pb_read_tag(reader, &field, &wire_type)
            || !decode_node_field(reader, request, &node, field, wire_type)) {
            return false;
        }
    }
    if (request->signal_node_count >= LOOPER_SIGNAL_GRAPH_CAPACITY) {
        request->signal_graph_overflow = true;
        return true;
    }
    request->signal_nodes[request->signal_node_count] = node;
    request->signal_node_count++;
    return true;
}

bool looper_decode_signal_target(looper_pb_reader_t *reader, looper_request_t *request)
{
    looper_signal_target_t target = {0};

    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t value = 0u;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (wire_type != LOOPER_PB_WIRE_VARINT) {
            if (!looper_pb_skip(reader, wire_type)) {
                return false;
            }
            continue;
        }
        if (!looper_pb_read_varint(reader, &value)) {
            return false;
        }
        if (field == SIGNAL_TARGET_FIELD_CHANNEL) {
            target.channel = (uint32_t)value;
        } else if (field == SIGNAL_TARGET_FIELD_NODE) {
            target.node = (uint32_t)value;
        }
    }
    if (request->signal_target_count >= LOOPER_ANALOG_CAPACITY) {
        request->signal_graph_overflow = true;
        return true;
    }
    request->signal_targets[request->signal_target_count] = target;
    request->signal_target_count++;
    return true;
}
