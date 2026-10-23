/*
 * The four functions the linker puts in front of the allocator.
 *
 * Nothing else is in this file, and that is the point: it refers to
 * `__real_malloc` and its three companions, which a linker only
 * provides when it was asked to wrap. A build that does not ask never
 * references `__wrap_malloc`, so this unit is never pulled in and
 * those references never have to be resolved.
 *
 * The return address is taken here rather than deeper, because here is
 * where the caller still is: one level further in it would be this
 * file every time.
 */
#include <stddef.h>
#include <stdint.h>

#include "looper_heap_record.h"

void *__real_malloc(size_t size);
void __real_free(void *block);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *block, size_t size);

void *__wrap_malloc(size_t size)
{
    void *block = __real_malloc(size);

    looper_heap_note(block, size, (uintptr_t)__builtin_return_address(0));
    return block;
}

void __wrap_free(void *block)
{
    looper_heap_forget(block);
    __real_free(block);
}

void *__wrap_calloc(size_t count, size_t size)
{
    void *block = __real_calloc(count, size);

    looper_heap_note(block, count * size, (uintptr_t)__builtin_return_address(0));
    return block;
}

void *__wrap_realloc(void *block, size_t size)
{
    void *moved = __real_realloc(block, size);

    looper_heap_forget(block);
    looper_heap_note(moved, size, (uintptr_t)__builtin_return_address(0));
    return moved;
}
