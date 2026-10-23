/*
 * The tree, and the walk that finds a place in it.
 *
 * Node zero is the root and is never a function. That is what lets a
 * child index of zero mean "none" without a separate flag, and it is
 * why a full table can answer with the root: a call charged to the
 * root is a call whose path was not recorded, which is counted.
 *
 * The fields are kept in arrays of their own rather than in one array
 * of structures, and that is a measurement rather than a taste. The
 * walk reads two fields of each node it passes; with the seven fields
 * laid out together it reads a different cache line per node, and a
 * tree of a few thousand nodes is nowhere near a cache. Split, the
 * same walk touches five nodes per line.
 *
 * Beside that, each node remembers the child it found last. A call
 * site in a loop calls the same function over and over, so the first
 * guess is nearly always right and the list is never walked at all.
 */
#include "looper_profile_paths.h"

#include <string.h>

/* Hot: read on every call, by the walk. */
static uintptr_t addresses[LOOPER_PROFILE_PATH_CAPACITY];
static uint32_t next_sibling[LOOPER_PROFILE_PATH_CAPACITY];
static uint32_t first_child[LOOPER_PROFILE_PATH_CAPACITY];
static uint32_t last_found[LOOPER_PROFILE_PATH_CAPACITY];

/* Cold: written once per call, read only when the window is read. */
static uint32_t parents[LOOPER_PROFILE_PATH_CAPACITY];
static uint64_t call_counts[LOOPER_PROFILE_PATH_CAPACITY];
static uint64_t total_ticks[LOOPER_PROFILE_PATH_CAPACITY];
static uint64_t self_ticks[LOOPER_PROFILE_PATH_CAPACITY];

static size_t occupied;
static uint64_t lost;
static bool watching;

void looper_profile_paths_clear(void)
{
    memset(addresses, 0, sizeof(addresses));
    memset(next_sibling, 0, sizeof(next_sibling));
    memset(first_child, 0, sizeof(first_child));
    memset(last_found, 0, sizeof(last_found));
    memset(parents, 0, sizeof(parents));
    memset(call_counts, 0, sizeof(call_counts));
    memset(total_ticks, 0, sizeof(total_ticks));
    memset(self_ticks, 0, sizeof(self_ticks));
    /* The root exists before anything is called, so that the first
     * call has somewhere to hang from. */
    occupied = 1u;
    lost = 0u;
}

void looper_profile_paths_watch(bool wanted)
{
    if (wanted && !watching) {
        looper_profile_paths_clear();
    }
    watching = wanted;
}

bool looper_profile_paths_are_watched(void)
{
    return watching;
}

/** @brief The child of this node for that function, if it is there. */
static uint32_t existing_child(uint32_t parent, uintptr_t address)
{
    const uint32_t guess = last_found[parent];

    if ((guess != LOOPER_PROFILE_PATH_NONE) && (addresses[guess] == address)) {
        return guess;
    }
    for (uint32_t child = first_child[parent]; child != LOOPER_PROFILE_PATH_NONE;
         child = next_sibling[child]) {
        if (addresses[child] == address) {
            last_found[parent] = child;
            return child;
        }
    }
    return LOOPER_PROFILE_PATH_NONE;
}

/** @brief A new child of this node, or none when the table is full. */
static uint32_t new_child(uint32_t parent, uintptr_t address)
{
    uint32_t child;

    if (occupied >= LOOPER_PROFILE_PATH_CAPACITY) {
        return LOOPER_PROFILE_PATH_NONE;
    }
    child = (uint32_t)occupied;
    occupied++;
    addresses[child] = address;
    parents[child] = parent;
    next_sibling[child] = first_child[parent];
    first_child[parent] = child;
    last_found[parent] = child;
    return child;
}

uint32_t looper_profile_paths_enter(uint32_t parent, uintptr_t address)
{
    uint32_t child;

    if (!watching) {
        return LOOPER_PROFILE_PATH_ROOT;
    }
    child = existing_child(parent, address);
    if (child != LOOPER_PROFILE_PATH_NONE) {
        return child;
    }
    child = new_child(parent, address);
    if (child == LOOPER_PROFILE_PATH_NONE) {
        lost++;
        return LOOPER_PROFILE_PATH_ROOT;
    }
    return child;
}

void looper_profile_paths_record(uint32_t node, uint64_t total, uint64_t in_children)
{
    if (node == LOOPER_PROFILE_PATH_ROOT) {
        return;
    }
    call_counts[node]++;
    total_ticks[node] += total;
    self_ticks[node] += (total > in_children) ? (total - in_children) : 0u;
}

size_t looper_profile_paths_count(void)
{
    return watching ? occupied : 0u;
}

bool looper_profile_paths_at(size_t index, looper_profile_path_t *node)
{
    if (index >= looper_profile_paths_count()) {
        return false;
    }
    node->address = addresses[index];
    node->parent = parents[index];
    node->calls = call_counts[index];
    node->total_ticks = total_ticks[index];
    node->self_ticks = self_ticks[index];
    return true;
}

uint64_t looper_profile_paths_lost(void)
{
    return lost;
}
