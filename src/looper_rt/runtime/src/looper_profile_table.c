/*
 * The table, and the arithmetic that finds a place in it.
 */
#include "looper_profile_table.h"

#include <string.h>

#define GOLDEN_RATIO_32 2654435761u

/*
 * Function addresses are aligned, so their low bits are always the
 * same and would all land in the same places. Shifting them away first
 * is what spreads them.
 */
#define ADDRESS_ALIGNMENT_BITS 4u

static looper_profile_slot_t slots[LOOPER_PROFILE_CAPACITY];
/** @brief Which slots are in use, in the order they were first seen. */
static uint32_t order[LOOPER_PROFILE_CAPACITY];
static size_t occupied;
static uint64_t lost;

void looper_profile_table_clear(void)
{
    memset(slots, 0, sizeof(slots));
    occupied = 0u;
    lost = 0u;
}

looper_profile_slot_t *looper_profile_table_slot(uintptr_t address)
{
    uint32_t index = (uint32_t)((address >> ADDRESS_ALIGNMENT_BITS) * GOLDEN_RATIO_32)
                     & (LOOPER_PROFILE_CAPACITY - 1u);

    for (size_t probe = 0u; probe < LOOPER_PROFILE_CAPACITY; probe++) {
        if (slots[index].address == address) {
            return &slots[index];
        }
        if (slots[index].address == 0u) {
            slots[index].address = address;
            order[occupied] = index;
            occupied++;
            return &slots[index];
        }
        index = (index + 1u) & (LOOPER_PROFILE_CAPACITY - 1u);
    }
    return NULL;
}

size_t looper_profile_table_count(void)
{
    return occupied;
}

const looper_profile_slot_t *looper_profile_table_at(size_t index)
{
    if (index >= occupied) {
        return NULL;
    }
    return &slots[order[index]];
}

void looper_profile_table_lose(void)
{
    lost++;
}

uint64_t looper_profile_table_lost(void)
{
    return lost;
}
