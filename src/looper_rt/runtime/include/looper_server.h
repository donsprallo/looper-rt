/**
 * @file
 * @brief Control channel the firmware serves from its own loop.
 *
 * The server is deliberately not threaded. The firmware calls
 * @ref looper_server_poll from its main loop, which services at most one
 * pending request per attached client. Nothing runs concurrently with the
 * firmware, so a virtual clock stays deterministic and a debugger sees a
 * single call stack.
 *
 * Several clients may be attached at once -- two windows, or a window and
 * a test -- and they are all equal. There is one simulation and they all
 * act on it, so a client that clears the trace clears it for everybody.
 * What each of them keeps to itself is where it has read up to: the
 * sequence numbers a client hands back are its own.
 */
#ifndef LOOPER_SERVER_H
#define LOOPER_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Port the simulation listens on unless another one is requested. */
#define LOOPER_DEFAULT_PORT 45771u

/**
 * @brief Start listening for a control client on the loopback interface.
 * @param port TCP port to bind.
 * @param firmware Name reported to a client, or NULL.
 * @retval true The server is listening.
 * @retval false The socket could not be opened or bound.
 */
bool looper_server_start(uint16_t port, const char *firmware);

/**
 * @brief Accept whoever is waiting and serve one request per client.
 *
 * One request each rather than all of them: a client asking faster than
 * the firmware runs would otherwise be served over and over while the
 * firmware stood still, and the others would wait behind it.
 *
 * The call never blocks: with nothing waiting it returns immediately,
 * and a request that has not completely arrived is left on its socket
 * and looked at again next pass.
 *
 * Also the moment every watched firmware variable is read, which is what
 * gives a watch the resolution of the firmware rather than of a client
 * polling it. That happens whether or not a client is attached.
 *
 * @retval true At least one request was served.
 * @retval false Nothing was pending.
 */
bool looper_server_poll(void);

/** @brief Report whether any client is currently connected. */
bool looper_server_has_client(void);

/** @brief How many clients are attached. */
size_t looper_server_client_count(void);

/** @brief Close every client connection and the listening socket. */
void looper_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_SERVER_H */
