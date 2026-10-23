/*
 * Buses and the components attached to them.
 *
 * Both tables are fixed arrays, like every other model in the runtime: a
 * simulation describes a board, and a board has as many buses and devices
 * as it has. Nothing here allocates, and every component's state belongs to
 * whoever registered it.
 */
#include "looper_bus.h"

#include <string.h>

#include "looper_analog.h"
#include "looper_clock.h"

/*
 * The marker a component carries instead of a vtable when a client is
 * taking its part. A distinct address rather than NULL, so that a missing
 * vtable stays the mistake it is and a remote component stays a decision.
 */
static const int remote_marker;
const void *const LOOPER_COMPONENT_REMOTE = &remote_marker;

static looper_bus_t buses[LOOPER_BUS_CAPACITY];
static size_t bus_count;
static looper_component_t components[LOOPER_COMPONENT_CAPACITY];
static size_t component_count;

static looper_bus_t *find_bus(uint32_t bus)
{
    for (size_t index = 0u; index < bus_count; index++) {
        if (buses[index].id == bus) {
            return &buses[index];
        }
    }
    return NULL;
}

static void copy_text(char *destination, size_t capacity, const char *text)
{
    if (text == NULL) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, text, capacity - 1u);
    destination[capacity - 1u] = '\0';
}

void looper_bus_clear(void)
{
    bus_count = 0u;
    component_count = 0u;
    memset(buses, 0, sizeof(buses));
    memset(components, 0, sizeof(components));
    looper_bus_trace_clear();
}

bool looper_bus_register(uint32_t bus, looper_bus_kind_t kind, const char *label)
{
    looper_bus_t *existing = find_bus(bus);

    if (existing == NULL) {
        if (bus_count >= LOOPER_BUS_CAPACITY) {
            return false;
        }
        existing = &buses[bus_count];
        memset(existing, 0, sizeof(*existing));
        existing->id = bus;
        bus_count++;
    }
    existing->kind = kind;
    copy_text(existing->label, LOOPER_LABEL_CAPACITY, label);
    return true;
}

bool looper_bus_set_bitrate(uint32_t bus, uint32_t bitrate_bps)
{
    looper_bus_t *found = find_bus(bus);

    if (found == NULL) {
        return false;
    }
    found->bitrate_bps = bitrate_bps;
    return true;
}

looper_bus_kind_t looper_bus_kind_of(uint32_t bus)
{
    const looper_bus_t *found = find_bus(bus);

    return (found == NULL) ? LOOPER_BUS_KIND_UNSPECIFIED : found->kind;
}

size_t looper_bus_count(void)
{
    return bus_count;
}

const looper_bus_t *looper_bus_at(size_t index)
{
    if (index >= bus_count) {
        return NULL;
    }
    return &buses[index];
}

static bool same_address(looper_bus_address_t left, looper_bus_address_t right)
{
    return (left.value == right.value) && (left.extended == right.extended);
}

const looper_component_t *looper_component_at_address(
    uint32_t bus, looper_bus_address_t address
)
{
    for (size_t index = 0u; index < component_count; index++) {
        const looper_component_t *component = &components[index];

        if ((component->bus == bus) && same_address(component->address, address)) {
            return component;
        }
    }
    return NULL;
}

bool looper_component_attach(
    uint32_t bus,
    looper_bus_address_t address,
    const char *label,
    const char *model,
    void *instance,
    const void *operations,
    void (*run)(void *instance, uint64_t microseconds)
)
{
    looper_component_t *attached;

    if ((find_bus(bus) == NULL) || (operations == NULL)) {
        return false;
    }
    /*
     * Two devices at one address is a wiring mistake on a board and a
     * silent mystery in a simulation, so it is refused rather than letting
     * the first or the last one win.
     */
    if (looper_component_at_address(bus, address) != NULL) {
        return false;
    }
    if (component_count >= LOOPER_COMPONENT_CAPACITY) {
        return false;
    }
    attached = &components[component_count];
    memset(attached, 0, sizeof(*attached));
    attached->bus = bus;
    attached->address = address;
    copy_text(attached->label, LOOPER_LABEL_CAPACITY, label);
    copy_text(attached->model, LOOPER_MODEL_CAPACITY, model);
    attached->instance = instance;
    attached->operations = operations;
    attached->run = run;
    component_count++;
    return true;
}

size_t looper_component_count(void)
{
    return component_count;
}

const looper_component_t *looper_component_at(size_t index)
{
    if (index >= component_count) {
        return NULL;
    }
    return &components[index];
}

bool looper_component_attach_remote(
    uint32_t bus, looper_bus_address_t address, const char *label, const char *model
)
{
    const looper_bus_kind_t kind = looper_bus_kind_of(bus);

    if ((kind == LOOPER_BUS_KIND_SPI) || (kind == LOOPER_BUS_KIND_I2C)) {
        return false;
    }
    return looper_component_attach(
        bus, address, label, model, NULL, LOOPER_COMPONENT_REMOTE, NULL
    );
}

bool looper_component_detach(uint32_t bus, looper_bus_address_t address)
{
    for (size_t index = 0u; index < component_count; index++) {
        if ((components[index].bus != bus) ||
            !same_address(components[index].address, address)) {
            continue;
        }
        component_count--;
        components[index] = components[component_count];
        memset(&components[component_count], 0, sizeof(components[component_count]));
        return true;
    }
    return false;
}

void looper_component_run(void)
{
    const uint64_t now = looper_clock_microseconds();

    for (size_t index = 0u; index < component_count; index++) {
        const looper_component_t *component = &components[index];

        if (component->run != NULL) {
            component->run(component->instance, now);
        }
    }
}
