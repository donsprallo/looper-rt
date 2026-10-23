#include "looper_clock.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define MICROSECONDS_PER_MILLISECOND 1000u
#define MICROSECONDS_PER_SECOND 1000000u
#define NANOSECONDS_PER_MICROSECOND 1000u

static looper_time_mode_t clock_mode = LOOPER_TIME_MODE_VIRTUAL;
static uint64_t virtual_microseconds;
static uint64_t real_origin_microseconds;

static uint64_t host_microseconds(void)
{
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
        return 0u;
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart * MICROSECONDS_PER_SECOND
           / (uint64_t)frequency.QuadPart;
#else
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0u;
    }
    return (uint64_t)now.tv_sec * MICROSECONDS_PER_SECOND
           + (uint64_t)now.tv_nsec / NANOSECONDS_PER_MICROSECOND;
#endif
}

static void host_sleep(uint64_t microseconds)
{
#if defined(_WIN32)
    DWORD milliseconds = (DWORD)(microseconds / MICROSECONDS_PER_MILLISECOND);

    Sleep(milliseconds);
#else
    struct timespec request;

    request.tv_sec = (time_t)(microseconds / MICROSECONDS_PER_SECOND);
    request.tv_nsec =
        (long)((microseconds % MICROSECONDS_PER_SECOND) * NANOSECONDS_PER_MICROSECOND);
    nanosleep(&request, NULL);
#endif
}

void looper_clock_init(looper_time_mode_t mode)
{
    clock_mode = (mode == LOOPER_TIME_MODE_REAL) ? LOOPER_TIME_MODE_REAL
                                                 : LOOPER_TIME_MODE_VIRTUAL;
    virtual_microseconds = 0u;
    real_origin_microseconds = host_microseconds();
}

looper_time_mode_t looper_clock_mode(void)
{
    return clock_mode;
}

uint64_t looper_clock_microseconds(void)
{
    if (clock_mode == LOOPER_TIME_MODE_REAL) {
        return host_microseconds() - real_origin_microseconds;
    }
    return virtual_microseconds;
}

uint32_t looper_clock_milliseconds(void)
{
    return (uint32_t)(looper_clock_microseconds() / MICROSECONDS_PER_MILLISECOND);
}

bool looper_clock_advance(uint64_t microseconds)
{
    if (clock_mode == LOOPER_TIME_MODE_REAL) {
        return false;
    }
    virtual_microseconds += microseconds;
    return true;
}

void looper_clock_wait(uint64_t microseconds)
{
    if (clock_mode == LOOPER_TIME_MODE_REAL) {
        host_sleep(microseconds);
        return;
    }
    virtual_microseconds += microseconds;
}
