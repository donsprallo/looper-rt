#include "looper_watch_memory.h"

#include <math.h>
#include <string.h>

#define BITS_PER_BYTE 8u
#define BYTE_WIDTH_8 1u
#define BYTE_WIDTH_16 2u
#define BYTE_WIDTH_32 4u
#define BYTE_WIDTH_64 8u
#define MAXIMUM_BITS 64u
#define SINGLE_PRECISION_MAXIMUM 3.4028234663852886e38

/*
 * Memory is copied rather than dereferenced through a cast. A firmware
 * variable can sit at any alignment its structure put it at, and a cast to a
 * wider type would be undefined behaviour the moment it is not aligned for
 * it. Bytes are read low first, which is the only byte order the rest of
 * this protocol assumes as well.
 */
static uint64_t load_bytes(uint64_t address, uint32_t byte_width)
{
    uint64_t value = 0u;

    memcpy(&value, (const void *)(uintptr_t)address, (size_t)byte_width);
    return value;
}

static void store_bytes(uint64_t address, uint64_t value, uint32_t byte_width)
{
    memcpy((void *)(uintptr_t)address, &value, (size_t)byte_width);
}

/** @brief Whether a whole number of bytes is a width the encoding has. */
static bool byte_width_valid(looper_encoding_t encoding, uint32_t byte_width)
{
    switch (encoding) {
        case LOOPER_ENCODING_SIGNED:
        case LOOPER_ENCODING_UNSIGNED:
            return byte_width == BYTE_WIDTH_8 || byte_width == BYTE_WIDTH_16
                   || byte_width == BYTE_WIDTH_32 || byte_width == BYTE_WIDTH_64;
        case LOOPER_ENCODING_FLOAT:
            return byte_width == BYTE_WIDTH_32 || byte_width == BYTE_WIDTH_64;
        case LOOPER_ENCODING_BOOLEAN:
            return byte_width == BYTE_WIDTH_8;
        default:
            return false;
    }
}

/** @brief Whether a run of bits is one the encoding can be read out of. */
static bool bit_width_valid(const looper_watch_location_t *location)
{
    uint64_t last = (uint64_t)location->bit_offset + location->bit_width;

    if (location->encoding == LOOPER_ENCODING_FLOAT) {
        return false;
    }
    if (location->bit_width > MAXIMUM_BITS) {
        return false;
    }
    return last <= (uint64_t)location->byte_width * BITS_PER_BYTE
           && location->byte_width <= BYTE_WIDTH_64;
}

bool looper_watch_location_valid(const looper_watch_location_t *location)
{
    if (location->bit_width != 0u) {
        return bit_width_valid(location);
    }
    return byte_width_valid(location->encoding, location->byte_width);
}

/** @brief Highest value an unsigned run of the given bits holds. */
static double unsigned_maximum(uint32_t bits)
{
    return ldexp(1.0, (int)bits) - 1.0;
}

/** @brief Magnitude of the most negative value a signed run of bits holds. */
static double signed_magnitude(uint32_t bits)
{
    return ldexp(1.0, (int)bits - 1);
}

/** @brief Bits the value occupies, whether it is a bit field or not. */
static uint32_t width_in_bits(const looper_watch_location_t *location)
{
    if (location->bit_width != 0u) {
        return location->bit_width;
    }
    return location->byte_width * BITS_PER_BYTE;
}

bool looper_watch_value_fits(const looper_watch_location_t *location, double value)
{
    uint32_t bits = width_in_bits(location);

    if (!looper_watch_location_valid(location) || isnan(value)) {
        return false;
    }
    switch (location->encoding) {
        case LOOPER_ENCODING_BOOLEAN:
            return value >= 0.0 && value <= 1.0;
        case LOOPER_ENCODING_UNSIGNED:
            return value >= 0.0 && value <= unsigned_maximum(bits);
        case LOOPER_ENCODING_SIGNED:
            return value >= -signed_magnitude(bits) && value <= signed_magnitude(bits) - 1.0;
        default:
            return location->byte_width == BYTE_WIDTH_64
                   || fabs(value) <= SINGLE_PRECISION_MAXIMUM;
    }
}

/** @brief All ones in the low bits, which is the mask of a bit field. */
static uint64_t bit_mask(uint32_t bits)
{
    if (bits >= MAXIMUM_BITS) {
        return ~(uint64_t)0u;
    }
    return ((uint64_t)1u << bits) - 1u;
}

/** @brief Widen the sign bit of a value that is narrower than 64 bits. */
static int64_t sign_extend(uint64_t raw, uint32_t bits)
{
    if (bits < MAXIMUM_BITS && (raw >> (bits - 1u)) != 0u) {
        raw |= ~(uint64_t)0u << bits;
    }
    return (int64_t)raw;
}

/** @brief Read the raw bits of a value, shifted and masked into place. */
static uint64_t load_raw(const looper_watch_location_t *location)
{
    uint64_t stored = load_bytes(location->address, location->byte_width);

    if (location->bit_width == 0u) {
        return stored;
    }
    return (stored >> location->bit_offset) & bit_mask(location->bit_width);
}

/** @brief Read a floating point variable of either supported width. */
static double load_float(uint64_t address, uint32_t byte_width)
{
    float narrow = 0.0f;
    double wide = 0.0;

    if (byte_width == BYTE_WIDTH_32) {
        memcpy(&narrow, (const void *)(uintptr_t)address, sizeof(narrow));
        return (double)narrow;
    }
    memcpy(&wide, (const void *)(uintptr_t)address, sizeof(wide));
    return wide;
}

double looper_watch_load(const looper_watch_location_t *location)
{
    switch (location->encoding) {
        case LOOPER_ENCODING_SIGNED:
            return (double)sign_extend(load_raw(location), width_in_bits(location));
        case LOOPER_ENCODING_UNSIGNED:
            return (double)load_raw(location);
        case LOOPER_ENCODING_FLOAT:
            return load_float(location->address, location->byte_width);
        case LOOPER_ENCODING_BOOLEAN:
            return (load_raw(location) != 0u) ? 1.0 : 0.0;
        default:
            return 0.0;
    }
}

/** @brief Write a floating point variable of either supported width. */
static void store_float(uint64_t address, uint32_t byte_width, double value)
{
    float narrow;

    if (byte_width == BYTE_WIDTH_32) {
        narrow = (float)value;
        memcpy((void *)(uintptr_t)address, &narrow, sizeof(narrow));
        return;
    }
    memcpy((void *)(uintptr_t)address, &value, sizeof(value));
}

/** @brief Turn a value into the raw bits that represent it. */
static uint64_t raw_of(looper_encoding_t encoding, double value)
{
    if (encoding == LOOPER_ENCODING_BOOLEAN) {
        return (value != 0.0) ? 1u : 0u;
    }
    if (encoding == LOOPER_ENCODING_SIGNED) {
        return (uint64_t)(int64_t)trunc(value);
    }
    return (uint64_t)trunc(value);
}

void looper_watch_store(const looper_watch_location_t *location, double value)
{
    uint64_t raw;
    uint64_t mask;
    uint64_t stored;

    if (location->encoding == LOOPER_ENCODING_FLOAT) {
        store_float(location->address, location->byte_width, value);
        return;
    }
    raw = raw_of(location->encoding, value);
    if (location->bit_width == 0u) {
        store_bytes(location->address, raw, location->byte_width);
        return;
    }
    mask = bit_mask(location->bit_width) << location->bit_offset;
    stored = load_bytes(location->address, location->byte_width);
    stored = (stored & ~mask) | ((raw << location->bit_offset) & mask);
    store_bytes(location->address, stored, location->byte_width);
}
