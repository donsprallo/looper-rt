/*
 * The four wrappers, and the two tables behind them.
 *
 * One table follows blocks so that a free knows what it is giving
 * back; the other gathers what is still out by the place it came from,
 * which is the question anyone asks of a leak. Both are open addressed
 * and fixed: an allocator tracker that allocates would be answering a
 * question by asking it again.
 *
 * A block the first table has no room for is still counted, and freed
 * correctly -- it simply cannot be attributed. Saying so is better
 * than a live figure that drifts.
 *
 * The wrappers themselves are in `looper_heap_wrap.c`, because they
 * refer to symbols that only a wrapping link provides.
 */
#include "looper_heap.h"

#include <string.h>

#include "looper_heap_record.h"

#define GOLDEN_RATIO_32 2654435761u
#define POINTER_ALIGNMENT_BITS 4u

/** @brief One block that has been handed out and not given back. */
typedef struct {
    void *block;
    size_t size;
    uintptr_t site;
} entry_t;

static entry_t blocks[LOOPER_HEAP_BLOCKS];
static looper_heap_site_t sites[LOOPER_HEAP_SITES];
static size_t site_count;

static uint64_t allocations;
static uint64_t frees;
static uint64_t failures;
static uint64_t untracked;
static uint64_t unattributed;
static uint64_t live_bytes;
static uint64_t peak_bytes;
static bool watched;

/** @brief Where in the table a pointer belongs. */
static size_t slot_of(const void *block)
{
    const uintptr_t value = (uintptr_t)block;

    return (size_t)(((uint32_t)(value >> POINTER_ALIGNMENT_BITS) * GOLDEN_RATIO_32)
                    & (LOOPER_HEAP_BLOCKS - 1u));
}

/** @brief The place in the site table for one address, made on demand. */
static looper_heap_site_t *site_for(uintptr_t address)
{
    for (size_t index = 0u; index < site_count; index++) {
        if (sites[index].site == address) {
            return &sites[index];
        }
    }
    if (site_count >= LOOPER_HEAP_SITES) {
        return NULL;
    }
    sites[site_count].site = address;
    site_count++;
    return &sites[site_count - 1u];
}

void looper_heap_note(void *block, size_t size, uintptr_t address)
{
    looper_heap_site_t *site;

    watched = true;
    if (block == NULL) {
        failures++;
        return;
    }
    allocations++;
    live_bytes += size;
    if (live_bytes > peak_bytes) {
        peak_bytes = live_bytes;
    }
    site = site_for(address);
    if (site != NULL) {
        site->blocks++;
        site->bytes += size;
    }
    for (size_t probe = 0u; probe < LOOPER_HEAP_BLOCKS; probe++) {
        const size_t index = (slot_of(block) + probe) & (LOOPER_HEAP_BLOCKS - 1u);

        if (blocks[index].block == NULL) {
            blocks[index].block = block;
            blocks[index].size = size;
            blocks[index].site = address;
            return;
        }
    }
    unattributed++;
}

void looper_heap_forget(void *block)
{
    watched = true;
    if (block == NULL) {
        return;
    }
    for (size_t probe = 0u; probe < LOOPER_HEAP_BLOCKS; probe++) {
        const size_t index = (slot_of(block) + probe) & (LOOPER_HEAP_BLOCKS - 1u);
        looper_heap_site_t *site;

        if (blocks[index].block != block) {
            continue;
        }
        site = site_for(blocks[index].site);
        if (site != NULL) {
            site->blocks--;
            site->bytes -= blocks[index].size;
        }
        live_bytes -= blocks[index].size;
        frees++;
        memset(&blocks[index], 0, sizeof(blocks[index]));
        return;
    }
    untracked++;
}

void looper_heap_clear(void)
{
    allocations = 0u;
    frees = 0u;
    failures = 0u;
    untracked = 0u;
    unattributed = 0u;
    peak_bytes = live_bytes;
}

void looper_heap_totals(looper_heap_totals_t *totals)
{
    if (totals == NULL) {
        return;
    }
    totals->allocations = allocations;
    totals->frees = frees;
    totals->failures = failures;
    totals->live_bytes = live_bytes;
    totals->peak_bytes = peak_bytes;
    totals->untracked_blocks = untracked + unattributed;
    totals->is_watched = watched;
}

bool looper_heap_is_watched(void)
{
    return watched;
}

size_t looper_heap_site_count(void)
{
    return site_count;
}

bool looper_heap_site_at(size_t index, looper_heap_site_t *site)
{
    if ((site == NULL) || (index >= site_count)) {
        return false;
    }
    *site = sites[index];
    return true;
}
