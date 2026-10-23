/*
 * The two functions the compiler calls, and the shadow stack between them.
 *
 * Everything here runs on every call of every woven function, which is
 * why nothing in it allocates, locks, copies a string or reads a slow
 * clock. What it does is push a frame, read a cheap counter, and on
 * the way back subtract what the callees took.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_profile_clock.h"
#include "looper_profile_paths.h"
#include "looper_profile_stack.h"
#include "looper_profile_table.h"

/** @brief One frame of the shadow stack. */
typedef struct {
    looper_profile_slot_t *slot;
    /**
     * @brief Where this call sits in the tree of paths.
     *
     * The slot says which function; this says which way it was
     * reached, which is what a flame graph is drawn from.
     */
    uint32_t node;
    uint64_t entered;
    uint64_t in_children;
    /** @brief Stack in use when this frame was entered. */
    size_t stack_used;
} frame_t;

static frame_t stack[LOOPER_PROFILE_DEPTH];
static uint32_t depth;
static uint32_t frames_too_deep;
static bool window_open;

void looper_profile_hooks_restart(void)
{
    depth = 0u;
    frames_too_deep = 0u;
    window_open = true;
}

/*
 * Charge a function for its own frame.
 *
 * The compiler sets a frame up in the prologue, before it calls this
 * hook, so the stack in use at the hook already includes the frame of
 * the function being entered. The difference from the caller's figure
 * is therefore that function's own frame -- including, and this is the
 * part worth having, any large buffer it declares. A function that
 * calls nothing is measured too, because it is measured at its own
 * entry rather than at a callee's.
 */
__attribute__((no_instrument_function))
static void charge_frame(size_t used)
{
    const size_t caller_used = (depth > 0u) ? stack[depth - 1u].stack_used : 0u;
    const size_t frame = (used > caller_used) ? (used - caller_used) : 0u;
    looper_profile_slot_t *slot = stack[depth].slot;

    stack[depth].stack_used = used;
    if ((slot != NULL) && (frame > slot->frame_bytes)) {
        slot->frame_bytes = frame;
    }
}

/** @brief The node the call being entered hangs from. */
__attribute__((no_instrument_function))
static uint32_t _parent_node(void)
{
    return (depth > 0u) ? stack[depth - 1u].node : LOOPER_PROFILE_PATH_ROOT;
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
    const char here = 0;

    (void)call_site;
    if (!window_open) {
        looper_profile_clock_restart();
        window_open = true;
    }
    if (depth >= LOOPER_PROFILE_DEPTH) {
        frames_too_deep++;
        return;
    }
    /* A frame is pushed even when the table is full, so that the stack
     * stays in step with the calls. Only its measurement is lost. */
    stack[depth].slot = looper_profile_table_slot((uintptr_t)this_fn);
    stack[depth].node = looper_profile_paths_enter(_parent_node(), (uintptr_t)this_fn);
    stack[depth].in_children = 0u;
    if (stack[depth].slot == NULL) {
        looper_profile_table_lose();
    } else {
        stack[depth].slot->active++;
    }
    charge_frame(looper_profile_stack_enter((uintptr_t)this_fn, depth + 1u, &here));
    stack[depth].entered = looper_profile_ticks();
    depth++;
}

/*
 * Put one finished frame into its function's measurements.
 *
 * A recursive function would otherwise count the same nanosecond once
 * per frame it has on the stack, so only the outermost frame adds to
 * the total. The self time of every frame is its own and adds up
 * whatever the recursion does.
 */
__attribute__((no_instrument_function))
static void record_frame(const frame_t *frame, uint64_t spent)
{
    looper_profile_slot_t *slot = frame->slot;

    if (slot == NULL) {
        return;
    }
    slot->calls++;
    slot->self_ticks += (spent > frame->in_children) ? (spent - frame->in_children) : 0u;
    slot->active--;
    if (slot->active > 0u) {
        return;
    }
    slot->total_ticks += spent;
    if (spent > slot->longest_ticks) {
        slot->longest_ticks = spent;
    }
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
    const uint64_t left = looper_profile_ticks();
    frame_t *frame;
    uint64_t spent;

    (void)this_fn;
    (void)call_site;
    if (frames_too_deep > 0u) {
        frames_too_deep--;
        return;
    }
    if (depth == 0u) {
        return;
    }
    depth--;
    frame = &stack[depth];
    spent = (left > frame->entered) ? (left - frame->entered) : 0u;
    record_frame(frame, spent);
    looper_profile_paths_record(frame->node, spent, frame->in_children);
    if (depth > 0u) {
        stack[depth - 1u].in_children += spent;
    }
}
