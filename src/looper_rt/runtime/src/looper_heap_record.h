/**
 * @file
 * @brief What the wrappers tell the bookkeeping.
 *
 * Split from the bookkeeping on purpose. The wrappers refer to
 * `__real_malloc` and friends, which exist only in a link that was
 * asked to wrap, so a build that does not want them must be able to
 * leave them out entirely -- and a translation unit nothing references
 * is a translation unit a static library never pulls in.
 *
 * Private to the heap tracker.
 */
#ifndef LOOPER_HEAP_RECORD_H
#define LOOPER_HEAP_RECORD_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Note one block that has been handed out.
 * @param block What was returned, or NULL when the call failed.
 * @param size How much was asked for.
 * @param site Where in the running process the call was made.
 */
void looper_heap_note(void *block, size_t size, uintptr_t site);

/** @brief Give one block back, if it is one that was noted. */
void looper_heap_forget(void *block);

#endif /* LOOPER_HEAP_RECORD_H */
