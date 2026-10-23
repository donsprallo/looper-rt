/*
 * The values a component publishes beside what it does on its bus.
 *
 * A property is not a new kind of thing: it is a value with a name and a
 * unit that a client reads, plots and sometimes drives, which is what an
 * analog channel already is. What is kept here is only which channels
 * belong to which component, so that a client can group them.
 */
#include <stddef.h>
#include <stdint.h>

#include "looper_analog.h"
#include "looper_bus.h"

/*
 * The component at an address, as something that can be changed.
 *
 * Found through the public listing rather than through the table itself,
 * which belongs to looper_bus.c; a component is a handful per board, so
 * walking the listing costs nothing and keeps the table in one place.
 */
static looper_component_t *mutable_component(uint32_t bus, looper_bus_address_t address)
{
    const looper_component_t *found = looper_component_at_address(bus, address);

    return (looper_component_t *)(uintptr_t)found;
}

bool looper_component_publish(
    uint32_t bus,
    looper_bus_address_t address,
    uint32_t channel,
    const char *name,
    const char *unit,
    looper_channel_direction_t direction
)
{
    looper_component_t *component = mutable_component(bus, address);

    if ((component == NULL) || (component->property_count >= LOOPER_PROPERTY_CAPACITY)) {
        return false;
    }
    if (!looper_analog_register(channel, name, unit, direction)) {
        return false;
    }
    component->properties[component->property_count] = channel;
    component->property_count++;
    return true;
}

