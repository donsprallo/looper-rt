/*
 * The measured half of a response.
 */
#include "looper_protocol_encode_memory.h"

#include "looper_protocol_elements.h"
#include "looper_protocol_fields.h"

static bool looper_write_timings(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->timing_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_timing(
                &response->timings[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_TIMINGS, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_profile_paths(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->profile_path_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_profile_path(
                &response->profile_paths[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(
                writer, RESPONSE_FIELD_PROFILE_PATHS, nested, nested_length
            )) {
            return false;
        }
    }
    return true;
}

static bool looper_write_profiles(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->profile_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_profile(
                &response->profiles[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_PROFILES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool looper_write_stack_path(looper_pb_writer_t *writer, const looper_response_t *response)
{
    /* Packed, so that the path keeps its length: a list written one
     * value at a time loses any zero in it, and a call stack is read
     * by its depth as much as by its addresses. */
    return looper_pb_write_packed_wide(
        writer, RESPONSE_FIELD_STACK_PATH, response->stack_path, response->stack_path_length
    );
}

static bool looper_write_heap(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];
    size_t nested_length = 0u;

    if (response->heap == NULL) {
        return true;
    }
    if (!looper_encode_heap(
            response->heap,
            response->heap_sites,
            response->heap_site_count,
            nested,
            sizeof(nested),
            &nested_length
        )) {
        return false;
    }
    return looper_pb_write_message(writer, RESPONSE_FIELD_HEAP, nested, nested_length);
}


bool looper_write_measurements(looper_pb_writer_t *writer, const looper_response_t *response)
{
    return looper_write_timings(writer, response) && looper_write_profiles(writer, response)
           && write_profile_paths(writer, response) && looper_write_stack_path(writer, response)
           && looper_write_heap(writer, response);
}
