#include "looper_gpio_model.h"

#include "looper_pin_trace.h"

#include <string.h>

static looper_pin_t pins[LOOPER_GPIO_CAPACITY];
static size_t pin_count;

static looper_pin_t *find(uint32_t pin)
{
    for (size_t index = 0u; index < pin_count; index++) {
        if (pins[index].id == pin) {
            return &pins[index];
        }
    }
    return NULL;
}

static looper_signal_level_t inactive_level(looper_active_polarity_t polarity)
{
    if (polarity == LOOPER_ACTIVE_POLARITY_LOW) {
        return LOOPER_SIGNAL_LEVEL_HIGH;
    }
    return LOOPER_SIGNAL_LEVEL_LOW;
}

static void copy_label(looper_pin_t *entry, const char *label)
{
    if (label == NULL) {
        entry->label[0] = '\0';
        return;
    }
    strncpy(entry->label, label, LOOPER_LABEL_CAPACITY - 1u);
    entry->label[LOOPER_LABEL_CAPACITY - 1u] = '\0';
}

void looper_gpio_clear(void)
{
    pin_count = 0u;
    memset(pins, 0, sizeof(pins));
}

bool looper_gpio_register(
    uint32_t pin,
    const char *label,
    looper_pin_direction_t direction,
    looper_active_polarity_t polarity
)
{
    looper_pin_t *entry = find(pin);

    bool is_new = (entry == NULL);

    if (is_new) {
        if (pin_count >= LOOPER_GPIO_CAPACITY) {
            return false;
        }
        entry = &pins[pin_count];
        pin_count++;
        entry->id = pin;
    }
    copy_label(entry, label);
    entry->direction = direction;
    entry->polarity = polarity;
    if (is_new) {
        entry->level = inactive_level(polarity);
        /* The level it starts at, so that a drawing has somewhere to
         * begin. Without it a pin that never changes has no history
         * at all and cannot be told from one nobody registered. */
        looper_pin_trace_record(pin, entry->level, LOOPER_SIGNAL_LEVEL_UNSPECIFIED);
    }
    return true;
}

bool looper_gpio_write(uint32_t pin, looper_signal_level_t level)
{
    looper_pin_t *entry = find(pin);

    if (entry == NULL) {
        return false;
    }
    looper_pin_trace_record(pin, level, entry->level);
    entry->level = level;
    return true;
}

bool looper_gpio_drive_input(uint32_t pin, looper_signal_level_t level)
{
    looper_pin_t *entry = find(pin);

    if (entry == NULL || entry->direction != LOOPER_PIN_DIRECTION_INPUT) {
        return false;
    }
    looper_pin_trace_record(pin, level, entry->level);
    entry->level = level;
    return true;
}

looper_signal_level_t looper_gpio_read(uint32_t pin)
{
    const looper_pin_t *entry = find(pin);

    if (entry == NULL) {
        return LOOPER_SIGNAL_LEVEL_UNSPECIFIED;
    }
    return entry->level;
}

size_t looper_gpio_count(void)
{
    return pin_count;
}

const looper_pin_t *looper_gpio_at(size_t index)
{
    if (index >= pin_count) {
        return NULL;
    }
    return &pins[index];
}
