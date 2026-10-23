#include "looper_analog.h"

#include <math.h>
#include <string.h>

#include "looper_clock.h"
#include "looper_trace.h"

#define MICROSECONDS_PER_SECOND 1000000.0
#define DEGREES_PER_TURN 360.0
#define HALF_TURN 0.5
#define QUARTER_TURN 0.25
#define THREE_QUARTER_TURN 0.75
#define FULL_SWING 4.0
#define TWO 2.0
/* M_PI is POSIX rather than C11, so the constant is spelled out here. */
#define LOOPER_PI 3.14159265358979323846

static looper_channel_t channels[LOOPER_ANALOG_CAPACITY];
static size_t channel_count;

static looper_channel_t *find(uint32_t channel)
{
    for (size_t index = 0u; index < channel_count; index++) {
        if (channels[index].id == channel) {
            return &channels[index];
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

/* Position within one period, in turns, wrapped into [0, 1). */
static double phase_of(const looper_signal_t *signal, uint64_t microseconds)
{
    double seconds = (double)microseconds / MICROSECONDS_PER_SECOND;
    double turns = seconds * signal->frequency_hz + signal->phase_degrees / DEGREES_PER_TURN;

    return turns - floor(turns);
}

static double unit_wave(looper_waveform_t waveform, double phase)
{
    switch (waveform) {
        case LOOPER_WAVEFORM_SINE:
            return sin(TWO * LOOPER_PI * phase);
        case LOOPER_WAVEFORM_SQUARE:
            return (phase < HALF_TURN) ? 1.0 : -1.0;
        case LOOPER_WAVEFORM_TRIANGLE:
            if (phase < QUARTER_TURN) {
                return FULL_SWING * phase;
            }
            if (phase < THREE_QUARTER_TURN) {
                return TWO - FULL_SWING * phase;
            }
            return FULL_SWING * phase - FULL_SWING;
        case LOOPER_WAVEFORM_SAWTOOTH:
            return TWO * phase - 1.0;
        case LOOPER_WAVEFORM_CONSTANT:
        default:
            return 0.0;
    }
}

double looper_analog_evaluate(const looper_signal_t *signal, uint64_t microseconds)
{
    if (signal == NULL) {
        return 0.0;
    }
    if (signal->waveform == LOOPER_WAVEFORM_CONSTANT || signal->frequency_hz <= 0.0) {
        return signal->offset;
    }
    double wave = unit_wave(signal->waveform, phase_of(signal, microseconds));

    return signal->offset + signal->amplitude * wave;
}

void looper_analog_clear(void)
{
    channel_count = 0u;
    memset(channels, 0, sizeof(channels));
}

bool looper_analog_register(
    uint32_t channel,
    const char *label,
    const char *unit,
    looper_channel_direction_t direction
)
{
    looper_channel_t *entry = find(channel);

    if (entry == NULL) {
        if (channel_count >= LOOPER_ANALOG_CAPACITY) {
            return false;
        }
        entry = &channels[channel_count];
        channel_count++;
        entry->id = channel;
        entry->signal.waveform = LOOPER_WAVEFORM_CONSTANT;
    }
    copy_text(entry->label, LOOPER_LABEL_CAPACITY, label);
    copy_text(entry->unit, LOOPER_UNIT_CAPACITY, unit);
    entry->direction = direction;
    return true;
}

bool looper_analog_set_signal(uint32_t channel, const looper_signal_t *signal)
{
    looper_channel_t *entry = find(channel);

    if (entry == NULL || signal == NULL) {
        return false;
    }
    if (entry->direction != LOOPER_CHANNEL_DIRECTION_INPUT) {
        return false;
    }
    entry->signal = *signal;
    return true;
}

double looper_analog_read(uint32_t channel)
{
    looper_channel_t *entry = find(channel);

    if (entry == NULL) {
        return 0.0;
    }
    if (entry->direction == LOOPER_CHANNEL_DIRECTION_INPUT) {
        entry->value = looper_analog_evaluate(&entry->signal, looper_clock_microseconds());
        entry->has_value = true;
    }
    looper_trace_record(channel, entry->value);
    return entry->value;
}

bool looper_analog_write(uint32_t channel, double value)
{
    looper_channel_t *entry = find(channel);

    if (entry == NULL) {
        return false;
    }
    entry->value = value;
    entry->has_value = true;
    looper_trace_record(channel, value);
    return true;
}

size_t looper_analog_count(void)
{
    return channel_count;
}

const looper_channel_t *looper_analog_at(size_t index)
{
    if (index >= channel_count) {
        return NULL;
    }
    return &channels[index];
}
