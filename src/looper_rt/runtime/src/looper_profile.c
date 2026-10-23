/*
 * What a client is told about a profile.
 *
 * Three pieces sit behind this one: the table the measurements live in,
 * the hooks the compiler calls, and the pair of clocks that turns ticks
 * into nanoseconds. None of them is anybody else's business, so this is
 * the only one of the four with a header that leaves the runtime.
 */
#include "looper_profile.h"

#include "looper_profile_clock.h"
#include "looper_profile_stack.h"
#include "looper_profile_paths.h"
#include "looper_profile_table.h"

/** @brief Begin the shadow stack again. Implemented by the hooks. */
void looper_profile_hooks_restart(void);

void looper_profile_clear(void)
{
    looper_profile_table_clear();
    looper_profile_paths_clear();
    looper_profile_hooks_restart();
    looper_profile_stack_clear();
    looper_profile_clock_restart();
}

/*
 * Whether instrumented code is running at all.
 *
 * Asked of the table rather than of a compile time switch, because the
 * switch belongs to the firmware's build and this unit belongs to the
 * runtime's. One instrumented function entering is proof enough, and it
 * is the thing a client actually wants to know: an empty profile from a
 * build that was never woven looks exactly like one from a window in
 * which nothing ran, and neither is worth drawing.
 */
bool looper_profile_is_active(void)
{
    return looper_profile_table_count() > 0u;
}

size_t looper_profile_count(void)
{
    return looper_profile_table_count();
}

uint64_t looper_profile_lost(void)
{
    return looper_profile_table_lost();
}

size_t looper_profile_deepest_stack(void)
{
    return looper_profile_stack_deepest_bytes();
}

size_t looper_profile_deepest_depth(void)
{
    return (size_t)looper_profile_stack_deepest_depth();
}

bool looper_profile_path_is_complete(void)
{
    return looper_profile_stack_path_is_complete();
}

uintptr_t looper_profile_path_at(size_t index)
{
    return looper_profile_stack_path_at(index);
}

void looper_profile_watch_paths(bool wanted)
{
    looper_profile_paths_watch(wanted);
}

bool looper_profile_watches_paths(void)
{
    return looper_profile_paths_are_watched();
}

size_t looper_profile_path_count(void)
{
    return looper_profile_paths_count();
}

bool looper_profile_path_entry_at(size_t index, looper_profile_path_entry_t *entry)
{
    const double scale = looper_profile_nanoseconds_per_tick();
    looper_profile_path_t node;

    if ((entry == NULL) || !looper_profile_paths_at(index, &node)) {
        return false;
    }
    entry->address = node.address;
    entry->parent = node.parent;
    entry->calls = node.calls;
    entry->total_nanoseconds = (uint64_t)((double)node.total_ticks * scale);
    entry->self_nanoseconds = (uint64_t)((double)node.self_ticks * scale);
    return true;
}

uint64_t looper_profile_paths_not_recorded(void)
{
    return looper_profile_paths_lost();
}

bool looper_profile_at(size_t index, looper_profile_entry_t *entry)
{
    const double scale = looper_profile_nanoseconds_per_tick();
    const looper_profile_slot_t *slot = looper_profile_table_at(index);

    if ((entry == NULL) || (slot == NULL)) {
        return false;
    }
    entry->address = slot->address;
    entry->calls = slot->calls;
    entry->total_nanoseconds = (uint64_t)((double)slot->total_ticks * scale);
    entry->self_nanoseconds = (uint64_t)((double)slot->self_ticks * scale);
    entry->longest_nanoseconds = (uint64_t)((double)slot->longest_ticks * scale);
    entry->frame_bytes = (uint64_t)slot->frame_bytes;
    return true;
}
