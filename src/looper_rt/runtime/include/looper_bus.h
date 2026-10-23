/**
 * @file
 * @brief Buses a firmware talks on, and the components answering on them.
 *
 * A firmware reaches most of its hardware through a bus. What happens there
 * is an exchange of bytes with something at an address, and neither half is
 * visible through pins or measurement channels, so a bus is a model of its
 * own and what sits on it is a component.
 *
 * This file holds what the four kinds share: registering a bus, attaching
 * something to it, carrying every component forward on the simulation's
 * clock, and recording what crossed. What an exchange looks like differs
 * per kind and lives in `looper_spi.h`, `looper_i2c.h`, `looper_uart.h` and
 * `looper_can.h`, each with a vtable of its own. The difference is kept
 * rather than averaged away: an SPI transfer and a CAN frame are not the
 * same thing, and pretending otherwise would make every component worse.
 *
 * A component is simulated in this process. It answers while the firmware
 * is inside the call that asked, so it must not block and must not wait for
 * anything outside; a device that needs its own clock gets a `run`, which
 * the loop calls once per iteration.
 */
#ifndef LOOPER_BUS_H
#define LOOPER_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifndef LOOPER_BUS_CAPACITY
/** @brief Maximum number of buses the model holds. */
#define LOOPER_BUS_CAPACITY 8u
#endif

#ifndef LOOPER_COMPONENT_CAPACITY
/** @brief Maximum number of components the model holds, across all buses. */
#define LOOPER_COMPONENT_CAPACITY 16u
#endif

#ifndef LOOPER_BUS_TRACE_CAPACITY
/**
 * @brief Exchanges kept in the recording before the oldest is discarded.
 *
 * A bus can be fast. A converter sampling at eight kilohertz is eight
 * thousand transfers a second, and no ring and no protocol will carry all
 * of that to a client that polls. What this depth buys is that a client
 * which asks often enough sees a continuous window rather than fragments;
 * a project that wants more can define it larger.
 */
#define LOOPER_BUS_TRACE_CAPACITY 4096u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Forget every bus, every component and everything recorded. */
void looper_bus_clear(void);

/**
 * @brief Register a bus so that clients can see and address it.
 *
 * Registering a bus that is already known updates its description and
 * keeps whatever is attached to it.
 *
 * @param bus Opaque bus number the firmware's binding addresses it by.
 * @param kind Which kind of bus it is.
 * @param label Readable name, truncated to LOOPER_LABEL_CAPACITY - 1 bytes.
 * @retval true The bus is registered.
 * @retval false The model is full.
 */
bool looper_bus_register(uint32_t bus, looper_bus_kind_t kind, const char *label);

/**
 * @brief Record the rate the firmware configured on a bus.
 * @retval true The bus is known and the rate was stored.
 */
bool looper_bus_set_bitrate(uint32_t bus, uint32_t bitrate_bps);

/** @brief The kind of a bus, or unspecified when it is not registered. */
looper_bus_kind_t looper_bus_kind_of(uint32_t bus);

/** @brief Number of registered buses. */
size_t looper_bus_count(void);

/**
 * @brief Read one registered bus by position.
 * @return The bus, or NULL when the index is out of range.
 */
const looper_bus_t *looper_bus_at(size_t index);

/**
 * @brief Attach a component to a bus.
 *
 * Used by the four bus kinds rather than by a device model directly; each
 * offers an `attach` of its own that takes the vtable it understands.
 *
 * @param bus Bus to attach to, which must already be registered.
 * @param address Where on that bus the component answers.
 * @param label Readable name, truncated to LOOPER_LABEL_CAPACITY - 1 bytes.
 * @param model What is being simulated, for example "as8510".
 * @param instance Component state, owned by the caller.
 * @param operations Vtable of the bus's kind, owned by the caller.
 * @param run Called once per loop iteration, or NULL when not needed.
 * @retval true The component is attached.
 * @retval false The bus is unknown, the address is taken, or the model is full.
 */
bool looper_component_attach(
    uint32_t bus,
    looper_bus_address_t address,
    const char *label,
    const char *model,
    void *instance,
    const void *operations,
    void (*run)(void *instance, uint64_t microseconds)
);

/**
 * @brief The component answering at one address, or NULL when there is none.
 */
const looper_component_t *looper_component_at_address(
    uint32_t bus, looper_bus_address_t address
);

/** @brief Number of attached components, across all buses. */
size_t looper_component_count(void);

/**
 * @brief Read one attached component by position.
 * @return The component, or NULL when the index is out of range.
 */
const looper_component_t *looper_component_at(size_t index);

/**
 * @brief The vtable of a component that is not simulated in this process.
 *
 * A client can take the part of a peer on a bus the firmware does not
 * master, answering over the control channel. Nothing here calls it: it
 * learns what crossed from the recording and answers by putting something
 * back, so what it needs from the simulation is to be listed and to have
 * its address taken.
 */
extern const void *const LOOPER_COMPONENT_REMOTE;

/**
 * @brief Attach a component a client takes the part of.
 *
 * Refused on SPI and I2C. There the firmware is the master and expects an
 * answer inside the call it made, which nothing on the other end of a
 * socket can give in time.
 *
 * @retval true The component is attached and will be listed.
 */
bool looper_component_attach_remote(
    uint32_t bus, looper_bus_address_t address, const char *label, const char *model
);

/**
 * @brief Remove a component, by where it sits.
 * @retval true A component was removed.
 */
bool looper_component_detach(uint32_t bus, looper_bus_address_t address);

/**
 * @brief Publish one value a component holds, as a named channel.
 *
 * A property is an analog channel: a client sees it beside the firmware's
 * own, plots it, and drives it when the component reads it rather than
 * writes it. The component uses `looper_analog_read` and
 * `looper_analog_write` on the channel number it chose.
 *
 * @param bus Bus the component is attached to.
 * @param address Where on that bus it answers.
 * @param channel Channel number to publish it as.
 * @param name Readable name, shown as the channel's label.
 * @param unit Unit the value is measured in.
 * @param direction Whether the component reads the channel or writes it.
 * @retval true The property is published.
 * @retval false There is no such component, or it holds all it can.
 */
bool looper_component_publish(
    uint32_t bus,
    looper_bus_address_t address,
    uint32_t channel,
    const char *name,
    const char *unit,
    looper_channel_direction_t direction
);

/**
 * @brief Carry every component forward to the current simulated time.
 *
 * A component that measures on its own clock, or that answers after a
 * delay, cannot do either inside a call the firmware made. This is where
 * it happens, once per iteration of the firmware loop.
 */
void looper_component_run(void);

/** @brief Discard every recorded exchange and restart the numbering. */
void looper_bus_trace_clear(void);

/**
 * @brief Record one exchange at the current simulated time.
 *
 * @param bus Bus the exchange crossed.
 * @param direction Whether the firmware started it or received it.
 * @param address Where on the bus it was addressed.
 * @param payload Bytes the firmware sent, may be NULL.
 * @param payload_length Number of those bytes.
 * @param response Bytes that came back, may be NULL.
 * @param response_length Number of those bytes.
 * @param accepted Whether anything on the bus took the exchange.
 */
void looper_bus_record(
    uint32_t bus,
    looper_bus_direction_t direction,
    looper_bus_address_t address,
    const uint8_t *payload,
    size_t payload_length,
    const uint8_t *response,
    size_t response_length,
    bool accepted
);

/** @brief Number of recorded exchanges currently held. */
size_t looper_bus_trace_count(void);

/**
 * @brief Read one recorded exchange by position, oldest first.
 * @return The record, or NULL when the index is out of range.
 */
const looper_bus_record_t *looper_bus_trace_at(size_t index);

/** @brief Sequence number the next recorded exchange will carry. */
uint32_t looper_bus_trace_next_sequence(void);

/**
 * @brief Copy the records from a sequence number onwards.
 * @param after Sequence number the client has already seen.
 * @param destination Destination array.
 * @param capacity Number of records the destination holds.
 * @return Number of records copied.
 */
size_t looper_bus_trace_after(
    uint32_t after, looper_bus_record_t *destination, size_t capacity
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_BUS_H */
