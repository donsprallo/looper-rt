/*
 * Encoding what was measured of time, stack and memory.
 *
 * Kept apart from the rest of the elements because it is the half that
 * exists only in a build that was asked to measure: a plain simulation
 * encodes none of it, and a reader of the other file should not have
 * to step over it.
 */
#include "looper_pb.h"
#include "looper_protocol_elements.h"
#include "looper_protocol_fields.h"

/** @brief Encode one place that is holding something. */
static bool encode_site(
    const looper_heap_site_t *site, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, HEAP_SITE_FIELD_SITE, (uint64_t)site->site);
    looper_pb_write_varint(&writer, HEAP_SITE_FIELD_BLOCKS, site->blocks);
    looper_pb_write_varint(&writer, HEAP_SITE_FIELD_BYTES, site->bytes);
    *length = writer.length;
    return !writer.overflow;
}

bool looper_encode_heap(
    const looper_heap_totals_t *totals,
    const looper_heap_site_t *sites,
    size_t site_count,
    uint8_t *buffer,
    size_t capacity,
    size_t *length
)
{
    looper_pb_writer_t writer;
    uint8_t nested[64];

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, HEAP_FIELD_ALLOCATIONS, totals->allocations);
    looper_pb_write_varint(&writer, HEAP_FIELD_FREES, totals->frees);
    looper_pb_write_varint(&writer, HEAP_FIELD_FAILURES, totals->failures);
    looper_pb_write_varint(&writer, HEAP_FIELD_LIVE, totals->live_bytes);
    looper_pb_write_varint(&writer, HEAP_FIELD_PEAK, totals->peak_bytes);
    looper_pb_write_varint(&writer, HEAP_FIELD_UNTRACKED, totals->untracked_blocks);
    looper_pb_write_varint(&writer, HEAP_FIELD_WATCHED, totals->is_watched ? 1u : 0u);
    for (size_t index = 0u; index < site_count; index++) {
        size_t nested_length = 0u;

        if (!encode_site(&sites[index], nested, sizeof(nested), &nested_length)) {
            return false;
        }
        if (!looper_pb_write_message(
                &writer, HEAP_FIELD_OUTSTANDING, nested, nested_length
            )) {
            return false;
        }
    }
    *length = writer.length;
    return !writer.overflow;
}


bool looper_encode_profile(
    const looper_profile_entry_t *entry, uint8_t *buffer, size_t capacity, size_t *length
)
{
    looper_pb_writer_t writer;

    looper_pb_writer_init(&writer, buffer, capacity);
    looper_pb_write_varint(&writer, PROFILE_FIELD_ADDRESS, (uint64_t)entry->address);
    looper_pb_write_varint(&writer, PROFILE_FIELD_CALLS, entry->calls);
    looper_pb_write_varint(&writer, PROFILE_FIELD_TOTAL, entry->total_nanoseconds);
    looper_pb_write_varint(&writer, PROFILE_FIELD_SELF, entry->self_nanoseconds);
    looper_pb_write_varint(&writer, PROFILE_FIELD_LONGEST, entry->longest_nanoseconds);
    looper_pb_write_varint(&writer, PROFILE_FIELD_FRAME_BYTES, entry->frame_bytes);
    *length = writer.length;
    return !writer.overflow;
}
