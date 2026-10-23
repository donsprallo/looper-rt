/*
 * What the AS8510 would report for the voltages at its inputs.
 *
 * The transfer function is the converter's own: the reference voltage over
 * the full scale, divided by the gain in front of the modulator. Which gain
 * and which rate are in force is read out of the registers the firmware
 * itself wrote, so this follows the configuration rather than assuming one,
 * and an autorange moves it the way it moves the chip.
 */
#include "looper_as8510_conversion.h"

#include <math.h>
#include <string.h>

/* Register addresses, as the data sheet lays the device out. */
#define REGISTER_DREG_I1 0x00u
#define REGISTER_DREG_V1 0x02u
#define REGISTER_STATUS 0x04u
#define REGISTER_DEC_R1_I 0x05u
#define REGISTER_PGA_CTL 0x13u
#define REGISTER_PD_CTL_2 0x15u
#define REGISTER_DEC_R1_V 0x45u

/* Bit positions within those registers, from the least significant bit. */
#define STATUS_VOLTAGE_READY_BIT 1u
#define STATUS_CURRENT_READY_BIT 2u
#define PD_CTL_2_BYPASS_VOLTAGE_BIT 2u
#define PD_CTL_2_BYPASS_CURRENT_BIT 3u
#define PGA_CTL_VOLTAGE_SHIFT 4u
#define PGA_CTL_CURRENT_SHIFT 6u
#define PGA_CTL_MASK 0x03u
#define DECIMATION_CIC_SHIFT 1u
#define DECIMATION_CIC_MASK 0x0Fu

/** @brief Counts at full scale, which is what the signed sample range is. */
#define FULL_SCALE_COUNTS 32768.0
#define SATURATION_COUNTS 0x7FFF

#define MICROSECONDS_PER_SECOND 1000000.0

/** @brief Gain the amplifier gives at each of its settings. */
static const double gain_by_index[] = {1.0, 5.0, 25.0, 40.0, 100.0};
#define GAIN_COUNT (sizeof(gain_by_index) / sizeof(gain_by_index[0]))

/** @brief Rate assumed when a board does not say what its clock gives. */
#define DEFAULT_BASE_RATE_HZ 8000.0

/*
 * Conversion rate for one decimation setting.
 *
 * The second CIC stage divides the modulator's rate by a power of two, so
 * the sixteen settings are that rate halved sixteen times over.
 */
static double rate_for(const looper_as8510_t *self, uint8_t decimation_register)
{
    const uint8_t index =
        (uint8_t)((decimation_register >> DECIMATION_CIC_SHIFT) & DECIMATION_CIC_MASK);
    const double base = (self->config.conversion_base_rate_hz > 0.0)
                            ? self->config.conversion_base_rate_hz
                            : DEFAULT_BASE_RATE_HZ;

    return base / (double)(1u << index);
}

static bool bit_of(uint8_t value, uint8_t position)
{
    return ((value >> position) & 1u) != 0u;
}

/*
 * The interval between conversions.
 *
 * The device reports when the slower of the two channels has a sample. A
 * channel whose register selects no rate does not take part.
 */
uint64_t looper_as8510_interval_us(const looper_as8510_t *self)
{
    double current_hz;
    double voltage_hz;
    double slower;

    if (self == NULL) {
        return 0u;
    }
    current_hz = rate_for(self, self->registers[REGISTER_DEC_R1_I]);
    voltage_hz = rate_for(self, self->registers[REGISTER_DEC_R1_V]);
    slower = (current_hz < voltage_hz) ? current_hz : voltage_hz;

    if (!(slower > 0.0)) {
        return 0u;
    }
    return (uint64_t)(MICROSECONDS_PER_SECOND / slower);
}

/** @brief The gain one channel is configured for. */
static double gain_of(const looper_as8510_t *self, uint8_t bypass_bit, uint8_t pga_shift)
{
    uint8_t index;

    if (bit_of(self->registers[REGISTER_PD_CTL_2], bypass_bit)) {
        return gain_by_index[0];
    }
    index = (uint8_t)(((self->registers[REGISTER_PGA_CTL] >> pga_shift) & PGA_CTL_MASK) + 1u);
    if (index >= GAIN_COUNT) {
        index = (uint8_t)(GAIN_COUNT - 1u);
    }
    return gain_by_index[index];
}

/** @brief The count the converter would report for a voltage. */
static int16_t counts_for(const looper_as8510_t *self, double volts, double gain)
{
    const double counts =
        volts * FULL_SCALE_COUNTS * gain / self->config.reference_voltage_v;

    if (!isfinite(counts)) {
        return 0;
    }
    if (counts > (double)SATURATION_COUNTS) {
        return (int16_t)SATURATION_COUNTS;
    }
    if (counts < -(double)SATURATION_COUNTS) {
        return (int16_t)(-SATURATION_COUNTS);
    }
    return (int16_t)lround(counts);
}

/** @brief Store a count the way the device presents it, high byte first. */
static void store(looper_as8510_t *self, uint8_t high_address, int16_t counts)
{
    const uint16_t raw = (uint16_t)counts;

    self->registers[high_address] = (uint8_t)(raw >> 8u);
    self->registers[high_address + 1u] = (uint8_t)(raw & 0xFFu);
}

void looper_as8510_convert(looper_as8510_t *self)
{
    store(
        self,
        REGISTER_DREG_I1,
        counts_for(
            self,
            looper_as8510_input(self, LOOPER_AS8510_INPUT_CURRENT),
            gain_of(self, PD_CTL_2_BYPASS_CURRENT_BIT, PGA_CTL_CURRENT_SHIFT)
        )
    );
    store(
        self,
        REGISTER_DREG_V1,
        counts_for(
            self,
            looper_as8510_input(self, LOOPER_AS8510_INPUT_VOLTAGE),
            gain_of(self, PD_CTL_2_BYPASS_VOLTAGE_BIT, PGA_CTL_VOLTAGE_SHIFT)
        )
    );
    self->registers[REGISTER_STATUS] =
        (uint8_t)((1u << STATUS_CURRENT_READY_BIT) | (1u << STATUS_VOLTAGE_READY_BIT));
}

