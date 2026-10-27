/**
 * @file
 * @brief Control channel to the process that started the simulation.
 *
 * The simulation has exactly one client: the tool that started it. That
 * tool serves everybody else -- windows, tests -- so the runtime neither
 * listens on a port nor keeps a table of clients. It speaks the control
 * protocol over the standard input and output it was given.
 *
 * Opening the channel takes standard output away from the firmware. What
 * the firmware prints from then on goes to the error stream, so a stray
 * printf can never land in the middle of a frame.
 *
 * The channel is deliberately not threaded. The firmware calls
 * @ref looper_channel_poll from its main loop, which serves at most one
 * pending request. Nothing runs concurrently with the firmware, so a
 * virtual clock stays deterministic and a debugger sees a single call
 * stack.
 *
 * The other end going away closes the channel. A simulation whose tool
 * is gone has nobody left to answer, so the loop is expected to end.
 */
#ifndef LOOPER_CHANNEL_H
#define LOOPER_CHANNEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Take standard input and output over as the control channel.
 *
 * Both are switched to binary, standard output is kept for the channel
 * alone, and what the firmware prints is pointed at the error stream.
 *
 * @param firmware Name reported to the client, or NULL.
 * @retval true The channel is open.
 * @retval false Standard input or output could not be taken over.
 */
bool looper_channel_open(const char *firmware);

/**
 * @brief Serve the next request, if one has completely arrived.
 *
 * The call never blocks on reading: with nothing waiting it returns
 * immediately, and a request that has only partly arrived is kept and
 * completed on a later pass. The answer is written before it returns.
 *
 * Also the moment every watched firmware variable is read, which is what
 * gives a watch the resolution of the firmware rather than of a client
 * polling it. That happens whether or not the channel is open.
 *
 * @retval true A request was served.
 * @retval false Nothing was pending, or the channel is closed.
 */
bool looper_channel_poll(void);

/**
 * @brief Report whether the channel is open.
 *
 * Turns false once the other end has gone away or sent something that
 * is not a frame, and stays false.
 */
bool looper_channel_is_open(void);

/** @brief Close the channel. Closing a closed channel does nothing. */
void looper_channel_close(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_CHANNEL_H */
