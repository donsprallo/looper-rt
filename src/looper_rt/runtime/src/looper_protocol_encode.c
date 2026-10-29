/*
 * Encoding of responses.
 *
 * Each repeated element is encoded into a scratch buffer first, because a
 * length delimited field has to know its length before its content. The
 * scratch buffer is one per call site rather than one shared, so that nesting
 * never writes over an enclosing message.
 */
#include "looper_pb.h"
#include "looper_protocol.h"
#include "looper_protocol_elements.h"
#include "looper_protocol_encode_memory.h"
#include "looper_protocol_fields.h"

static bool write_pins(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->pin_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_pin(&response->pins[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_PINS, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_journal(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->journal_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_entry(&response->journal[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_JOURNAL, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_channels(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->channel_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_channel(&response->channels[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_CHANNELS, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_samples(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->sample_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_sample(&response->samples[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_SAMPLES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_pin_changes(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->pin_change_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_pin_change(
                &response->pin_changes[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_PIN_CHANGES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

/* A status carries up to a model's whole state: more than other nested messages. */
#define STATUS_CAPACITY (LOOPER_MODEL_STATES * sizeof(double) + NESTED_CAPACITY)

static bool encode_status(
    const looper_signal_node_status_t *status, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, SIGNAL_NODE_STATUS_FIELD_NODE, status->node);
    looper_pb_write_string(&writer, SIGNAL_NODE_STATUS_FIELD_FAULT, status->fault);
    looper_pb_write_packed_doubles(
        &writer, SIGNAL_NODE_STATUS_FIELD_STATE, status->state, status->state_count
    );
    *length = writer.length;
    return !writer.overflow;
}

static bool write_signal_nodes(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[STATUS_CAPACITY];

    for (size_t index = 0u; index < response->signal_node_status_count; index++) {
        size_t nested_length = 0u;

        if (!encode_status(&response->signal_node_statuses[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_SIGNAL_NODES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_watches(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->watch_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_watch(&response->watches[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_WATCHES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_buses(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->bus_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_bus(&response->buses[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_BUSES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_components(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->component_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_component(
                &response->components[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_COMPONENTS, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_exchanges(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];

    for (size_t index = 0u; index < response->exchange_count; index++) {
        size_t nested_length = 0u;

        if (!looper_encode_record(
                &response->exchanges[index], nested, sizeof(nested), &nested_length
            )) {
            return false;
        }
        if (!looper_pb_write_message(writer, RESPONSE_FIELD_EXCHANGES, nested, nested_length)) {
            return false;
        }
    }
    return true;
}

static bool write_image(looper_pb_writer_t *writer, const looper_response_t *response)
{
    uint8_t nested[NESTED_CAPACITY];
    size_t nested_length = 0u;

    if (response->image == NULL) {
        return true;
    }
    if (!looper_encode_image(response->image, nested, sizeof(nested), &nested_length)) {
        return false;
    }
    return looper_pb_write_message(writer, RESPONSE_FIELD_IMAGE, nested, nested_length);
}

static bool write_repeated(looper_pb_writer_t *writer, const looper_response_t *response)
{
    return write_pins(writer, response) && write_journal(writer, response)
           && write_channels(writer, response) && write_samples(writer, response)
           && write_watches(writer, response) && write_buses(writer, response)
           && write_components(writer, response) && write_exchanges(writer, response)
           && looper_write_measurements(writer, response)
           && write_pin_changes(writer, response) && write_image(writer, response)
           && write_signal_nodes(writer, response);
}

bool looper_protocol_encode_response(
    const looper_response_t *response, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_ID, response->id);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_STATUS, (uint64_t)response->status);
    looper_pb_write_string(&writer, RESPONSE_FIELD_MESSAGE, response->message);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_MICROSECONDS, response->microseconds);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_TIME_MODE, (uint64_t)response->time_mode);
    if (!write_repeated(&writer, response)) {
        return false;
    }
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_PROTOCOL_VERSION, response->protocol_version
    );
    looper_pb_write_string(&writer, RESPONSE_FIELD_FIRMWARE, response->firmware);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_NEXT_SEQUENCE, response->next_sequence);
    looper_pb_write_varint(&writer, RESPONSE_FIELD_LOST_PROFILES, response->lost_profiles);
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_STACK_BYTES, response->stack_deepest_bytes
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_STACK_DEPTH, response->stack_deepest_depth
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_STACK_COMPLETE, response->stack_path_complete ? 1u : 0u
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_PROFILING, response->profiling ? 1u : 0u
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_NEXT_BUS_SEQUENCE, response->next_bus_sequence
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_LOST_PROFILE_PATHS, response->lost_profile_paths
    );
    looper_pb_write_varint(
        &writer, RESPONSE_FIELD_WATCHING_PATHS, response->watching_profile_paths ? 1u : 0u
    );
    *length = writer.length;
    return !writer.overflow;
}
