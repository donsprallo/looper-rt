/*
 * Decoding of client requests.
 *
 * A request is a flat message of scalars plus one nested signal, so the
 * decoder walks fields and assigns. Unknown fields are skipped rather than
 * refused: a newer client talking to an older simulation should lose the
 * field it added, not the request it belongs to.
 */
#include <string.h>

#include "looper_pb.h"
#include "looper_protocol.h"
#include "looper_protocol_fields.h"

static void apply_request_watch_varint(looper_request_t *request, uint32_t field, uint64_t value)
{
    switch (field) {
        case REQUEST_FIELD_CHANNEL:
            request->channel = (uint32_t)value;
            break;
        case REQUEST_FIELD_AFTER_SEQUENCE:
            request->after_sequence = (uint32_t)value;
            break;
        case REQUEST_FIELD_ADDRESS:
            request->address = value;
            break;
        case REQUEST_FIELD_ENCODING:
            request->encoding = (looper_encoding_t)value;
            break;
        case REQUEST_FIELD_BYTE_WIDTH:
            request->byte_width = (uint32_t)value;
            break;
        case REQUEST_FIELD_WRITABLE:
            request->writable = value != 0u;
            break;
        case REQUEST_FIELD_WATCHING:
            request->watching = value != 0u;
            break;
        case REQUEST_FIELD_BIT_OFFSET:
            request->bit_offset = (uint32_t)value;
            break;
        case REQUEST_FIELD_BIT_WIDTH:
            request->bit_width = (uint32_t)value;
            break;
        case REQUEST_FIELD_BUS:
            request->bus = (uint32_t)value;
            break;
        default:
            break;
    }
}

static void apply_request_varint(looper_request_t *request, uint32_t field, uint64_t value)
{
    switch (field) {
        case REQUEST_FIELD_ID:
            request->id = (uint32_t)value;
            break;
        case REQUEST_FIELD_COMMAND:
            request->command = (looper_command_t)value;
            break;
        case REQUEST_FIELD_PIN:
            request->pin = (uint32_t)value;
            break;
        case REQUEST_FIELD_LEVEL:
            request->level = (looper_signal_level_t)value;
            break;
        case REQUEST_FIELD_MICROSECONDS:
            request->microseconds = value;
            break;
        default:
            apply_request_watch_varint(request, field, value);
            break;
    }
}

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

static bool decode_signal(looper_pb_reader_t *reader, looper_signal_t *signal)
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

/* Bytes, so they are taken from the nested reader's own buffer. */
static void apply_payload(looper_request_t *request, const looper_pb_reader_t *payload)
{
    size_t length = payload->length;

    if (length > LOOPER_BUS_PAYLOAD_CAPACITY) {
        length = LOOPER_BUS_PAYLOAD_CAPACITY;
    }
    memcpy(request->payload, payload->data, length);
    request->payload_length = (uint8_t)length;
}

static bool decode_address(looper_pb_reader_t *reader, looper_bus_address_t *address)
{
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
        if (field == BUS_ADDRESS_FIELD_VALUE) {
            address->value = (uint32_t)value;
        } else if (field == BUS_ADDRESS_FIELD_EXTENDED) {
            address->extended = value != 0u;
        }
    }
    return true;
}

/** @brief Decode a field that is not a varint, if this message has one. */
static bool decode_wide_field(
    looper_pb_reader_t *reader, looper_request_t *request, uint32_t field, uint32_t wire_type
)
{
    looper_pb_reader_t nested;

    if (field == REQUEST_FIELD_SIGNAL && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return looper_pb_read_message(reader, &nested) && decode_signal(&nested, &request->signal);
    }
    if (field == REQUEST_FIELD_PATH && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return looper_pb_read_string(reader, request->path, sizeof(request->path));
    }
    if (field == REQUEST_FIELD_LABEL && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return looper_pb_read_string(reader, request->label, sizeof(request->label));
    }
    if (field == REQUEST_FIELD_VALUE && wire_type == LOOPER_PB_WIRE_FIXED64) {
        return looper_pb_read_double(reader, &request->value);
    }
    if (field == REQUEST_FIELD_BUS_ADDRESS && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return looper_pb_read_message(reader, &nested)
               && decode_address(&nested, &request->bus_address);
    }
    if (field == REQUEST_FIELD_PAYLOAD && wire_type == LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        if (!looper_pb_read_message(reader, &nested)) {
            return false;
        }
        apply_payload(request, &nested);
        return true;
    }
    return looper_pb_skip(reader, wire_type);
}

static bool decode_request_field(
    looper_pb_reader_t *reader, looper_request_t *request, uint32_t field, uint32_t wire_type
)
{
    uint64_t value = 0u;

    if (wire_type != LOOPER_PB_WIRE_VARINT) {
        return decode_wide_field(reader, request, field, wire_type);
    }
    if (!looper_pb_read_varint(reader, &value)) {
        return false;
    }
    apply_request_varint(request, field, value);
    return true;
}

bool looper_protocol_decode_request(
    const uint8_t *data, size_t length, looper_request_t *request
)
{
    looper_pb_reader_t reader;

    memset(request, 0, sizeof(*request));
    looper_pb_reader_init(&reader, data, length);

    while (!looper_pb_reader_done(&reader)) {
        uint32_t field = 0u;
        uint32_t wire_type = 0u;

        if (!looper_pb_read_tag(&reader, &field, &wire_type)) {
            return false;
        }
        if (!decode_request_field(&reader, request, field, wire_type)) {
            return false;
        }
    }
    return true;
}
