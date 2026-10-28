/*
 * Encoding of the messages nested inside a response.
 *
 * Each value is encoded into a buffer of its own. A length delimited field
 * has to know its length before its content, so a nested message is finished
 * first and written afterwards.
 */
#include "looper_protocol_elements.h"

#include "looper_pb.h"
#include "looper_protocol_fields.h"

bool looper_encode_pin(const looper_pin_t *pin, uint8_t *buffer, size_t capacity, size_t *length)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, PIN_FIELD_ID, pin->id);
    looper_pb_write_string(&writer, PIN_FIELD_LABEL, pin->label);
    looper_pb_write_varint(&writer, PIN_FIELD_DIRECTION, (uint64_t)pin->direction);
    looper_pb_write_varint(&writer, PIN_FIELD_LEVEL, (uint64_t)pin->level);
    looper_pb_write_varint(&writer, PIN_FIELD_POLARITY, (uint64_t)pin->polarity);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_entry(
    const looper_journal_entry_t *entry, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_string(&writer, JOURNAL_FIELD_FUNCTION, entry->function);
    looper_pb_write_varint(&writer, JOURNAL_FIELD_MICROSECONDS, entry->microseconds);
    looper_pb_write_varint(&writer, JOURNAL_FIELD_SEQUENCE, entry->sequence);
    *length = writer.length;
    return !writer.overflow;
}

static bool encode_signal(
    const looper_signal_t *signal, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, SIGNAL_FIELD_WAVEFORM, (uint64_t)signal->waveform);
    looper_pb_write_double(&writer, SIGNAL_FIELD_AMPLITUDE, signal->amplitude);
    looper_pb_write_double(&writer, SIGNAL_FIELD_OFFSET, signal->offset);
    looper_pb_write_double(&writer, SIGNAL_FIELD_FREQUENCY, signal->frequency_hz);
    looper_pb_write_double(&writer, SIGNAL_FIELD_PHASE, signal->phase_degrees);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_channel(
    const looper_channel_t *channel, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;
    uint8_t signal[NESTED_CAPACITY];
    size_t signal_length = 0u;

    if (!encode_signal(&channel->signal, signal, sizeof(signal), &signal_length)) {
        return false;
    }
    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, CHANNEL_FIELD_ID, channel->id);
    looper_pb_write_string(&writer, CHANNEL_FIELD_LABEL, channel->label);
    looper_pb_write_string(&writer, CHANNEL_FIELD_UNIT, channel->unit);
    looper_pb_write_varint(&writer, CHANNEL_FIELD_DIRECTION, (uint64_t)channel->direction);
    looper_pb_write_double(&writer, CHANNEL_FIELD_VALUE, channel->value);
    looper_pb_write_varint(&writer, CHANNEL_FIELD_HAS_VALUE, channel->has_value ? 1u : 0u);
    looper_pb_write_varint(&writer, CHANNEL_FIELD_DRIVEN_BY_GRAPH, channel->driven_by_graph ? 1u : 0u);
    if (signal_length > 0u) {
        looper_pb_write_message(&writer, CHANNEL_FIELD_SIGNAL, signal, signal_length);
    }
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_sample(
    const looper_sample_t *sample, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, SAMPLE_FIELD_CHANNEL, sample->channel);
    looper_pb_write_varint(&writer, SAMPLE_FIELD_MICROSECONDS, sample->microseconds);
    looper_pb_write_double(&writer, SAMPLE_FIELD_VALUE, sample->value);
    looper_pb_write_varint(&writer, SAMPLE_FIELD_SEQUENCE, sample->sequence);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_profile_path(
    const looper_profile_path_entry_t *node, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, PROFILE_PATH_FIELD_ADDRESS, node->address);
    looper_pb_write_varint(&writer, PROFILE_PATH_FIELD_PARENT, node->parent);
    looper_pb_write_varint(&writer, PROFILE_PATH_FIELD_CALLS, node->calls);
    looper_pb_write_varint(&writer, PROFILE_PATH_FIELD_TOTAL, node->total_nanoseconds);
    looper_pb_write_varint(&writer, PROFILE_PATH_FIELD_SELF, node->self_nanoseconds);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_pin_change(
    const looper_pin_change_t *change, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, PIN_CHANGE_FIELD_PIN, change->pin);
    looper_pb_write_varint(&writer, PIN_CHANGE_FIELD_LEVEL, (uint64_t)change->level);
    looper_pb_write_varint(&writer, PIN_CHANGE_FIELD_MICROSECONDS, change->microseconds);
    looper_pb_write_varint(&writer, PIN_CHANGE_FIELD_SEQUENCE, change->sequence);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_watch(
    const looper_watch_t *watch, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, WATCH_FIELD_CHANNEL, watch->channel);
    looper_pb_write_string(&writer, WATCH_FIELD_PATH, watch->path);
    looper_pb_write_varint(&writer, WATCH_FIELD_ADDRESS, watch->address);
    looper_pb_write_varint(&writer, WATCH_FIELD_ENCODING, (uint64_t)watch->encoding);
    looper_pb_write_varint(&writer, WATCH_FIELD_BYTE_WIDTH, watch->byte_width);
    looper_pb_write_varint(&writer, WATCH_FIELD_WRITABLE, watch->writable ? 1u : 0u);
    looper_pb_write_double(&writer, WATCH_FIELD_VALUE, watch->value);
    looper_pb_write_varint(&writer, WATCH_FIELD_BIT_OFFSET, watch->bit_offset);
    looper_pb_write_varint(&writer, WATCH_FIELD_BIT_WIDTH, watch->bit_width);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_image(
    const looper_image_t *image, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, IMAGE_FIELD_RUNTIME_BASE, image->runtime_base);
    looper_pb_write_varint(&writer, IMAGE_FIELD_SIZE, image->size);
    looper_pb_write_string(&writer, IMAGE_FIELD_EXECUTABLE, image->executable);
    *length = writer.length;
    return !writer.overflow;
}


/*
 * An address is its own message because it means different things on
 * different buses and a client needs both halves of it together.
 */
static bool encode_address(
    looper_bus_address_t address, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, BUS_ADDRESS_FIELD_VALUE, address.value);
    looper_pb_write_varint(&writer, BUS_ADDRESS_FIELD_EXTENDED, address.extended ? 1u : 0u);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_bus(
    const looper_bus_t *bus, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, BUS_FIELD_ID, bus->id);
    looper_pb_write_varint(&writer, BUS_FIELD_KIND, (uint64_t)bus->kind);
    looper_pb_write_string(&writer, BUS_FIELD_LABEL, bus->label);
    looper_pb_write_varint(&writer, BUS_FIELD_BITRATE, bus->bitrate_bps);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_component(
    const looper_component_t *component, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;
    uint8_t address[NESTED_CAPACITY];
    size_t address_length = 0u;

    if (!encode_address(component->address, address, sizeof(address), &address_length)) {
        return false;
    }
    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, COMPONENT_FIELD_BUS, component->bus);
    looper_pb_write_message(&writer, COMPONENT_FIELD_ADDRESS, address, address_length);
    looper_pb_write_string(&writer, COMPONENT_FIELD_LABEL, component->label);
    looper_pb_write_string(&writer, COMPONENT_FIELD_MODEL, component->model);
    /* Packed, because channel zero is an ordinary channel and the
     * singular writer would leave it out. */
    looper_pb_write_packed(
        &writer, COMPONENT_FIELD_PROPERTIES, component->properties, component->property_count
    );
    *length = writer.length;
    return !writer.overflow;
}

/*
 * Both halves of an exchange are bytes rather than text: either may hold a
 * zero and neither is terminated, so they are written as length delimited
 * fields directly.
 */
bool looper_encode_record(
    const looper_bus_record_t *record, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;
    uint8_t address[NESTED_CAPACITY];
    size_t address_length = 0u;

    if (!encode_address(record->address, address, sizeof(address), &address_length)) {
        return false;
    }
    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_BUS, record->bus);
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_KIND, (uint64_t)record->kind);
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_DIRECTION, (uint64_t)record->direction);
    looper_pb_write_message(&writer, BUS_RECORD_FIELD_ADDRESS, address, address_length);
    looper_pb_write_message(
        &writer, BUS_RECORD_FIELD_PAYLOAD, record->payload, record->payload_length
    );
    looper_pb_write_message(
        &writer, BUS_RECORD_FIELD_RESPONSE, record->response, record->response_length
    );
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_ACCEPTED, record->accepted ? 1u : 0u);
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_MICROSECONDS, record->microseconds);
    looper_pb_write_varint(&writer, BUS_RECORD_FIELD_SEQUENCE, record->sequence);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_timing(
    const looper_timing_t *timing, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_string(&writer, TIMING_FIELD_FUNCTION, timing->function);
    looper_pb_write_varint(&writer, TIMING_FIELD_CALLS, timing->calls);
    looper_pb_write_varint(&writer, TIMING_FIELD_ANSWERED, timing->answered_calls);
    looper_pb_write_varint(&writer, TIMING_FIELD_TOTAL, timing->total_nanoseconds);
    looper_pb_write_varint(&writer, TIMING_FIELD_SHORTEST, timing->shortest_nanoseconds);
    looper_pb_write_varint(&writer, TIMING_FIELD_LONGEST, timing->longest_nanoseconds);
    /* Packed, so that the empty bands survive: the position in the
     * list is what says which band a count belongs to, and the
     * singular writer leaves a zero out. */
    looper_pb_write_packed(&writer, TIMING_FIELD_BANDS, timing->bands, LOOPER_TIMING_BANDS);
    *length = writer.length;
    return !writer.overflow;
}
