/**
 * @file
 * @brief How deep the stack went, measured from where the hooks stand.
 *
 * The address of a local in the entry hook is, to within a frame, where
 * the stack had got to when that function was entered. The difference
 * between that and the same address at the first call ever made is how
 * much stack is in use; the largest such difference is the mark worth
 * keeping, together with the calls that were open at the time.
 *
 * It costs about four nanoseconds a call on top of the profiler's own
 * forty, which is why it is not a separate switch: a build that is
 * woven for a profile may as well say how deep it went.
 *
 * Private to the profiler.
 */
#ifndef LOOPER_PROFILE_STACK_H
#define LOOPER_PROFILE_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LOOPER_PROFILE_PATH_DEPTH
/**
 * @brief Frames the deepest path is remembered to.
 *
 * A path longer than this is reported as cut short rather than
 * quietly shortened: a reader who cannot see the bottom of the stack
 * should be told that is what they are looking at.
 */
#define LOOPER_PROFILE_PATH_DEPTH 64u
#endif

/** @brief Begin again: forget the mark and take a new baseline. */
void looper_profile_stack_clear(void);

/**
 * @brief Note that a function has been entered.
 * @param address Where that function sits.
 * @param depth Frames on the stack including this one.
 * @param local Address of a local of the hook, which stands for where
 *        the stack has reached.
 * @return Stack bytes in use at this point, for the caller to keep.
 */
size_t looper_profile_stack_enter(uintptr_t address, uint32_t depth, const void *local);

/** @brief Stack bytes in use at the deepest moment seen. */
size_t looper_profile_stack_deepest_bytes(void);

/** @brief Frames open at that moment. */
uint32_t looper_profile_stack_deepest_depth(void);

/** @brief Whether the whole of that path was remembered. */
bool looper_profile_stack_path_is_complete(void);

/**
 * @brief One function of the deepest path, outermost first.
 * @return Its address, or zero when the index is past the path.
 */
uintptr_t looper_profile_stack_path_at(size_t index);

#endif /* LOOPER_PROFILE_STACK_H */
