/**
 * @file
 * @brief Encoding of the messages nested inside a response.
 *
 * Private to the protocol encoder. Each of these turns one value into its own
 * buffer, because a length delimited field has to know its length before its
 * content; assembling them into the response is the caller's job.
 */
#ifndef LOOPER_PROTOCOL_ELEMENTS_H
#define LOOPER_PROTOCOL_ELEMENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_heap.h"
#include "looper_profile.h"
#include "looper_timing.h"
#include "looper_types.h"

/** @brief Encode one pin. @return false when the buffer is too small. */
bool looper_encode_pin(const looper_pin_t *pin, uint8_t *buffer, size_t capacity, size_t *length);

/** @brief Encode one journal entry. @return false when the buffer is too small. */
bool looper_encode_entry(
    const looper_journal_entry_t *entry, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one analog channel. @return false when the buffer is too small. */
bool looper_encode_channel(
    const looper_channel_t *channel, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one recorded sample. @return false when the buffer is too small. */
bool looper_encode_profile_path(
    const looper_profile_path_entry_t *node, uint8_t *buffer, size_t capacity, size_t *length
);

bool looper_encode_pin_change(
    const looper_pin_change_t *change, uint8_t *buffer, size_t capacity, size_t *length
);

bool looper_encode_sample(
    const looper_sample_t *sample, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one watched variable. @return false when the buffer is too small. */
bool looper_encode_watch(
    const looper_watch_t *watch, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one bus. @return false when the buffer is too small. */
bool looper_encode_bus(
    const looper_bus_t *bus, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one attached component. @return false when the buffer is too small. */
bool looper_encode_component(
    const looper_component_t *component, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one recorded exchange. @return false when the buffer is too small. */
bool looper_encode_record(
    const looper_bus_record_t *record, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode what the allocator was asked for. @return false when too small. */
bool looper_encode_heap(
    const looper_heap_totals_t *totals,
    const looper_heap_site_t *sites,
    size_t site_count,
    uint8_t *buffer,
    size_t capacity,
    size_t *length
);

/** @brief Encode one profiled function. @return false when too small. */
bool looper_encode_profile(
    const looper_profile_entry_t *entry, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode one function's measurements. @return false when too small. */
bool looper_encode_timing(
    const looper_timing_t *timing, uint8_t *buffer, size_t capacity, size_t *length
);

/** @brief Encode the running image. @return false when the buffer is too small. */
bool looper_encode_image(
    const looper_image_t *image, uint8_t *buffer, size_t capacity, size_t *length
);

#endif /* LOOPER_PROTOCOL_ELEMENTS_H */
