/*
 * Standard input and output as the control channel, on each platform.
 *
 * Reading must never wait, and an anonymous pipe has no non blocking mode
 * that works the same everywhere. Windows is asked how many bytes are
 * waiting and only that many are read; POSIX is polled with no timeout.
 *
 * Writing does wait. A response is written whole before the firmware
 * goes on, and the tool on the other end reads it as soon as it has sent
 * the request, so the wait is as long as the copy.
 */
#include "looper_channel_pipe.h"

#include <stdio.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#endif

#define STANDARD_INPUT 0
#define STANDARD_OUTPUT 1
#define STANDARD_ERROR 2
#define NO_DESCRIPTOR (-1)

/** @brief Where the channel writes, once it has been taken from fd 1. */
static int channel_output = NO_DESCRIPTOR;

#if defined(_WIN32)

/** @brief Largest single read or write, as the API counts it. */
#define LARGEST_TRANSFER 0x7FFFFFFFu

static DWORD transfer_size(size_t length)
{
    return (DWORD)(length < LARGEST_TRANSFER ? length : LARGEST_TRANSFER);
}

bool looper_pipe_take_stdio(void)
{
    fflush(stdout);
    if (_setmode(STANDARD_INPUT, _O_BINARY) == NO_DESCRIPTOR) {
        return false;
    }
    channel_output = _dup(STANDARD_OUTPUT);
    if (channel_output == NO_DESCRIPTOR || _setmode(channel_output, _O_BINARY) == NO_DESCRIPTOR) {
        return false;
    }
    return _dup2(STANDARD_ERROR, STANDARD_OUTPUT) == 0;
}

long looper_pipe_read_available(void *destination, size_t capacity)
{
    const HANDLE input = (HANDLE)_get_osfhandle(STANDARD_INPUT);
    DWORD available = 0u;
    DWORD taken = 0u;

    if (!PeekNamedPipe(input, NULL, 0u, NULL, &available, NULL)) {
        return LOOPER_PIPE_GONE;
    }
    if (available == 0u) {
        return 0L;
    }
    if (!ReadFile(input, destination, transfer_size(capacity < available ? capacity : available),
                  &taken, NULL)) {
        return LOOPER_PIPE_GONE;
    }
    return (long)taken;
}

bool looper_pipe_write(const void *data, size_t length)
{
    const HANDLE output = (HANDLE)_get_osfhandle(channel_output);
    const char *bytes = (const char *)data;
    size_t sent = 0u;

    while (sent < length) {
        DWORD written = 0u;

        if (!WriteFile(output, &bytes[sent], transfer_size(length - sent), &written, NULL)) {
            return false;
        }
        sent += written;
    }
    return true;
}

void looper_pipe_release(void)
{
    if (channel_output != NO_DESCRIPTOR) {
        _close(channel_output);
        channel_output = NO_DESCRIPTOR;
    }
}

#else

bool looper_pipe_take_stdio(void)
{
    fflush(stdout);
    /* A tool that went away mid response is noticed by the failed write. */
    signal(SIGPIPE, SIG_IGN);
    channel_output = dup(STANDARD_OUTPUT);
    if (channel_output == NO_DESCRIPTOR) {
        return false;
    }
    return dup2(STANDARD_ERROR, STANDARD_OUTPUT) == STANDARD_OUTPUT;
}

long looper_pipe_read_available(void *destination, size_t capacity)
{
    struct pollfd input = {STANDARD_INPUT, POLLIN, 0};
    const int ready = poll(&input, 1u, 0);
    ssize_t read_bytes;

    if (ready < 0) {
        return (errno == EINTR) ? 0L : LOOPER_PIPE_GONE;
    }
    if (ready == 0) {
        return 0L;
    }
    read_bytes = read(STANDARD_INPUT, destination, capacity);
    if (read_bytes == 0) {
        return LOOPER_PIPE_GONE;
    }
    if (read_bytes < 0) {
        return (errno == EINTR || errno == EAGAIN) ? 0L : LOOPER_PIPE_GONE;
    }
    return (long)read_bytes;
}

bool looper_pipe_write(const void *data, size_t length)
{
    const char *bytes = (const char *)data;
    size_t sent = 0u;

    while (sent < length) {
        const ssize_t written = write(channel_output, &bytes[sent], length - sent);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0) {
            return false;
        }
        sent += (size_t)written;
    }
    return true;
}

void looper_pipe_release(void)
{
    if (channel_output != NO_DESCRIPTOR) {
        close(channel_output);
        channel_output = NO_DESCRIPTOR;
    }
}

#endif
