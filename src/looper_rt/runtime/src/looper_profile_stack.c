/*
 * The high water mark of the stack, and the calls that were open at it.
 *
 * The baseline is wherever the stack stood at the first instrumented
 * call after a window began. Everything is measured against that, so
 * the number is "how much deeper than the top of the loop did this
 * get" rather than an absolute, which is the comparable quantity
 * anyway.
 *
 * Stacks grow downward on every machine a simulation runs on. The
 * subtraction is written to give nothing rather than something absurd
 * if that ever stops being true.
 */
#include "looper_profile_stack.h"

#include <string.h>

static const char *baseline;
static size_t deepest_bytes;
static uint32_t deepest_depth;
static bool path_is_complete = true;
static uintptr_t open_path[LOOPER_PROFILE_PATH_DEPTH];
static uintptr_t deepest_path[LOOPER_PROFILE_PATH_DEPTH];

void looper_profile_stack_clear(void)
{
    baseline = NULL;
    deepest_bytes = 0u;
    deepest_depth = 0u;
    path_is_complete = true;
    memset(open_path, 0, sizeof(open_path));
    memset(deepest_path, 0, sizeof(deepest_path));
}

/** @brief How far below the baseline the stack has reached. */
__attribute__((no_instrument_function))
static size_t below_baseline(const char *here)
{
    return (baseline > here) ? (size_t)(baseline - here) : 0u;
}

/** @brief Remember the calls that were open at a new deepest point. */
__attribute__((no_instrument_function))
static void keep_path(uint32_t depth)
{
    const uint32_t kept = (depth < LOOPER_PROFILE_PATH_DEPTH) ? depth
                                                              : LOOPER_PROFILE_PATH_DEPTH;

    memcpy(deepest_path, open_path, (size_t)kept * sizeof(open_path[0]));
    if (kept < LOOPER_PROFILE_PATH_DEPTH) {
        memset(&deepest_path[kept], 0, (LOOPER_PROFILE_PATH_DEPTH - kept) * sizeof(uintptr_t));
    }
    path_is_complete = depth <= LOOPER_PROFILE_PATH_DEPTH;
}

size_t looper_profile_stack_enter(uintptr_t address, uint32_t depth, const void *local)
{
    const char *here = local;
    size_t used;

    /*
     * The baseline is the shallowest point seen, not the first one.
     *
     * A window begins when a client says so, which may be while the
     * firmware is several calls deep. Taking the first address as the
     * top would measure everything against the middle of the stack and
     * report nothing for the frames above it. Letting the baseline
     * move up settles it within one pass of the loop.
     */
    if ((baseline == NULL) || (here > baseline)) {
        baseline = here;
    }
    if ((depth > 0u) && (depth <= LOOPER_PROFILE_PATH_DEPTH)) {
        open_path[depth - 1u] = address;
    }
    used = below_baseline(here);
    if (used > deepest_bytes) {
        deepest_bytes = used;
        deepest_depth = depth;
        keep_path(depth);
    }
    return used;
}

size_t looper_profile_stack_deepest_bytes(void)
{
    return deepest_bytes;
}

uint32_t looper_profile_stack_deepest_depth(void)
{
    return deepest_depth;
}

bool looper_profile_stack_path_is_complete(void)
{
    return path_is_complete;
}

uintptr_t looper_profile_stack_path_at(size_t index)
{
    if (index >= LOOPER_PROFILE_PATH_DEPTH) {
        return 0u;
    }
    return deepest_path[index];
}
