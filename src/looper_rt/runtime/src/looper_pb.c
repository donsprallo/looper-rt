#include "looper_pb.h"

#include <string.h>

#define VARINT_CONTINUATION 0x80u
#define VARINT_PAYLOAD 0x7Fu
#define VARINT_SHIFT 7u
#define VARINT_MAX_BYTES 10u

static bool write_raw(looper_pb_writer_t *writer, const uint8_t *bytes, size_t count)
{
    if (writer->overflow) {
        return false;
    }
    if (writer->length + count > writer->capacity) {
        writer->overflow = true;
        return false;
    }
    memcpy(&writer->data[writer->length], bytes, count);
    writer->length += count;
    return true;
}

static bool write_raw_varint(looper_pb_writer_t *writer, uint64_t value)
{
    uint8_t encoded[VARINT_MAX_BYTES];
    size_t count = 0u;

    do {
        uint8_t byte = (uint8_t)(value & VARINT_PAYLOAD);
        value >>= VARINT_SHIFT;
        if (value != 0u) {
            byte |= VARINT_CONTINUATION;
        }
        encoded[count] = byte;
        count++;
    } while (value != 0u && count < VARINT_MAX_BYTES);

    return write_raw(writer, encoded, count);
}

static bool write_tag(looper_pb_writer_t *writer, uint32_t field, uint32_t wire_type)
{
    return write_raw_varint(writer, ((uint64_t)field << 3u) | (uint64_t)wire_type);
}

void looper_pb_writer_init(looper_pb_writer_t *writer, uint8_t *buffer, size_t capacity)
{
    writer->data = buffer;
    writer->capacity = capacity;
    writer->length = 0u;
    writer->overflow = false;
}

bool looper_pb_write_varint(looper_pb_writer_t *writer, uint32_t field, uint64_t value)
{
    if (value == 0u) {
        return !writer->overflow;
    }
    if (!write_tag(writer, field, LOOPER_PB_WIRE_VARINT)) {
        return false;
    }
    return write_raw_varint(writer, value);
}

/** @brief How many bytes one value takes as a varint. */
static size_t varint_length(uint64_t value)
{
    size_t count = 1u;

    while (value > VARINT_PAYLOAD) {
        value >>= VARINT_SHIFT;
        count++;
    }
    return count;
}

/** @brief The shared half of writing a packed field. */
static bool begin_packed(looper_pb_writer_t *writer, uint32_t field, size_t payload)
{
    return write_tag(writer, field, LOOPER_PB_WIRE_LENGTH_DELIMITED) &&
           write_raw_varint(writer, payload);
}

bool looper_pb_write_packed(
    looper_pb_writer_t *writer, uint32_t field, const uint32_t *values, size_t count
)
{
    size_t payload = 0u;

    if (count == 0u) {
        return !writer->overflow;
    }
    for (size_t index = 0u; index < count; index++) {
        payload += varint_length(values[index]);
    }
    if (!begin_packed(writer, field, payload)) {
        return false;
    }
    for (size_t index = 0u; index < count; index++) {
        if (!write_raw_varint(writer, values[index])) {
            return false;
        }
    }
    return true;
}

bool looper_pb_write_packed_wide(
    looper_pb_writer_t *writer, uint32_t field, const uintptr_t *values, size_t count
)
{
    size_t payload = 0u;

    if (count == 0u) {
        return !writer->overflow;
    }
    for (size_t index = 0u; index < count; index++) {
        payload += varint_length((uint64_t)values[index]);
    }
    if (!begin_packed(writer, field, payload)) {
        return false;
    }
    for (size_t index = 0u; index < count; index++) {
        if (!write_raw_varint(writer, (uint64_t)values[index])) {
            return false;
        }
    }
    return true;
}

bool looper_pb_write_string(looper_pb_writer_t *writer, uint32_t field, const char *value)
{
    size_t length = (value != NULL) ? strlen(value) : 0u;

    if (length == 0u) {
        return !writer->overflow;
    }
    return looper_pb_write_message(writer, field, (const uint8_t *)value, length);
}

bool looper_pb_write_message(
    looper_pb_writer_t *writer, uint32_t field, const uint8_t *message, size_t length
)
{
    if (!write_tag(writer, field, LOOPER_PB_WIRE_LENGTH_DELIMITED)) {
        return false;
    }
    if (!write_raw_varint(writer, (uint64_t)length)) {
        return false;
    }
    return write_raw(writer, message, length);
}

bool looper_pb_write_double(looper_pb_writer_t *writer, uint32_t field, double value)
{
    uint8_t encoded[sizeof(double)];

    if (value == 0.0) {
        return !writer->overflow;
    }
    if (!write_tag(writer, field, LOOPER_PB_WIRE_FIXED64)) {
        return false;
    }
    memcpy(encoded, &value, sizeof(encoded));
    return write_raw(writer, encoded, sizeof(encoded));
}

void looper_pb_reader_init(looper_pb_reader_t *reader, const uint8_t *data, size_t length)
{
    reader->data = data;
    reader->length = length;
    reader->offset = 0u;
}

bool looper_pb_reader_done(const looper_pb_reader_t *reader)
{
    return reader->offset >= reader->length;
}

bool looper_pb_read_varint(looper_pb_reader_t *reader, uint64_t *value)
{
    uint64_t result = 0u;
    unsigned int shift = 0u;

    while (reader->offset < reader->length && shift < VARINT_MAX_BYTES * VARINT_SHIFT) {
        uint8_t byte = reader->data[reader->offset];
        reader->offset++;
        result |= (uint64_t)(byte & VARINT_PAYLOAD) << shift;
        if ((byte & VARINT_CONTINUATION) == 0u) {
            *value = result;
            return true;
        }
        shift += VARINT_SHIFT;
    }
    return false;
}

bool looper_pb_read_tag(looper_pb_reader_t *reader, uint32_t *field, uint32_t *wire_type)
{
    uint64_t tag = 0u;

    if (!looper_pb_read_varint(reader, &tag)) {
        return false;
    }
    *field = (uint32_t)(tag >> 3u);
    *wire_type = (uint32_t)(tag & 0x07u);
    return *field != 0u;
}

bool looper_pb_read_double(looper_pb_reader_t *reader, double *value)
{
    double decoded = 0.0;

    if (reader->offset + sizeof(double) > reader->length) {
        return false;
    }
    memcpy(&decoded, &reader->data[reader->offset], sizeof(double));
    reader->offset += sizeof(double);
    *value = decoded;
    return true;
}

bool looper_pb_read_string(looper_pb_reader_t *reader, char *destination, size_t capacity)
{
    uint64_t length = 0u;
    size_t copied;

    if (capacity == 0u) {
        return false;
    }
    if (!looper_pb_read_varint(reader, &length)) {
        return false;
    }
    if (reader->offset + (size_t)length > reader->length) {
        return false;
    }
    copied = ((size_t)length < capacity - 1u) ? (size_t)length : capacity - 1u;
    memcpy(destination, &reader->data[reader->offset], copied);
    destination[copied] = '\0';
    reader->offset += (size_t)length;
    return true;
}

bool looper_pb_read_message(looper_pb_reader_t *reader, looper_pb_reader_t *nested)
{
    uint64_t length = 0u;

    if (!looper_pb_read_varint(reader, &length)) {
        return false;
    }
    if (reader->offset + (size_t)length > reader->length) {
        return false;
    }
    looper_pb_reader_init(nested, &reader->data[reader->offset], (size_t)length);
    reader->offset += (size_t)length;
    return true;
}

bool looper_pb_skip(looper_pb_reader_t *reader, uint32_t wire_type)
{
    uint64_t value = 0u;

    if (wire_type == LOOPER_PB_WIRE_VARINT) {
        return looper_pb_read_varint(reader, &value);
    }
    if (wire_type == LOOPER_PB_WIRE_FIXED64) {
        double ignored = 0.0;
        return looper_pb_read_double(reader, &ignored);
    }
    if (wire_type != LOOPER_PB_WIRE_LENGTH_DELIMITED) {
        return false;
    }
    if (!looper_pb_read_varint(reader, &value)) {
        return false;
    }
    if (reader->offset + (size_t)value > reader->length) {
        return false;
    }
    reader->offset += (size_t)value;
    return true;
}
