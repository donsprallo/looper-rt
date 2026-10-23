/**
 * @file
 * @brief The entry point every simulation shares.
 *
 * A simulation is the firmware compiled for the host, with four things
 * around it: a clock, a model of what the firmware talks to, a control
 * channel, and a loop that drives all three. Only the model is the project's
 * own. The rest was being written out by hand in every project, identically,
 * and this is that part.
 *
 * What a project supplies is an application: what to set up, how to start
 * the firmware, and what one iteration of it is. @ref looper_main does the
 * rest, and a project that wants the loop for itself simply does not link
 * this part of the runtime.
 *
 * This is the one place in the runtime that writes to a stream. An entry
 * point has to say which port it is listening on and why it stopped, so it
 * lives apart from the model and the protocol, which say nothing.
 */
#ifndef LOOPER_APP_H
#define LOOPER_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Simulated time one idle pass of the loop costs.
 *
 * In real time mode it keeps the process from spinning a core. In virtual
 * mode it is what makes the clock advance at all, since a polling loop never
 * blocks and so never yields to a client that would advance it.
 */
#define LOOPER_DEFAULT_IDLE_MICROSECONDS 250u

/**
 * @brief What one firmware looks like to the loop that runs it.
 *
 * Attributes are function pointers rather than a fixed sequence of calls
 * because a firmware decides for itself what starting up means: one project
 * calls a single init, another brings up a board first and refuses to run if
 * that fails.
 */
typedef struct {
    /** @brief Name the simulation reports to a client. */
    const char *name;
    /**
     * @brief Register pins and channels and install the device model.
     *
     * Called once, before the control channel opens, so that a client which
     * connects immediately already sees everything. May be NULL.
     */
    void (*prepare)(void);
    /**
     * @brief Bring the firmware up.
     *
     * Returning false stops the loop from running the firmware, but not the
     * process: the control channel stays open so that the journal can be
     * read. A firmware that refuses to start means it, and running it on
     * half initialised state crashes on pointers that were never set.
     *
     * May be NULL for a firmware that cannot fail to start.
     */
    bool (*start)(void);
    /** @brief Run one iteration of the firmware loop. Required. */
    void (*step)(void);
    /**
     * @brief Simulated time one idle pass costs, or zero for the default.
     */
    uint32_t idle_microseconds;
} looper_application_t;

/**
 * @brief Run a simulation until it is told to stop.
 *
 * Understands `--virtual`, `--port N` and `--steps N`. Without `--steps` the
 * loop runs until the process is stopped.
 *
 * @param argc Argument count as main received it.
 * @param argv Arguments as main received it.
 * @param application What to run and how to set it up.
 * @return An exit status: zero when the loop finished, non zero when the
 *         control channel could not be opened.
 */
int looper_main(int argc, char **argv, const looper_application_t *application);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_APP_H */
