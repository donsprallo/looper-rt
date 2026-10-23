/**
 * @file
 * @brief Where the whole simulation spent its time, not only its edge.
 *
 * `looper_timing.h` measures the adapter boundary, because that is where
 * the generated code already stands. This measures everything the
 * project asked the compiler to instrument: every function of the
 * firmware and of the device model, whether or not it has anything to do
 * with the adapter.
 *
 * The compiler does the weaving. Built with `-finstrument-functions`, it
 * calls `__cyg_profile_func_enter` on entry and `__cyg_profile_func_exit`
 * on return, handing over the address of the function. Nothing here is
 * ever called by hand.
 *
 * Only an address is kept. Turning it into a name is the client's work,
 * which it does from the debug information the executable already
 * carries -- and which also tells it which source file, and so which
 * part of the simulation, the function came from. Keeping a name here
 * would mean a string table in the hot path for something the other side
 * can answer at its leisure.
 *
 * Two durations are kept per function. The total is everything between
 * entry and return. The self time is the total less what the function
 * called, which is what makes the numbers add up: totals overlap, self
 * times do not.
 *
 * The same hooks see the stack. Where a function's frame begins is the
 * address of a local in the entry hook, and the deepest such address
 * over a window is the high water mark -- with the calls that were
 * open at the time, which is the part that carries over to a target.
 *
 * ## What it costs
 *
 * About forty nanoseconds per call, and instrumentation also stops the
 * compiler inlining what it instruments. A profiled build is therefore a
 * different build, not a flag on the usual one, and the measurement is
 * of that build rather than of the plain one.
 *
 * The hooks are always compiled. Whether they are ever called is the
 * firmware build's decision, not the runtime's, so there is no switch
 * here to keep in step with one over there.
 */
#ifndef LOOPER_PROFILE_H
#define LOOPER_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LOOPER_PROFILE_CAPACITY
/**
 * @brief Number of distinct functions the table holds.
 *
 * A power of two: the index is masked rather than divided. A firmware
 * large enough to be worth profiling instruments a few hundred
 * functions, so this is room to spare.
 */
#define LOOPER_PROFILE_CAPACITY 2048u
#endif

#ifndef LOOPER_PROFILE_DEPTH
/** @brief Deepest call stack the profiler follows. */
#define LOOPER_PROFILE_DEPTH 256u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief What was measured of one function. */
typedef struct {
    /** @brief Where the function sits in the running process. */
    uintptr_t address;
    uint64_t calls;
    /** @brief Host time between entry and return, callees included. */
    uint64_t total_nanoseconds;
    /** @brief The same with the time spent in callees taken away. */
    uint64_t self_nanoseconds;
    /** @brief The slowest single call, callees included. */
    uint64_t longest_nanoseconds;
    /**
     * @brief Largest stack frame of this function that was seen.
     *
     * Measured at the function's own entry, where the compiler has
     * already set its frame up, less the same figure for its caller.
     * Host bytes: another machine lays a frame out differently. What
     * does carry over is a large local buffer, which is the same
     * number of bytes wherever it is declared.
     */
    uint64_t frame_bytes;
} looper_profile_entry_t;

/** @brief What one function cost when it was reached one way. */
typedef struct {
    /** @brief Where the function sits in the running process. */
    uintptr_t address;
    /** @brief The node it was called from; zero is the root. */
    uint32_t parent;
    uint64_t calls;
    /** @brief Host time between entry and return, callees included. */
    uint64_t total_nanoseconds;
    /** @brief The same with the time spent in callees taken away. */
    uint64_t self_nanoseconds;
} looper_profile_path_entry_t;

/** @brief Forget every measurement and start the window again. */
void looper_profile_clear(void);

/**
 * @brief Begin, or stop, following which way the calls went.
 *
 * The ranking needs only a total per function and is always kept.
 * Following the paths as well is what a flame graph is drawn from,
 * and it costs more than everything else the hooks do -- so it is
 * armed by whoever wants the picture and idle the rest of the time.
 *
 * Arming it begins a window of its own: what the calls did while
 * nobody was following them is of no use to whoever just asked.
 */
void looper_profile_watch_paths(bool wanted);

/** @brief Whether the paths are being followed. */
bool looper_profile_watches_paths(void);

/** @brief Nodes the tree of paths holds, the root included. */
size_t looper_profile_path_count(void);

/**
 * @brief One node of the tree of paths.
 * @param index Position, below `looper_profile_path_count()`.
 * @param entry Where to put it.
 * @return Whether there was one at that position.
 */
bool looper_profile_path_entry_at(size_t index, looper_profile_path_entry_t *entry);

/** @brief Paths that could not be recorded because the tree was full. */
uint64_t looper_profile_paths_not_recorded(void);

/**
 * @brief Whether any instrumented function has run since the window began.
 *
 * Which is the same question as whether this simulation was built with
 * instrumentation, answered by observation rather than by a flag.
 */
bool looper_profile_is_active(void);

/** @brief Number of functions measurements are held for. */
size_t looper_profile_count(void);

/**
 * @brief Read one function's measurements by position.
 *
 * The durations are held in the processor's own ticks while the window
 * runs and turned into nanoseconds here, against a scale measured over
 * that same window. Reading therefore copies rather than points.
 *
 * @param index Position, below `looper_profile_count()`.
 * @param entry Where to put the measurement.
 * @return Whether there was one at that position.
 */
bool looper_profile_at(size_t index, looper_profile_entry_t *entry);

/** @brief Stack bytes in use at the deepest moment of the window. */
size_t looper_profile_deepest_stack(void);

/** @brief Frames on the stack at that moment. */
size_t looper_profile_deepest_depth(void);

/** @brief Whether the whole of that call path was remembered. */
bool looper_profile_path_is_complete(void);

/**
 * @brief One function of the deepest call path, outermost first.
 * @return Its address, or zero when the index is past the path.
 */
uintptr_t looper_profile_path_at(size_t index);

/**
 * @brief Functions the table had no room for.
 *
 * A profile that quietly dropped the last function would put its time
 * into nobody's column, so the number of them is reported instead.
 */
uint64_t looper_profile_lost(void);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_PROFILE_H */
