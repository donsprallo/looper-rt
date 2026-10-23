/*
 * One page of the profile.
 *
 * The measurements are held in the processor's own ticks and turned into
 * nanoseconds as they are read, so a page is copied out rather than
 * pointed at. Which also makes the page size the only thing standing
 * between a thousand measured functions and a frame that does not fit.
 */
#include "looper_commands_profile.h"

#include "looper_heap.h"
#include "looper_profile.h"

/**
 * @brief Measurements carried by one reply.
 *
 * Five varints each, so a full page is a few kilobytes against a frame
 * limit of thirty two. A client reads until the position it is given
 * back stops moving.
 */
#define PROFILES_PER_RESPONSE 192u

static looper_profile_entry_t page[PROFILES_PER_RESPONSE];

/** @brief Frames of the deepest path one reply carries. */
#define PATH_PER_RESPONSE 64u

static uintptr_t path[PATH_PER_RESPONSE];
static looper_heap_totals_t totals;
static looper_heap_site_t holders[LOOPER_HEAP_SITES];

/** @brief Copy out the deepest call path, as far as it was kept. */
static size_t fill_path(void)
{
    size_t filled = 0u;

    while ((filled < PATH_PER_RESPONSE) && (looper_profile_path_at(filled) != 0u)) {
        path[filled] = looper_profile_path_at(filled);
        filled++;
    }
    return filled;
}

void looper_serve_read_profile(const looper_request_t *request, looper_response_t *response)
{
    size_t filled = 0u;

    while ((filled < PROFILES_PER_RESPONSE)
           && looper_profile_at((size_t)request->after_sequence + filled, &page[filled])) {
        filled++;
    }
    response->profiles = page;
    response->profile_count = filled;
    response->next_sequence = (uint32_t)((size_t)request->after_sequence + filled);
    response->lost_profiles = looper_profile_lost();
    response->profiling = looper_profile_is_active();
    response->stack_deepest_bytes = (uint64_t)looper_profile_deepest_stack();
    response->stack_deepest_depth = (uint32_t)looper_profile_deepest_depth();
    response->stack_path = path;
    response->stack_path_length = fill_path();
    response->stack_path_complete = looper_profile_path_is_complete();
}


/**
 * @brief Nodes of the tree one reply carries.
 *
 * Five varints each, as a profile is, so a full page is a few
 * kilobytes against a frame limit of thirty two.
 */
#define PATHS_PER_RESPONSE 192u

static looper_profile_path_entry_t path_page[PATHS_PER_RESPONSE];

void looper_serve_read_profile_paths(
    const looper_request_t *request, looper_response_t *response
)
{
    size_t filled = 0u;
    size_t position = (size_t)request->after_sequence;

    /* The root is never sent: it is not a function, and a client
     * that was handed one would have to know to throw it away. */
    position = (position > 0u) ? position : 1u;
    while ((filled < PATHS_PER_RESPONSE)
           && looper_profile_path_entry_at(position + filled, &path_page[filled])) {
        filled++;
    }
    response->profile_paths = path_page;
    response->profile_path_count = filled;
    response->next_sequence = (uint32_t)(position + filled);
    response->lost_profile_paths = looper_profile_paths_not_recorded();
    response->watching_profile_paths = looper_profile_watches_paths();
}

void looper_serve_watch_profile_paths(
    const looper_request_t *request, looper_response_t *response
)
{
    looper_profile_watch_paths(request->watching);
    response->watching_profile_paths = looper_profile_watches_paths();
}

void looper_serve_read_heap(looper_response_t *response)
{
    looper_heap_site_t site;
    size_t kept = 0u;

    looper_heap_totals(&totals);
    for (size_t index = 0u; looper_heap_site_at(index, &site); index++) {
        /* A place that has given everything back is not holding
         * anything, and a list of those is a list of nothing. */
        if ((site.blocks > 0u) && (kept < LOOPER_HEAP_SITES)) {
            holders[kept] = site;
            kept++;
        }
    }
    response->heap = &totals;
    response->heap_sites = holders;
    response->heap_site_count = kept;
}
