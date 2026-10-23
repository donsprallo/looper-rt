/**
 * @file
 * @brief Where the running process put the image it was built as.
 *
 * A client that wants to watch a firmware variable reads its address out of
 * the debug information, and that address is the one the linker chose.
 * Windows and Linux both relocate an image when they load it, so the address
 * in the running process is somewhere else.
 *
 * What the process reports is where it is now. Where it was linked is not
 * something it can answer: a relocating loader overwrites the header field
 * that held it, so a process asking itself gets its own runtime base back.
 * The executable file still has the right answer, and a client reads that
 * file anyway to get at the debug information.
 *
 * The bounds are reported for a second reason. An address that falls outside
 * the image did not come from this build, and reading it would at best
 * return nonsense and at worst take the firmware down. Knowing how big the
 * image is turns that into a refusal.
 */
#ifndef LOOPER_IMAGE_H
#define LOOPER_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Describe the running image.
 *
 * Resolved once on the first call and remembered. On a platform the runtime
 * has no answer for, every field is zero, which reports honestly that the
 * image could not be located rather than guessing at it.
 *
 * @return The image. Never NULL.
 */
const looper_image_t *looper_image(void);

/**
 * @brief Whether a range of bytes lies inside the running image.
 * @param address First byte, as an address in the running process.
 * @param length Number of bytes.
 * @retval true The whole range is inside the image.
 * @retval false It is not, or the image could not be located.
 */
bool looper_image_contains(uint64_t address, uint64_t length);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_IMAGE_H */
