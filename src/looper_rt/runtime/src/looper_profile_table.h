/**
 * @file
 * @brief The table a profile is kept in, and nothing about measuring.
 *
 * One entry per function, found by the function's own address: a
 * multiply, a mask and a comparison, because this is looked up on
 * every call of every woven function and a search would cost more than
 * the measurement is worth.
 *
 * Open addressing with linear probing, and no deletion. A window ends
 * by clearing the whole table, which is the only removal there is.
 *
 * Private to the profiler.
 */
#ifndef LOOPER_PROFILE_TABLE_H
#define LOOPER_PROFILE_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "looper_profile.h"

/** @brief What is kept for one function while the window runs. */
typedef struct {
    uintptr_t address;
    uint64_t calls;
    uint64_t total_ticks;
    uint64_t self_ticks;
    uint64_t longest_ticks;
    /** @brief Largest stack frame of this function that was seen. */
    size_t frame_bytes;
    /** @brief How many frames of this function are on the stack now. */
    uint32_t active;
} looper_profile_slot_t;

/** @brief Forget everything and begin a window. */
void looper_profile_table_clear(void);

/**
 * @brief The slot of one function, made the first time it is seen.
 * @return The slot, or NULL when the table is full.
 */
looper_profile_slot_t *looper_profile_table_slot(uintptr_t address);

/** @brief How many functions the table holds. */
size_t looper_profile_table_count(void);

/**
 * @brief One slot by position, in the order the functions were seen.
 * @return The slot, or NULL when the index is past the end.
 */
const looper_profile_slot_t *looper_profile_table_at(size_t index);

/** @brief Note that a measurement had nowhere to go. */
void looper_profile_table_lose(void);

/** @brief How many measurements had nowhere to go. */
uint64_t looper_profile_table_lost(void);

#endif /* LOOPER_PROFILE_TABLE_H */
