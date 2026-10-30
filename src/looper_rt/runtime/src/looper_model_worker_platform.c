/*
 * Shared memory, named semaphores and a process, on Windows and elsewhere.
 *
 * The names follow Python's multiprocessing.shared_memory, which is what
 * looper-core creates the memory with: the name as it is on Windows, with a
 * leading slash for shm_open elsewhere. The semaphores carry "-q" for the
 * request and "-a" for the answer.
 */
#include "looper_model_worker_platform.h"

#include <stdio.h>
#include <string.h>

#include "looper_model_worker.h"

/* How long one wait lasts before the worker is asked whether it lives. */
#define WAIT_ROUND_MILLISECONDS 100u

#if defined(_WIN32)
#include <windows.h>

void *looper_worker_map(const char *name, looper_worker_link_t *link)
{
    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (mapping == NULL) {
        return NULL;
    }
    link->mapping = mapping;
    link->memory = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(looper_worker_memory_t));
    return link->memory;
}

static void *open_semaphore(const char *name, const char *suffix)
{
    char full[LOOPER_WORKER_NAME_CAPACITY + 4u];
    snprintf(full, sizeof full, "%s-%s", name, suffix);
    return (void *)OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, full);
}

bool looper_worker_watch_process(uint32_t pid, looper_worker_link_t *link)
{
    link->pid = pid;
    link->process = (void *)OpenProcess(SYNCHRONIZE, FALSE, pid);
    return link->process != NULL;
}

void looper_worker_post(const looper_worker_link_t *link)
{
    ReleaseSemaphore((HANDLE)link->request, 1, NULL);
}

looper_wait_t looper_worker_wait_round(const looper_worker_link_t *link)
{
    DWORD waited = WaitForSingleObject((HANDLE)link->reply, WAIT_ROUND_MILLISECONDS);
    if (waited == WAIT_OBJECT_0) {
        return LOOPER_WAIT_ANSWERED;
    }
    return waited == WAIT_TIMEOUT ? LOOPER_WAIT_NOT_YET : LOOPER_WAIT_FAILED;
}

bool looper_worker_ended(const looper_worker_link_t *link)
{
    return WaitForSingleObject((HANDLE)link->process, 0u) == WAIT_OBJECT_0;
}

static void close_handle(void *handle)
{
    if (handle != NULL) {
        CloseHandle((HANDLE)handle);
    }
}

void looper_worker_unlink(looper_worker_link_t *link)
{
    if (link->memory != NULL) {
        UnmapViewOfFile(link->memory);
    }
    close_handle(link->mapping);
    close_handle(link->request);
    close_handle(link->reply);
    close_handle(link->process);
    memset(link, 0, sizeof(*link));
}

#else
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

void *looper_worker_map(const char *name, looper_worker_link_t *link)
{
    char full[LOOPER_WORKER_NAME_CAPACITY + 2u];
    snprintf(full, sizeof full, "/%s", name);
    int descriptor = shm_open(full, O_RDWR, 0);
    if (descriptor < 0) {
        return NULL;
    }
    void *memory = mmap(NULL, sizeof(looper_worker_memory_t), PROT_READ | PROT_WRITE, MAP_SHARED, descriptor, 0);
    close(descriptor);
    link->memory = memory == MAP_FAILED ? NULL : memory;
    return link->memory;
}

static void *open_semaphore(const char *name, const char *suffix)
{
    char full[LOOPER_WORKER_NAME_CAPACITY + 6u];
    snprintf(full, sizeof full, "/%s-%s", name, suffix);
    sem_t *semaphore = sem_open(full, 0);
    return semaphore == SEM_FAILED ? NULL : (void *)semaphore;
}

bool looper_worker_watch_process(uint32_t pid, looper_worker_link_t *link)
{
    link->pid = pid;
    return pid != 0u && kill((pid_t)pid, 0) == 0;
}

void looper_worker_post(const looper_worker_link_t *link)
{
    sem_post((sem_t *)link->request);
}

#if defined(__APPLE__)
/* macOS has no sem_timedwait: try, and sleep a little between tries. */
looper_wait_t looper_worker_wait_round(const looper_worker_link_t *link)
{
    const struct timespec pause = {0, 100000L};
    for (uint32_t tried = 0u; tried < WAIT_ROUND_MILLISECONDS * 10u; tried++) {
        if (sem_trywait((sem_t *)link->reply) == 0) {
            return LOOPER_WAIT_ANSWERED;
        }
        if (errno != EAGAIN) {
            return LOOPER_WAIT_FAILED;
        }
        nanosleep(&pause, NULL);
    }
    return LOOPER_WAIT_NOT_YET;
}
#else
looper_wait_t looper_worker_wait_round(const looper_worker_link_t *link)
{
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_nsec += (long)WAIT_ROUND_MILLISECONDS * 1000000L;
    if (until.tv_nsec >= 1000000000L) {
        until.tv_sec += 1;
        until.tv_nsec -= 1000000000L;
    }
    while (sem_timedwait((sem_t *)link->reply, &until) != 0) {
        if (errno == ETIMEDOUT) {
            return LOOPER_WAIT_NOT_YET;
        }
        if (errno != EINTR) {
            return LOOPER_WAIT_FAILED;
        }
    }
    return LOOPER_WAIT_ANSWERED;
}
#endif

bool looper_worker_ended(const looper_worker_link_t *link)
{
    return kill((pid_t)link->pid, 0) != 0 && errno == ESRCH;
}

void looper_worker_unlink(looper_worker_link_t *link)
{
    if (link->memory != NULL) {
        munmap(link->memory, sizeof(looper_worker_memory_t));
    }
    if (link->request != NULL) {
        sem_close((sem_t *)link->request);
    }
    if (link->reply != NULL) {
        sem_close((sem_t *)link->reply);
    }
    memset(link, 0, sizeof(*link));
}
#endif

bool looper_worker_open_semaphores(const char *name, looper_worker_link_t *link)
{
    link->request = open_semaphore(name, "q");
    link->reply = open_semaphore(name, "a");
    return link->request != NULL && link->reply != NULL;
}
