#include "looper_watch.h"

#include <string.h>

#include "looper_image.h"
#include "looper_trace.h"
#include "looper_watch_memory.h"

static looper_watch_t watches[LOOPER_WATCH_CAPACITY];
static size_t watch_count;
static uint32_t next_channel = LOOPER_WATCH_CHANNEL_BASE;
static bool recorded[LOOPER_WATCH_CAPACITY];

/** @brief Find a watch by its trace channel. */
static size_t index_of(uint32_t channel, bool *found)
{
    for (size_t index = 0u; index < watch_count; index++) {
        if (watches[index].channel == channel) {
            *found = true;
            return index;
        }
    }
    *found = false;
    return 0u;
}

static void copy_path(char *destination, const char *path)
{
    size_t length;

    if (path == NULL) {
        destination[0] = '\0';
        return;
    }
    length = strlen(path);
    if (length >= LOOPER_VARIABLE_PATH_CAPACITY) {
        length = LOOPER_VARIABLE_PATH_CAPACITY - 1u;
    }
    memcpy(destination, path, length);
    destination[length] = '\0';
}

void looper_watch_clear(void)
{
    memset(watches, 0, sizeof(watches));
    memset(recorded, 0, sizeof(recorded));
    watch_count = 0u;
    next_channel = LOOPER_WATCH_CHANNEL_BASE;
}

/** @brief Describe where a watch reads, for the conversion routines. */
static looper_watch_location_t location_of(const looper_watch_t *watch)
{
    looper_watch_location_t location = {
        watch->address, watch->encoding, watch->byte_width, watch->bit_offset, watch->bit_width
    };

    return location;
}

bool looper_watch_add(
    const char *path,
    uint64_t address,
    looper_encoding_t encoding,
    uint32_t byte_width,
    uint32_t bit_offset,
    uint32_t bit_width,
    bool writable,
    const looper_watch_t **watch
)
{
    looper_watch_t *created;
    looper_watch_location_t location = {address, encoding, byte_width, bit_offset, bit_width};

    if (watch_count >= LOOPER_WATCH_CAPACITY) {
        return false;
    }
    if (!looper_watch_location_valid(&location)) {
        return false;
    }
    if (!looper_image_contains(address, byte_width)) {
        return false;
    }
    created = &watches[watch_count];
    memset(created, 0, sizeof(*created));
    created->channel = next_channel;
    copy_path(created->path, path);
    created->address = address;
    created->encoding = encoding;
    created->byte_width = byte_width;
    created->bit_offset = bit_offset;
    created->bit_width = bit_width;
    created->writable = writable;
    created->value = looper_watch_load(&location);

    recorded[watch_count] = false;
    watch_count++;
    next_channel++;
    if (watch != NULL) {
        *watch = created;
    }
    return true;
}

bool looper_watch_remove(uint32_t channel)
{
    bool found = false;
    size_t index = index_of(channel, &found);

    if (!found) {
        return false;
    }
    for (size_t next = index + 1u; next < watch_count; next++) {
        watches[next - 1u] = watches[next];
        recorded[next - 1u] = recorded[next];
    }
    watch_count--;
    memset(&watches[watch_count], 0, sizeof(watches[watch_count]));
    recorded[watch_count] = false;
    return true;
}

bool looper_watch_write(uint32_t channel, double value)
{
    bool found = false;
    size_t index = index_of(channel, &found);
    looper_watch_t *target;
    looper_watch_location_t location;

    if (!found) {
        return false;
    }
    target = &watches[index];
    if (!target->writable) {
        return false;
    }
    location = location_of(target);
    if (!looper_watch_value_fits(&location, value)) {
        return false;
    }
    looper_watch_store(&location, value);
    return true;
}

void looper_watch_poll(void)
{
    for (size_t index = 0u; index < watch_count; index++) {
        looper_watch_t *target = &watches[index];
        looper_watch_location_t location = location_of(target);
        double value = looper_watch_load(&location);

        if (recorded[index] && value == target->value) {
            continue;
        }
        target->value = value;
        recorded[index] = true;
        looper_trace_record(target->channel, value);
    }
}

size_t looper_watch_count(void)
{
    return watch_count;
}

const looper_watch_t *looper_watch_at(size_t index)
{
    if (index >= watch_count) {
        return NULL;
    }
    return &watches[index];
}
