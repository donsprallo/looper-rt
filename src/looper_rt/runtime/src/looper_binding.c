/*
 * Which bus and which address an adapter instance stands for.
 *
 * A short table, walked linearly: a board has a handful of devices, and a
 * lookup happens once per adapter call, so the cost is a few comparisons
 * against a cache line.
 */
#include "looper_binding.h"

#include <string.h>

#include "looper_bus.h"

/** @brief One adapter instance and the place it stands for. */
typedef struct {
    bool used;
    const void *instance;
    uint32_t bus;
    uint32_t address;
} placement_t;

static placement_t placements[LOOPER_BINDING_CAPACITY];

void looper_binding_clear(void)
{
    memset(placements, 0, sizeof(placements));
}

bool looper_binding_bind(const void *instance, uint32_t bus, uint32_t address)
{
    placement_t *free_slot = NULL;

    for (size_t index = 0u; index < LOOPER_BINDING_CAPACITY; index++) {
        placement_t *placement = &placements[index];

        if (placement->used && (placement->instance == instance)) {
            free_slot = placement;
            break;
        }
        if (!placement->used && (free_slot == NULL)) {
            free_slot = placement;
        }
    }
    if (free_slot == NULL) {
        return false;
    }
    free_slot->used = true;
    free_slot->instance = instance;
    free_slot->bus = bus;
    free_slot->address = address;
    return true;
}

/** @brief The placement of an instance, or NULL when it was never bound. */
static const placement_t *placement_of(const void *instance)
{
    for (size_t index = 0u; index < LOOPER_BINDING_CAPACITY; index++) {
        const placement_t *placement = &placements[index];

        if (placement->used && (placement->instance == instance)) {
            return placement;
        }
    }
    return NULL;
}

/*
 * The only bus of one kind, when a board has only one.
 *
 * Two of a kind and the guess would be a coin toss, so there is none: the
 * project binds, and a generated binding that cannot resolve reports a
 * failure rather than reaching for whichever bus came first.
 */
static bool only_bus_of(looper_bus_kind_t kind, uint32_t *bus)
{
    bool found = false;

    for (size_t index = 0u; index < looper_bus_count(); index++) {
        const looper_bus_t *candidate = looper_bus_at(index);

        if (candidate->kind != kind) {
            continue;
        }
        if (found) {
            return false;
        }
        *bus = candidate->id;
        found = true;
    }
    return found;
}

/*
 * The only component on a bus, when a bus has only one.
 *
 * The second half of the same narrow guess: a board with one device on a
 * bus means that one, and saying so would be noise. Two and there is no
 * guess to make, so the address stays zero and whoever cares binds.
 */
static bool only_component_on(uint32_t bus, uint32_t *address)
{
    bool found = false;

    for (size_t index = 0u; index < looper_component_count(); index++) {
        const looper_component_t *candidate = looper_component_at(index);

        if (candidate->bus != bus) {
            continue;
        }
        if (found) {
            return false;
        }
        *address = candidate->address.value;
        found = true;
    }
    return found;
}

bool looper_binding_resolve(
    const void *instance, looper_bus_kind_t kind, uint32_t *bus, uint32_t *address
)
{
    const placement_t *placement = placement_of(instance);

    if ((bus == NULL) || (address == NULL)) {
        return false;
    }
    if (placement != NULL) {
        *bus = placement->bus;
        *address = placement->address;
        return true;
    }
    if (!only_bus_of(kind, bus)) {
        return false;
    }
    *address = 0u;
    (void)only_component_on(*bus, address);
    return true;
}
