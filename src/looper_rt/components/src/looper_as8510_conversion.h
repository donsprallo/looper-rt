/**
 * @file
 * @brief What the AS8510 would report for the voltages at its inputs.
 *
 * Private to the component. Separate from the register file because the two
 * answer different questions: one is how the firmware reaches the device,
 * the other is what the device does with what it measures.
 */
#ifndef LOOPER_AS8510_CONVERSION_H
#define LOOPER_AS8510_CONVERSION_H

#include "looper_as8510.h"

/** @brief The interval between conversions, in microseconds, or zero. */
uint64_t looper_as8510_interval_us(const looper_as8510_t *self);

/** @brief Take one sample of both inputs and mark both as ready. */
void looper_as8510_convert(looper_as8510_t *self);

#endif /* LOOPER_AS8510_CONVERSION_H */
