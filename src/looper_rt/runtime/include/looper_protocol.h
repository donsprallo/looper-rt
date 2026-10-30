/**
 * @file
 * @brief Encoding and decoding of the control protocol messages.
 *
 * Mirrors `proto/looper_control.proto`. Field numbers live in the private
 * `looper_protocol_fields.h`, so that a change to the schema has exactly one
 * counterpart in C. Encoding and decoding are separate translation units
 * because they share nothing but those numbers.
 */
#ifndef LOOPER_PROTOCOL_H
#define LOOPER_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "looper_heap.h"
#include "looper_profile.h"
#include "looper_timing.h"
#include "looper_analog.h"
#include "looper_signal_graph.h"
#include "looper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Version of the control protocol this runtime speaks.
 *
 * Answered on `COMMAND_HELLO` so that a client can refuse a simulation it
 * cannot read rather than misread it. Raise it whenever a field changes
 * meaning or a number moves; adding a field at a fresh number does not
 * need it, because an older client ignores what it does not know.
 *
 * 2: state models. Only fields were added, but an older runtime would
 * refuse a model node as a kind it does not know, with a reason nobody
 * could act on. The version lets looper-core say "rebuild" instead.
 */
#define LOOPER_PROTOCOL_VERSION 2u

/** @brief A decoded client request. */
typedef struct {
    uint32_t id;
    looper_command_t command;
    uint32_t pin;
    looper_signal_level_t level;
    uint64_t microseconds;
    uint32_t channel;
    looper_signal_t signal;
    uint32_t after_sequence;
    uint64_t address;
    looper_encoding_t encoding;
    uint32_t byte_width;
    uint32_t bit_offset;
    uint32_t bit_width;
    char path[LOOPER_VARIABLE_PATH_CAPACITY];
    bool writable;
    double value;
    uint32_t bus;
    looper_bus_address_t bus_address;
    uint8_t payload[LOOPER_BUS_PAYLOAD_CAPACITY];
    uint8_t payload_length;
    char label[LOOPER_LABEL_CAPACITY];
    bool watching;
    looper_signal_node_t signal_nodes[LOOPER_SIGNAL_GRAPH_CAPACITY];
    size_t signal_node_count;
    looper_signal_target_t signal_targets[LOOPER_ANALOG_CAPACITY];
    size_t signal_target_count;
    looper_linear_system_t signal_systems[LOOPER_SIGNAL_GRAPH_SYSTEMS];
    size_t signal_system_count;
    looper_state_model_t signal_models[LOOPER_SIGNAL_GRAPH_MODELS];
    size_t signal_model_count;
    /** COMMAND_RESET_MODEL: the node, by its position in the graph. */
    uint32_t signal_node;
    /** COMMAND_SET_SIGNAL_GRAPH: the model worker, for a graph with interpreted models. */
    char model_worker[LOOPER_MODEL_WORKER_NAME_CAPACITY];
    /** More nodes or targets arrived than there is room for. */
    bool signal_graph_overflow;
} looper_request_t;

/** @brief A response about to be encoded. */
typedef struct {
    uint32_t id;
    looper_status_t status;
    /** Set on COMMAND_HELLO only; zero on every other response. */
    uint32_t protocol_version;
    const char *message;
    uint64_t microseconds;
    looper_time_mode_t time_mode;
    const looper_pin_t *pins;
    size_t pin_count;
    const looper_journal_entry_t *journal;
    size_t journal_count;
    const char *firmware;
    const looper_channel_t *channels;
    size_t channel_count;
    const looper_sample_t *samples;
    size_t sample_count;
    uint32_t next_sequence;
    const looper_watch_t *watches;
    size_t watch_count;
    const looper_image_t *image;
    const looper_bus_t *buses;
    size_t bus_count;
    const looper_bus_record_t *exchanges;
    size_t exchange_count;
    uint32_t next_bus_sequence;
    const looper_component_t *components;
    size_t component_count;
    const looper_timing_t *timings;
    size_t timing_count;
    const looper_profile_entry_t *profiles;
    size_t profile_count;
    uint64_t lost_profiles;
    bool profiling;
    uint64_t stack_deepest_bytes;
    uint32_t stack_deepest_depth;
    const uintptr_t *stack_path;
    size_t stack_path_length;
    bool stack_path_complete;
    /** @brief What the allocator was asked for, or NULL when unasked. */
    const looper_heap_totals_t *heap;
    const looper_pin_change_t *pin_changes;
    size_t pin_change_count;
    const looper_profile_path_entry_t *profile_paths;
    size_t profile_path_count;
    uint64_t lost_profile_paths;
    bool watching_profile_paths;
    const looper_heap_site_t *heap_sites;
    size_t heap_site_count;
    /** COMMAND_READ_SIGNAL_GRAPH: every model node of the graph. */
    const looper_signal_node_status_t *signal_node_statuses;
    size_t signal_node_status_count;
} looper_response_t;

/**
 * @brief Decode one request message.
 * @param data Encoded bytes.
 * @param length Number of encoded bytes.
 * @param request Receives the decoded request. Absent fields stay zero.
 * @retval true The message was decoded.
 * @retval false The message is malformed.
 */
bool looper_protocol_decode_request(
    const uint8_t *data, size_t length, looper_request_t *request
);

/**
 * @brief Encode one response message.
 * @param response Response to encode.
 * @param buffer Destination buffer.
 * @param capacity Size of the destination buffer.
 * @param length Receives the number of bytes written.
 * @retval true The response was encoded completely.
 * @retval false The buffer was too small.
 */
bool looper_protocol_encode_response(
    const looper_response_t *response, uint8_t *buffer, size_t capacity, size_t *length
);

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_PROTOCOL_H */
