/*
 * The commands that answer about buses and components.
 *
 * The recording is read the same way the analog trace is: a client
 * remembers the sequence number it last saw and asks for what came after,
 * so a continuous view is possible over a request and response protocol.
 */
#include "looper_commands_bus.h"

#include <string.h>

#include "looper_bus.h"
#include "looper_can.h"
#include "looper_uart.h"

#define MAX_BUSES_PER_RESPONSE LOOPER_BUS_CAPACITY
#define MAX_COMPONENTS_PER_RESPONSE LOOPER_COMPONENT_CAPACITY
#define MAX_EXCHANGES_PER_RESPONSE 128u

/** @brief Widest value each kind of address can carry. */
#define MAXIMUM_I2C_ADDRESS 0x7Fu
#define MAXIMUM_PORT 0xFFu
#define MAXIMUM_STANDARD_ID 0x7FFu
#define MAXIMUM_EXTENDED_ID 0x1FFFFFFFu

static looper_bus_t bus_buffer[MAX_BUSES_PER_RESPONSE];
static looper_component_t component_buffer[MAX_COMPONENTS_PER_RESPONSE];
static looper_bus_record_t exchange_buffer[MAX_EXCHANGES_PER_RESPONSE];

void looper_serve_list_buses(looper_response_t *response)
{
    size_t count = looper_bus_count();

    if (count > MAX_BUSES_PER_RESPONSE) {
        count = MAX_BUSES_PER_RESPONSE;
    }
    for (size_t index = 0u; index < count; index++) {
        bus_buffer[index] = *looper_bus_at(index);
    }
    response->buses = bus_buffer;
    response->bus_count = count;
    response->next_bus_sequence = looper_bus_trace_next_sequence();
}

void looper_serve_list_components(looper_response_t *response)
{
    size_t count = looper_component_count();

    if (count > MAX_COMPONENTS_PER_RESPONSE) {
        count = MAX_COMPONENTS_PER_RESPONSE;
    }
    for (size_t index = 0u; index < count; index++) {
        component_buffer[index] = *looper_component_at(index);
    }
    response->components = component_buffer;
    response->component_count = count;
}

void looper_serve_read_bus_trace(const looper_request_t *request, looper_response_t *response)
{
    response->exchanges = exchange_buffer;
    response->exchange_count = looper_bus_trace_after(
        request->after_sequence, exchange_buffer, MAX_EXCHANGES_PER_RESPONSE
    );
    response->next_bus_sequence = looper_bus_trace_next_sequence();
}

/** @brief What a client taking a peer's part is listed as. */
#define REMOTE_MODEL "remote"

void looper_serve_attach_component(
    const looper_request_t *request, looper_response_t *response
)
{
    if (looper_component_attach_remote(
            request->bus, request->bus_address, request->label, REMOTE_MODEL
        )) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message =
        "the bus is unknown, the firmware masters it, something is already at that "
        "address, or the model is full";
}

void looper_serve_detach_component(
    const looper_request_t *request, looper_response_t *response
)
{
    if (looper_component_detach(request->bus, request->bus_address)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = "nothing is attached there";
}

/*
 * What a client may put on a bus, and where.
 *
 * Checked here rather than in the bus models, because this is where a
 * client's mistake has to be reported back to it. The models are used by
 * device models, which do not send nonsense.
 */
static const char *rejection_of(const looper_request_t *request, looper_bus_kind_t kind)
{
    const uint32_t value = request->bus_address.value;

    if (kind == LOOPER_BUS_KIND_CAN) {
        if (request->payload_length > LOOPER_CAN_PAYLOAD_CAPACITY) {
            return "a Classic CAN frame carries at most eight bytes";
        }
        if (request->bus_address.extended) {
            return (value > MAXIMUM_EXTENDED_ID) ? "the identifier does not fit 29 bits" : NULL;
        }
        return (value > MAXIMUM_STANDARD_ID) ? "the identifier does not fit 11 bits" : NULL;
    }
    if (kind == LOOPER_BUS_KIND_UART) {
        return (value > MAXIMUM_PORT) ? "that is not a port number" : NULL;
    }
    if (kind == LOOPER_BUS_KIND_UNSPECIFIED) {
        return "no bus of that number is modelled";
    }
    (void)MAXIMUM_I2C_ADDRESS;
    return "the firmware is the master on that bus, so nothing can be sent to it";
}

/** @brief Hand a CAN message to the bus as a frame. */
static bool send_can(const looper_request_t *request)
{
    looper_can_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    frame.id = request->bus_address.value;
    frame.extended = request->bus_address.extended;
    frame.length = request->payload_length;
    memcpy(frame.payload, request->payload, request->payload_length);
    return looper_can_inject(request->bus, &frame);
}

void looper_serve_send_bus_message(
    const looper_request_t *request, looper_response_t *response
)
{
    const looper_bus_kind_t kind = looper_bus_kind_of(request->bus);
    const char *rejection = rejection_of(request, kind);
    bool sent;

    if (rejection != NULL) {
        response->status = LOOPER_STATUS_INVALID_ARGUMENT;
        response->message = rejection;
        return;
    }
    if (kind == LOOPER_BUS_KIND_CAN) {
        sent = send_can(request);
    } else {
        sent = looper_uart_deliver(
            request->bus,
            (uint8_t)request->bus_address.value,
            request->payload,
            request->payload_length
        );
    }
    if (sent) {
        response->next_bus_sequence = looper_bus_trace_next_sequence();
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = "the bus is holding as much as it can";
}
