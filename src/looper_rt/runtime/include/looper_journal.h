/**
 * @file
 * @brief Record of the adapter calls the firmware made.
 *
 * A test can install a hook to control what an adapter does. The journal
 * answers the other half of the question: whether it was called at all, and
 * when. Recording is opt in -- generated adapters only call into the journal
 * when they are compiled with `LOOPER_JOURNAL` defined.
 */
#ifndef LOOPER_JOURNAL_H
#define LOOPER_JOURNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOOPER_JOURNAL_CAPACITY
/** @brief Number of calls the journal keeps before discarding the oldest. */
#define LOOPER_JOURNAL_CAPACITY 256u
#endif

/** @brief Discard every recorded call and restart the sequence numbering. */
void looper_journal_clear(void);

/**
 * @brief Record one adapter call at the current simulated time.
 * @param function Name of the adapter function. The string is not copied, so
 *        pass a literal or another pointer that outlives the journal.
 */
void looper_journal_record(const char *function);

/** @brief Number of calls currently held. */
size_t looper_journal_count(void);

/**
 * @brief Read one recorded call.
 * @param index Position from the oldest held entry.
 * @return The entry, or NULL when the index is out of range.
 */
const looper_journal_entry_t *looper_journal_at(size_t index);

/**
 * @brief Count how often one function was recorded.
 * @param function Name to look for, compared by content.
 */
size_t looper_journal_count_of(const char *function);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_JOURNAL_H */
