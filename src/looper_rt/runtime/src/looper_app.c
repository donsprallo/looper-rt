#include "looper_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "looper_analog.h"
#include "looper_bridge.h"
#include "looper_clock.h"
#include "looper_binding.h"
#include "looper_bus.h"
#include "looper_can.h"
#include "looper_channel.h"
#include "looper_gpio_model.h"
#include "looper_heap.h"
#include "looper_profile.h"
#include "looper_timing.h"
#include "looper_uart.h"
#include "looper_journal.h"
#include "looper_trace.h"
#include "looper_watch.h"

#define UNLIMITED_STEPS 0u
#define MICROSECONDS_PER_MILLISECOND 1000.0

/** @brief What the command line asked for. */
typedef struct {
    looper_time_mode_t mode;
    bool control;
    unsigned long steps;
} options_t;

static options_t parse_options(int argc, char **argv)
{
    options_t options = {LOOPER_TIME_MODE_REAL, false, UNLIMITED_STEPS};

    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--virtual") == 0) {
            options.mode = LOOPER_TIME_MODE_VIRTUAL;
        } else if (strcmp(argv[index], "--control") == 0) {
            options.control = true;
        } else if (strcmp(argv[index], "--steps") == 0 && index + 1 < argc) {
            index++;
            options.steps = strtoul(argv[index], NULL, 10);
        }
    }
    return options;
}

/** @brief Number of entries from the given one on that repeat the same call. */
static size_t repeat_count_at(size_t index)
{
    const looper_journal_entry_t *first = looper_journal_at(index);
    size_t count = 1u;

    while (index + count < looper_journal_count() &&
           looper_journal_at(index + count)->function == first->function) {
        count++;
    }
    return count;
}

/*
 * Write the journal to the error stream.
 *
 * A client can read the journal over the control channel, which is the
 * richer way to look at it. This is for the case where start up failed: the
 * answer to "how far did it get" is then wanted immediately and without
 * attaching anything.
 *
 * Repeated calls are folded into a count. A register write and verify
 * sequence is dozens of identical lines, and printing them all buries what
 * came before.
 */
static void report_journal(const char *name)
{
    size_t index = 0u;

    fprintf(stderr, "%s: %zu adapter calls recorded\n", name, looper_journal_count());
    while (index < looper_journal_count()) {
        const looper_journal_entry_t *entry = looper_journal_at(index);
        size_t repeats = repeat_count_at(index);

        fprintf(
            stderr, "  %10.3f ms  %-40s x%zu\n",
            (double)entry->microseconds / MICROSECONDS_PER_MILLISECOND, entry->function, repeats
        );
        index += repeats;
    }
}

/** @brief Set the model up before anything can look at it. */
static void prepare(const looper_application_t *application, looper_time_mode_t mode)
{
    looper_clock_init(mode);
    looper_journal_clear();
    looper_timing_clear();
    looper_profile_clear();
    looper_heap_clear();
    looper_trace_clear();
    looper_watch_clear();
    looper_gpio_clear();
    looper_analog_clear();
    looper_binding_clear();
    looper_bridge_clear();
    looper_bus_clear();
    looper_can_clear();
    looper_uart_clear();
    if (application->prepare != NULL) {
        application->prepare();
    }
}

/**
 * @brief Bring the firmware up, reporting how far it got if it refuses.
 * @return Whether the loop may run the firmware.
 */
static bool start_firmware(const looper_application_t *application)
{
    if (application->start == NULL || application->start()) {
        return true;
    }
    fprintf(stderr, "%s: start up failed, the firmware loop will not run\n", application->name);
    report_journal(application->name);
    return false;
}

/** @brief What one pass of the firmware loop is called in the measurements. */
#define LOOPER_STEP_NAME "firmware step"

/** @brief Simulated time one idle pass of this application costs. */
static uint32_t idle_microseconds(const looper_application_t *application)
{
    if (application->idle_microseconds == 0u) {
        return LOOPER_DEFAULT_IDLE_MICROSECONDS;
    }
    return application->idle_microseconds;
}

/*
 * Whether the loop goes on.
 *
 * A simulation started with a control channel ends once its tool has gone
 * away: there is nobody left to answer, and a firmware left running with
 * nobody to stop it is only in the way.
 */
static bool keeps_running(const options_t *options, unsigned long completed)
{
    if (options->control && !looper_channel_is_open()) {
        return false;
    }
    return options->steps == UNLIMITED_STEPS || completed < options->steps;
}

/** @brief Open the control channel if one was asked for. */
static bool open_channel(const options_t *options, const char *name)
{
    if (!options->control || looper_channel_open(name)) {
        return true;
    }
    fprintf(stderr, "%s: could not take over standard input and output\n", name);
    return false;
}

int looper_main(int argc, char **argv, const looper_application_t *application)
{
    options_t options;
    unsigned long completed = 0u;
    uint32_t idle;
    bool running;

    if (application == NULL || application->step == NULL) {
        fprintf(stderr, "looper: an application has to say what one iteration is\n");
        return EXIT_FAILURE;
    }
    options = parse_options(argc, argv);
    prepare(application, options.mode);

    if (!open_channel(&options, application->name)) {
        return EXIT_FAILURE;
    }

    running = start_firmware(application);
    idle = idle_microseconds(application);
    while (keeps_running(&options, completed)) {
        looper_channel_poll();
        if (running) {
            /*
             * The headline number: what one pass of the firmware costs on
             * the host. Measured the same way the adapter calls are, so the
             * two appear side by side and the parts add up to the whole.
             */
            const uint64_t looper_started = looper_timing_enter(LOOPER_STEP_NAME);

            application->step();
            looper_timing_leave(LOOPER_STEP_NAME, looper_started, true);
        }
        looper_clock_wait(idle);
        completed++;
    }

    looper_channel_close();
    return EXIT_SUCCESS;
}
