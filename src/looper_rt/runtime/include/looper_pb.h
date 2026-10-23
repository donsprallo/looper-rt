/**
 * @file
 * @brief Minimal protobuf wire format reader and writer.
 *
 * The simulation speaks the control protocol defined in
 * `proto/looper_control.proto`. Only the three wire features that protocol
 * uses are implemented: varints, length delimited fields, and nested
 * messages. Proto3 default values are omitted on the wire, which is what the
 * reference implementations expect.
 */
#ifndef LOOPER_PB_H
#define LOOPER_PB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Protobuf wire types used by the control protocol. */
#define LOOPER_PB_WIRE_VARINT 0u
#define LOOPER_PB_WIRE_LENGTH_DELIMITED 2u
#define LOOPER_PB_WIRE_FIXED64 1u

/** @brief Append only writer over a caller supplied buffer. */
typedef struct {
    uint8_t *data;    /**< Destination buffer. */
    size_t capacity;  /**< Size of the destination buffer. */
    size_t length;    /**< Bytes written so far. */
    bool overflow;    /**< Set once a write did not fit. */
} looper_pb_writer_t;

/** @brief Cursor over an encoded message. */
typedef struct {
    const uint8_t *data; /**< Encoded bytes. */
    size_t length;       /**< Number of encoded bytes. */
    size_t offset;       /**< Read position. */
} looper_pb_reader_t;

/**
 * @brief Prepare a writer over a buffer.
 * @param writer Writer to initialise.
 * @param buffer Destination buffer.
 * @param capacity Size of the destination buffer.
 */
void looper_pb_writer_init(looper_pb_writer_t *writer, uint8_t *buffer, size_t capacity);

/**
 * @brief Append a varint field, omitting it when the value is zero.
 * @retval true The field was written or deliberately omitted.
 * @retval false The buffer overflowed.
 */
bool looper_pb_write_varint(looper_pb_writer_t *writer, uint32_t field, uint64_t value);

/**
 * @brief Append a string field, omitting it when the string is empty.
 * @retval true The field was written or deliberately omitted.
 * @retval false The buffer overflowed.
 */
/**
 * @brief Write a repeated numeric field, every value of it.
 *
 * Packed into one length delimited field, which is what proto3
 * declares a repeated scalar to be. It exists because the singular
 * writer leaves a zero out -- right for a field whose absence means
 * the default, and wrong for a list whose position carries meaning:
 * a dropped zero shifts every value after it.
 *
 * @param writer Writer to append to.
 * @param field Field number.
 * @param values The values, in order.
 * @param count How many there are. Nothing is written for none.
 * @retval true Everything fitted.
 */
bool looper_pb_write_packed(
    looper_pb_writer_t *writer, uint32_t field, const uint32_t *values, size_t count
);

/**
 * @brief The same, for pointer sized values such as addresses.
 *
 * @param writer Writer to append to.
 * @param field Field number.
 * @param values The values, in order.
 * @param count How many there are. Nothing is written for none.
 * @retval true Everything fitted.
 */
bool looper_pb_write_packed_wide(
    looper_pb_writer_t *writer, uint32_t field, const uintptr_t *values, size_t count
);

bool looper_pb_write_string(looper_pb_writer_t *writer, uint32_t field, const char *value);

/**
 * @brief Append a length delimited field holding an already encoded message.
 * @retval true The field was written.
 * @retval false The buffer overflowed.
 */
bool looper_pb_write_message(
    looper_pb_writer_t *writer, uint32_t field, const uint8_t *message, size_t length
);

/**
 * @brief Append a double field, omitting it when the value is zero.
 * @retval true The field was written or deliberately omitted.
 * @retval false The buffer overflowed.
 */
bool looper_pb_write_double(looper_pb_writer_t *writer, uint32_t field, double value);

/**
 * @brief Read a double value stored as a fixed 64 bit field.
 * @retval true A value was read.
 * @retval false The message is exhausted or malformed.
 */
bool looper_pb_read_double(looper_pb_reader_t *reader, double *value);

/**
 * @brief Read a length delimited field as a string.
 *
 * Longer content is truncated rather than refused: the field is a label, and
 * a shortened label is more useful than a rejected message.
 *
 * @param reader Reader positioned on the field content.
 * @param destination Receives the string, always terminated.
 * @param capacity Size of the destination, including the terminator.
 * @retval true The field was read.
 * @retval false The message ends before the field does.
 */
bool looper_pb_read_string(looper_pb_reader_t *reader, char *destination, size_t capacity);

/**
 * @brief Read a length delimited field as a nested message.
 * @param reader Reader positioned on the field content.
 * @param nested Receives a reader over the nested message.
 * @retval true The nested message was framed.
 * @retval false The message ends before the nested one does.
 */
bool looper_pb_read_message(looper_pb_reader_t *reader, looper_pb_reader_t *nested);

/**
 * @brief Prepare a reader over an encoded message.
 * @param reader Reader to initialise.
 * @param data Encoded bytes.
 * @param length Number of encoded bytes.
 */
void looper_pb_reader_init(looper_pb_reader_t *reader, const uint8_t *data, size_t length);

/**
 * @brief Report whether the reader has reached the end of the message.
 */
bool looper_pb_reader_done(const looper_pb_reader_t *reader);

/**
 * @brief Read the next field tag.
 * @param reader Reader positioned at a tag.
 * @param field Receives the field number.
 * @param wire_type Receives the wire type.
 * @retval true A tag was read.
 * @retval false The message is exhausted or malformed.
 */
bool looper_pb_read_tag(looper_pb_reader_t *reader, uint32_t *field, uint32_t *wire_type);

/**
 * @brief Read a varint value.
 * @retval true A value was read.
 * @retval false The message is exhausted or malformed.
 */
bool looper_pb_read_varint(looper_pb_reader_t *reader, uint64_t *value);

/**
 * @brief Skip the value of a field the caller does not handle.
 * @retval true The value was skipped.
 * @retval false The message is malformed.
 */
bool looper_pb_skip(looper_pb_reader_t *reader, uint32_t wire_type);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_PB_H */
