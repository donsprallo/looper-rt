#include "looper_journal.h"

#include <string.h>

#include "looper_clock.h"

static looper_journal_entry_t entries[LOOPER_JOURNAL_CAPACITY];
static size_t entry_count;
static size_t next_slot;
static uint32_t next_sequence;

static size_t slot_of(size_t index)
{
    if (entry_count < LOOPER_JOURNAL_CAPACITY) {
        return index;
    }
    return (next_slot + index) % LOOPER_JOURNAL_CAPACITY;
}

void looper_journal_clear(void)
{
    entry_count = 0u;
    next_slot = 0u;
    next_sequence = 0u;
}

void looper_journal_record(const char *function)
{
    looper_journal_entry_t *entry = &entries[next_slot];

    entry->function = function;
    entry->microseconds = looper_clock_microseconds();
    entry->sequence = next_sequence;

    next_sequence++;
    next_slot = (next_slot + 1u) % LOOPER_JOURNAL_CAPACITY;
    if (entry_count < LOOPER_JOURNAL_CAPACITY) {
        entry_count++;
    }
}

size_t looper_journal_count(void)
{
    return entry_count;
}

const looper_journal_entry_t *looper_journal_at(size_t index)
{
    if (index >= entry_count) {
        return NULL;
    }
    return &entries[slot_of(index)];
}

size_t looper_journal_count_of(const char *function)
{
    size_t matches = 0u;

    if (function == NULL) {
        return 0u;
    }
    for (size_t index = 0u; index < entry_count; index++) {
        const looper_journal_entry_t *entry = &entries[slot_of(index)];
        if (entry->function != NULL && strcmp(entry->function, function) == 0) {
            matches++;
        }
    }
    return matches;
}
