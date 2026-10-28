/*
 * Decoding of a linear system: four sizes and four matrices.
 *
 * Repeated numbers may come packed, as one run of bytes, or one by one;
 * both are proto3 and both are read. A matrix with more entries than there
 * is room for is counted but not stored, so that the graph refuses it as
 * one that does not match its size rather than reading past the end.
 */
#include "looper_protocol_decode_graph.h"

#include "looper_protocol_fields.h"

static void append(double *matrix, uint32_t capacity, uint32_t *count, double value)
{
    if (*count < capacity) {
        matrix[*count] = value;
    }
    (*count)++;
}

static bool decode_entries(
    looper_pb_reader_t *reader, uint32_t wire_type, double *matrix, uint32_t capacity, uint32_t *count
)
{
    looper_pb_reader_t packed;
    double value = 0.0;

    if (wire_type == LOOPER_PB_WIRE_FIXED64) {
        if (!looper_pb_read_double(reader, &value)) {
            return false;
        }
        append(matrix, capacity, count, value);
        return true;
    }
    if (wire_type != LOOPER_PB_WIRE_LENGTH_DELIMITED || !looper_pb_read_message(reader, &packed)) {
        return false;
    }
    while (!looper_pb_reader_done(&packed)) {
        if (!looper_pb_read_double(&packed, &value)) {
            return false;
        }
        append(matrix, capacity, count, value);
    }
    return true;
}

static bool decode_matrix(
    looper_pb_reader_t *reader, looper_linear_system_t *system, uint32_t field, uint32_t wire_type
)
{
    switch (field) {
        case LINEAR_SYSTEM_FIELD_A:
            return decode_entries(reader, wire_type, system->a, (uint32_t)(sizeof(system->a) / sizeof(double)), &system->a_count);
        case LINEAR_SYSTEM_FIELD_B:
            return decode_entries(reader, wire_type, system->b, (uint32_t)(sizeof(system->b) / sizeof(double)), &system->b_count);
        case LINEAR_SYSTEM_FIELD_C:
            return decode_entries(reader, wire_type, system->c, (uint32_t)(sizeof(system->c) / sizeof(double)), &system->c_count);
        case LINEAR_SYSTEM_FIELD_D:
            return decode_entries(reader, wire_type, system->d, (uint32_t)(sizeof(system->d) / sizeof(double)), &system->d_count);
        default:
            return looper_pb_skip(reader, wire_type);
    }
}

static void apply_size(looper_linear_system_t *system, uint32_t field, uint64_t value)
{
    switch (field) {
        case LINEAR_SYSTEM_FIELD_STATES:
            system->states = (uint32_t)value;
            break;
        case LINEAR_SYSTEM_FIELD_INPUTS:
            system->inputs = (uint32_t)value;
            break;
        case LINEAR_SYSTEM_FIELD_OUTPUTS:
            system->outputs = (uint32_t)value;
            break;
        case LINEAR_SYSTEM_FIELD_STEP_MICROSECONDS:
            system->step_microseconds = value;
            break;
        default:
            break;
    }
}

bool looper_decode_linear_system(looper_pb_reader_t *reader, looper_linear_system_t *system)
{
    while (!looper_pb_reader_done(reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;
        uint64_t value = 0u;

        if (!looper_pb_read_tag(reader, &field, &wire_type)) {
            return false;
        }
        if (wire_type != LOOPER_PB_WIRE_VARINT) {
            if (!decode_matrix(reader, system, field, wire_type)) {
                return false;
            }
            continue;
        }
        if (!looper_pb_read_varint(reader, &value)) {
            return false;
        }
        apply_size(system, field, value);
    }
    return true;
}
