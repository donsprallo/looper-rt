/**
 * @file
 * @brief The tree of call paths, which is what a flame graph is made of.
 *
 * The profile table answers "what did this function cost altogether".
 * That is the right question for a ranking and the wrong one for a
 * picture of a call stack: a function called from three places has
 * one row in the table and belongs in three places in the picture,
 * and the interesting one is usually the place rather than the
 * function.
 *
 * So this keeps a node per *path* rather than per function. The root
 * of a window has one child per function the firmware was first seen
 * in, each of those has one child per function it called, and so on.
 * A node's total is the time spent in that function reached that way.
 *
 * Why a tree rather than a list of caller and callee pairs: pairs
 * cannot be assembled back into a tree without guessing. If `read`
 * calls `wait` and `write` calls `wait`, the pairs say how much
 * `wait` cost under each -- but with three levels and a diamond they
 * stop saying anything a drawing can use. A path recorded as a path
 * needs no reconstruction and cannot be wrong.
 *
 * ## What it costs, and why it is off until asked for
 *
 * Measurably more than the aggregate profile: following the tree
 * touches memory the ranking never does, and a cache miss per call
 * is worth more than everything else the hooks do. So this is armed
 * by a client that wants a flame graph and idle otherwise, which
 * costs one test of a flag per call and leaves the everyday profile
 * as cheap as it was. The figures are in `docs/checking.md`.
 *
 * Recursion makes a chain of nodes, one per level, which is honest
 * and finite only because the table is. A table that fills stops
 * taking new paths and counts how many it turned away, because a
 * flame graph missing a branch should say so rather than look
 * complete.
 *
 * Private to the profiler.
 */
#ifndef LOOPER_PROFILE_PATHS_H
#define LOOPER_PROFILE_PATHS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LOOPER_PROFILE_PATH_CAPACITY
/**
 * @brief Distinct call paths a window holds.
 *
 * Five hundred instrumented functions reach a few thousand distinct
 * paths through them. Four thousand nodes is room for that and costs
 * 192 KB, which is looper's own memory and is reported apart from the
 * firmware's.
 */
#define LOOPER_PROFILE_PATH_CAPACITY 4096u
#endif

/** @brief The node every path hangs from, which is never a function. */
#define LOOPER_PROFILE_PATH_ROOT 0u

/** @brief Said of a node that has no child, or no next sibling. */
#define LOOPER_PROFILE_PATH_NONE 0u

/** @brief One function, reached one way. */
typedef struct {
    /** @brief Where the function sits in the running process. */
    uintptr_t address;
    /** @brief The node this was called from. */
    uint32_t parent;
    uint64_t calls;
    /** @brief Ticks between entry and return, callees included. */
    uint64_t total_ticks;
    /** @brief The same with the time spent in callees taken away. */
    uint64_t self_ticks;
} looper_profile_path_t;

/** @brief Forget every path and begin a window. */
void looper_profile_paths_clear(void);

/**
 * @brief Begin, or stop, following where the calls go.
 *
 * Arming it starts a window of its own: what was recorded before
 * nobody was watching is of no use to whoever just asked.
 */
void looper_profile_paths_watch(bool wanted);

/** @brief Whether paths are being followed at all. */
bool looper_profile_paths_are_watched(void);

/**
 * @brief The node for a function reached from a given node.
 *
 * Made the first time that path is walked, found by walking the
 * parent's children afterwards.
 *
 * @param parent Node the call was made from, or the root.
 * @param address Function being called.
 * @return The node's index, or the root when the table is full.
 */
uint32_t looper_profile_paths_enter(uint32_t parent, uintptr_t address);

/**
 * @brief Add one finished call to a node.
 * @param node Node the call was made at.
 * @param total Ticks the call took, callees included.
 * @param in_children Ticks of those callees.
 */
void looper_profile_paths_record(uint32_t node, uint64_t total, uint64_t in_children);

/** @brief How many nodes the tree holds, the root included. */
size_t looper_profile_paths_count(void);

/**
 * @brief One node by index.
 *
 * Copied rather than pointed at: the fields are kept in arrays of
 * their own so that the walk stays in cache, and a node is only a
 * structure when somebody reads one.
 *
 * @param index Position, below `looper_profile_paths_count()`.
 * @param node Where to put it.
 * @return Whether there was one at that position.
 */
bool looper_profile_paths_at(size_t index, looper_profile_path_t *node);

/** @brief Paths that could not be recorded because the table was full. */
uint64_t looper_profile_paths_lost(void);

#endif /* LOOPER_PROFILE_PATHS_H */
