#include "looper_commands.h"

#include <string.h>

#include "looper_analog.h"
#include "looper_bus.h"
#include "looper_clock.h"
#include "looper_commands_bus.h"
#include "looper_commands_profile.h"
#include "looper_commands_watch.h"
#include "looper_gpio_model.h"
#include "looper_image.h"
#include "looper_journal.h"
#include "looper_pin_trace.h"
#include "looper_heap.h"
#include "looper_profile.h"
#include "looper_timing.h"
#include "looper_trace.h"

#define MAX_PINS_PER_RESPONSE LOOPER_GPIO_CAPACITY
#define MAX_JOURNAL_PER_RESPONSE 64u
#define MAX_CHANNELS_PER_RESPONSE LOOPER_ANALOG_CAPACITY
#define MAX_SAMPLES_PER_RESPONSE 512u
#define MAX_PIN_CHANGES_PER_RESPONSE 512u

static looper_pin_t pin_buffer[MAX_PINS_PER_RESPONSE];
static looper_journal_entry_t journal_buffer[MAX_JOURNAL_PER_RESPONSE];
static looper_channel_t channel_buffer[MAX_CHANNELS_PER_RESPONSE];
static looper_sample_t sample_buffer[MAX_SAMPLES_PER_RESPONSE];
static looper_pin_change_t pin_change_buffer[MAX_PIN_CHANGES_PER_RESPONSE];

static size_t collect_pins(void)
{
    size_t count = looper_gpio_count();

    if (count > MAX_PINS_PER_RESPONSE) {
        count = MAX_PINS_PER_RESPONSE;
    }
    for (size_t index = 0u; index < count; index++) {
        pin_buffer[index] = *looper_gpio_at(index);
    }
    return count;
}

static size_t collect_journal(void)
{
    size_t total = looper_journal_count();
    size_t count = (total > MAX_JOURNAL_PER_RESPONSE) ? MAX_JOURNAL_PER_RESPONSE : total;
    size_t first = total - count;

    for (size_t index = 0u; index < count; index++) {
        journal_buffer[index] = *looper_journal_at(first + index);
    }
    return count;
}

static size_t collect_channels(void)
{
    size_t count = looper_analog_count();

    if (count > MAX_CHANNELS_PER_RESPONSE) {
        count = MAX_CHANNELS_PER_RESPONSE;
    }
    for (size_t index = 0u; index < count; index++) {
        channel_buffer[index] = *looper_analog_at(index);
    }
    return count;
}

static void serve_set_signal(const looper_request_t *request, looper_response_t *response)
{
    if (looper_analog_set_signal(request->channel, &request->signal)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = "unknown channel, or the channel is an output";
}

static void serve_read_trace(const looper_request_t *request, looper_response_t *response)
{
    response->samples = sample_buffer;
    response->sample_count =
        looper_trace_after(request->after_sequence, sample_buffer, MAX_SAMPLES_PER_RESPONSE);
    response->next_sequence = looper_trace_next_sequence();
}

static void serve_read_pin_trace(const looper_request_t *request, looper_response_t *response)
{
    response->pin_changes = pin_change_buffer;
    response->pin_change_count = looper_pin_trace_after(
        request->after_sequence, pin_change_buffer, MAX_PIN_CHANGES_PER_RESPONSE
    );
    response->next_sequence = looper_pin_trace_next_sequence();
}

static void serve_drive_pin(const looper_request_t *request, looper_response_t *response)
{
    if (looper_gpio_drive_input(request->pin, request->level)) {
        return;
    }
    response->status = LOOPER_STATUS_INVALID_ARGUMENT;
    response->message = "unknown pin, or the pin is an output";
}

static void serve_advance_time(const looper_request_t *request, looper_response_t *response)
{
    if (looper_clock_advance(request->microseconds)) {
        response->microseconds = looper_clock_microseconds();
        return;
    }
    response->status = LOOPER_STATUS_UNSUPPORTED;
    response->message = "the clock runs in real time and cannot be advanced";
}

void looper_commands_serve(
    const looper_request_t *request, looper_response_t *response, const char *firmware
)
{
    memset(response, 0, sizeof(*response));
    response->id = request->id;
    response->status = LOOPER_STATUS_OK;
    response->microseconds = looper_clock_microseconds();
    response->time_mode = looper_clock_mode();

    switch (request->command) {
        case LOOPER_COMMAND_HELLO:
            response->protocol_version = LOOPER_PROTOCOL_VERSION;
            response->firmware = firmware;
            response->image = looper_image();
            break;
        case LOOPER_COMMAND_READ_TIME:
            break;
        case LOOPER_COMMAND_LIST_PINS:
            response->pins = pin_buffer;
            response->pin_count = collect_pins();
            break;
        case LOOPER_COMMAND_DRIVE_PIN:
            serve_drive_pin(request, response);
            break;
        case LOOPER_COMMAND_ADVANCE_TIME:
            serve_advance_time(request, response);
            break;
        case LOOPER_COMMAND_READ_JOURNAL:
            response->journal = journal_buffer;
            response->journal_count = collect_journal();
            break;
        case LOOPER_COMMAND_CLEAR_JOURNAL:
            looper_journal_clear();
            break;
        case LOOPER_COMMAND_LIST_CHANNELS:
            response->channels = channel_buffer;
            response->channel_count = collect_channels();
            response->next_sequence = looper_trace_next_sequence();
            break;
        case LOOPER_COMMAND_SET_SIGNAL:
            serve_set_signal(request, response);
            break;
        case LOOPER_COMMAND_READ_PIN_TRACE:
            serve_read_pin_trace(request, response);
            break;
        case LOOPER_COMMAND_CLEAR_PIN_TRACE:
            looper_pin_trace_clear();
            break;
        case LOOPER_COMMAND_READ_TRACE:
            serve_read_trace(request, response);
            break;
        case LOOPER_COMMAND_CLEAR_TRACE:
            looper_trace_clear();
            break;
        case LOOPER_COMMAND_LIST_WATCHES:
            looper_serve_list_watches(response);
            break;
        case LOOPER_COMMAND_WATCH_VARIABLE:
            looper_serve_watch_variable(request, response);
            break;
        case LOOPER_COMMAND_REMOVE_WATCH:
            looper_serve_remove_watch(request, response);
            break;
        case LOOPER_COMMAND_WRITE_VARIABLE:
            looper_serve_write_variable(request, response);
            break;
        case LOOPER_COMMAND_LIST_BUSES:
            looper_serve_list_buses(response);
            break;
        case LOOPER_COMMAND_LIST_COMPONENTS:
            looper_serve_list_components(response);
            break;
        case LOOPER_COMMAND_READ_BUS_TRACE:
            looper_serve_read_bus_trace(request, response);
            break;
        case LOOPER_COMMAND_CLEAR_BUS_TRACE:
            looper_bus_trace_clear();
            break;
        case LOOPER_COMMAND_READ_TIMINGS:
            response->timings = looper_timing_at(0u);
            response->timing_count = looper_timing_count();
            break;
        case LOOPER_COMMAND_CLEAR_TIMINGS:
            looper_timing_clear();
            break;
        case LOOPER_COMMAND_READ_PROFILE:
            looper_serve_read_profile(request, response);
            break;
        case LOOPER_COMMAND_READ_PROFILE_PATHS:
            looper_serve_read_profile_paths(request, response);
            break;
        case LOOPER_COMMAND_WATCH_PROFILE_PATHS:
            looper_serve_watch_profile_paths(request, response);
            break;
        case LOOPER_COMMAND_CLEAR_PROFILE:
            looper_profile_clear();
            break;
        case LOOPER_COMMAND_READ_HEAP:
            looper_serve_read_heap(response);
            break;
        case LOOPER_COMMAND_CLEAR_HEAP:
            looper_heap_clear();
            break;
        case LOOPER_COMMAND_ATTACH_COMPONENT:
            looper_serve_attach_component(request, response);
            break;
        case LOOPER_COMMAND_DETACH_COMPONENT:
            looper_serve_detach_component(request, response);
            break;
        case LOOPER_COMMAND_SEND_BUS_MESSAGE:
            looper_serve_send_bus_message(request, response);
            break;
        default:
            response->status = LOOPER_STATUS_INVALID_ARGUMENT;
            response->message = "unknown command";
            break;
    }
}
