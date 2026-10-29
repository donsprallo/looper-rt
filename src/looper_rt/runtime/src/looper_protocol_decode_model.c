/*
 * Decoding of a state model: sizes, clock, three lists of numbers, a provider.
 *
 * As for a linear system, a list longer than there is room for is counted
 * but not stored, so that the graph refuses the model as one whose lists do
 * not match its size rather than reading past the end.
 */
#include "looper_protocol_decode_graph.h"

#include "looper_protocol_fields.h"

static void append(double *list, uint32_t capacity, uint32_t *count, double value)
{
    if (*count < capacity) {
        list[*count] = value;
    }
    (*count)++;
}

/* Packed, as proto3 sends a repeated double, or one at a time; both are read. */
static bool decode_numbers(
    looper_pb_reader_t *reader, uint32_t wire_type, double *list, uint32_t capacity, uint32_t *count
)
{
    looper_pb_reader_t packed;
    double value = 0.0;

    if (wire_type == LOOPER_PB_WIRE_FIXED64) {
        if (!looper_pb_read_double(reader, &value)) {
            return false;
        }
        append(list, capacity, count, value);
        return true;
    }
    if (wire_type != LOOPER_PB_WIRE_LENGTH_DELIMITED || !looper_pb_read_message(reader, &packed)) {
        return false;
    }
    while (!looper_pb_reader_done(&packed)) {
        if (!looper_pb_read_double(&packed, &value)) {
            return false;
        }
        append(list, capacity, count, value);
    }
    return true;
}

static bool decode_compiled(looper_pb_reader_t *reader, looper_state_model_t *model)
{
    model->provider = LOOPER_MODEL_PROVIDER_COMPILED;
    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t value = 0u;
        bool read = false;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (field == COMPILED_MODEL_FIELD_LIBRARY && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
            read = looper_pb_read_string(reader, model->library, sizeof(model->library));
        } else if (field == COMPILED_MODEL_FIELD_SYMBOL && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
            read = looper_pb_read_string(reader, model->symbol, sizeof(model->symbol));
        } else if (field == COMPILED_MODEL_FIELD_ABI && wire_type == LOOPER_PB_WIRE_VARINT) {
            read = looper_pb_read_varint(reader, &value);
            model->abi = (uint32_t)value;
        } else {
            read = looper_pb_skip(reader, wire_type);
        }
        if (!read) {
            return false;
        }
    }
    return true;
}

static bool decode_interpreted(looper_pb_reader_t *reader, looper_state_model_t *model)
{
    model->provider = LOOPER_MODEL_PROVIDER_INTERPRETED;
    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t value = 0u;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (field != INTERPRETED_MODEL_FIELD_SLOT || wire_type != LOOPER_PB_WIRE_VARINT) {
            if (!looper_pb_skip(reader, wire_type)) {
                return false;
            }
            continue;
        }
        if (!looper_pb_read_varint(reader, &value)) {
            return false;
        }
        model->slot = (uint32_t)value;
    }
    return true;
}

static void apply_size(looper_state_model_t *model, uint32_t field, uint64_t value)
{
    switch (field) {
        case STATE_MODEL_FIELD_STATES:
            model->states = (uint32_t)value;
            break;
        case STATE_MODEL_FIELD_INPUTS:
            model->inputs = (uint32_t)value;
            break;
        case STATE_MODEL_FIELD_OUTPUTS:
            model->outputs = (uint32_t)value;
            break;
        case STATE_MODEL_FIELD_STEP_MICROSECONDS:
            model->step_microseconds = value;
            break;
        default:
            break;
    }
}

static bool decode_lists_and_providers(
    looper_pb_reader_t *reader, looper_state_model_t *model, uint32_t field, uint32_t wire_type
)
{
    looper_pb_reader_t nested;

    switch (field) {
        case STATE_MODEL_FIELD_CONSTANTS:
            return decode_numbers(reader, wire_type, model->constants, LOOPER_MODEL_CONSTANTS, &model->constant_count);
        case STATE_MODEL_FIELD_STATE:
            return decode_numbers(reader, wire_type, model->state, LOOPER_MODEL_STATES, &model->state_count);
        case STATE_MODEL_FIELD_INITIAL_STATE:
            return decode_numbers(
                reader, wire_type, model->initial_state, LOOPER_MODEL_STATES, &model->initial_state_count
            );
        case STATE_MODEL_FIELD_STEP_SECONDS:
            return wire_type == LOOPER_PB_WIRE_FIXED64 ? looper_pb_read_double(reader, &model->step_seconds)
                                                       : looper_pb_skip(reader, wire_type);
        case STATE_MODEL_FIELD_COMPILED:
            return looper_pb_read_message(reader, &nested) && decode_compiled(&nested, model);
        case STATE_MODEL_FIELD_INTERPRETED:
            return looper_pb_read_message(reader, &nested) && decode_interpreted(&nested, model);
        default:
            return looper_pb_skip(reader, wire_type);
    }
}

bool looper_decode_state_model(looper_pb_reader_t *reader, looper_state_model_t *model)
{
    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t value = 0u;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (wire_type != LOOPER_PB_WIRE_VARINT) {
            if (!decode_lists_and_providers(reader, model, field, wire_type)) {
                return false;
            }
            continue;
        }
        if (!looper_pb_read_varint(reader, &value)) {
            return false;
        }
        apply_size(model, field, value);
    }
    return true;
}
