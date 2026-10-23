/**
 * @file
 * @brief What the firmware asked the allocator for.
 *
 * Much embedded firmware is not allowed to allocate at all once it has
 * started, and the rule is usually kept by reading the code. A
 * simulation can answer it instead: the linker puts these wrappers in
 * front of the allocator, so every call passes through here and
 * "this firmware never allocates" becomes a measurement.
 *
 * ## Switching it on
 *
 * The wrappers are always compiled and are called only when the link
 * asks for them:
 *
 * ```
 * -Wl,--wrap=malloc,--wrap=free,--wrap=calloc,--wrap=realloc
 * ```
 *
 * which `memory = true` in a build definition adds. Without it nothing
 * here ever runs, and the report says so rather than claiming that
 * nothing was allocated.
 *
 * ## What it says, and about whom
 *
 * A C runtime allocates for its own reasons -- a buffer for the first
 * thing printed, something for a locale -- and those pass through here
 * too, because a wrapper cannot tell who is calling until the call
 * site is given a name. Every block therefore remembers where it was
 * allocated from, and a client sorts the firmware's own from the rest
 * by looking that address up in the debug information.
 *
 * The sizes carry over to a target. A `malloc(256)` asks for 256 bytes
 * there as well; what differs is the allocator's own overhead and how
 * badly it fragments, neither of which this claims to know.
 */
#ifndef LOOPER_HEAP_H
#define LOOPER_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LOOPER_HEAP_BLOCKS
/** @brief Live blocks that can be followed at once. */
#define LOOPER_HEAP_BLOCKS 4096u
#endif

#ifndef LOOPER_HEAP_SITES
/** @brief Distinct places that can be remembered as allocating. */
#define LOOPER_HEAP_SITES 256u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What is still out from one place in the code. */
typedef struct {
    /** @brief Where in the running process the allocation was made. */
    uintptr_t site;
    uint64_t blocks;
    uint64_t bytes;
} looper_heap_site_t;

/** @brief Everything counted about the allocator, in one value. */
typedef struct {
    uint64_t allocations;
    uint64_t frees;
    uint64_t failures;
    uint64_t live_bytes;
    uint64_t peak_bytes;
    uint64_t untracked_blocks;
    bool is_watched;
} looper_heap_totals_t;

/** @brief Take one reading of everything that is counted. */
void looper_heap_totals(looper_heap_totals_t *totals);

/**
 * @brief Begin a window: forget the counts and the peak.
 *
 * What is still out is not forgotten. A block allocated before the
 * window is still held now, and a report of what is held would be a
 * lie without it. Only the counting starts again, and the peak starts
 * from whatever is currently out.
 */
void looper_heap_clear(void);

/**
 * @brief Whether the allocator has been seen at all.
 *
 * Which is the same question as whether this build was linked with the
 * wrappers in front of it, answered by observation. A build that is
 * not watched reports nothing rather than reporting nothing allocated.
 */
bool looper_heap_is_watched(void);

/** @brief How many places are holding something. */
size_t looper_heap_site_count(void);

/**
 * @brief One place that is holding something, by position.
 * @return Whether there was one at that position.
 */
bool looper_heap_site_at(size_t index, looper_heap_site_t *site);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_HEAP_H */
