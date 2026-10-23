#include "looper_image.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <link.h>
#include <unistd.h>
#endif

static looper_image_t image;
static bool resolved;

#if defined(_WIN32)

static void resolve_image(looper_image_t *target)
{
    const uint8_t *base = (const uint8_t *)GetModuleHandleW(NULL);
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS *headers;

    if (base == NULL || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    headers = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (headers->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }
    target->runtime_base = (uint64_t)(uintptr_t)base;
    target->size = (uint64_t)headers->OptionalHeader.SizeOfImage;
    GetModuleFileNameA(NULL, target->executable, (DWORD)sizeof(target->executable));
}

#else

/** @brief Lowest and highest address the loadable segments of an image span. */
static void segment_bounds(const struct dl_phdr_info *info, uint64_t *lowest, uint64_t *highest)
{
    for (uint16_t index = 0u; index < info->dlpi_phnum; index++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[index];
        uint64_t start = (uint64_t)header->p_vaddr;

        if (header->p_type != PT_LOAD) {
            continue;
        }
        if (start < *lowest) {
            *lowest = start;
        }
        if (start + header->p_memsz > *highest) {
            *highest = start + (uint64_t)header->p_memsz;
        }
    }
}

/*
 * The first image dl_iterate_phdr reports is the main program, which is the
 * one carrying the firmware. Returning non zero stops the walk there.
 */
static int collect_main_image(struct dl_phdr_info *info, size_t size, void *context)
{
    looper_image_t *target = context;
    uint64_t lowest = UINT64_MAX;
    uint64_t highest = 0u;

    (void)size;
    segment_bounds(info, &lowest, &highest);
    if (lowest == UINT64_MAX) {
        return 1;
    }
    target->runtime_base = (uint64_t)info->dlpi_addr + lowest;
    target->size = highest - lowest;
    return 1;
}

static void resolve_image(looper_image_t *target)
{
    ssize_t written;

    (void)dl_iterate_phdr(collect_main_image, target);
    written = readlink("/proc/self/exe", target->executable, sizeof(target->executable) - 1u);
    if (written > 0) {
        target->executable[written] = '\0';
    }
}

#endif

const looper_image_t *looper_image(void)
{
    if (!resolved) {
        memset(&image, 0, sizeof(image));
        resolve_image(&image);
        resolved = true;
    }
    return &image;
}

bool looper_image_contains(uint64_t address, uint64_t length)
{
    const looper_image_t *current = looper_image();
    uint64_t last;

    if (current->size == 0u || length == 0u) {
        return false;
    }
    if (address < current->runtime_base) {
        return false;
    }
    last = address + length;
    if (last < address) {
        return false;
    }
    return last <= current->runtime_base + current->size;
}
