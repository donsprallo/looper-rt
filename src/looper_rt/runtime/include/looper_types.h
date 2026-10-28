/**
 * @file
 * @brief Value types shared by the runtime and the control protocol.
 *
 * Every enumerator matches the numeric value of its counterpart in
 * `proto/looper_control.proto`, so the protocol encoder can cast directly.
 */
#ifndef LOOPER_TYPES_H
#define LOOPER_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief How simulated time advances. */
typedef enum {
    LOOPER_TIME_MODE_UNSPECIFIED = 0,
    LOOPER_TIME_MODE_VIRTUAL = 1,
    LOOPER_TIME_MODE_REAL = 2
} looper_time_mode_t;

/** @brief Whether the firmware drives a pin or reads it. */
typedef enum {
    LOOPER_PIN_DIRECTION_UNSPECIFIED = 0,
    LOOPER_PIN_DIRECTION_INPUT = 1,
    LOOPER_PIN_DIRECTION_OUTPUT = 2
} looper_pin_direction_t;

/** @brief Electrical level present at a pin. */
typedef enum {
    LOOPER_SIGNAL_LEVEL_UNSPECIFIED = 0,
    LOOPER_SIGNAL_LEVEL_LOW = 1,
    LOOPER_SIGNAL_LEVEL_HIGH = 2
} looper_signal_level_t;

/** @brief Which electrical level the firmware treats as active. */
typedef enum {
    LOOPER_ACTIVE_POLARITY_UNSPECIFIED = 0,
    LOOPER_ACTIVE_POLARITY_HIGH = 1,
    LOOPER_ACTIVE_POLARITY_LOW = 2
} looper_active_polarity_t;

/** @brief Outcome of a control request. */
typedef enum {
    LOOPER_STATUS_UNSPECIFIED = 0,
    LOOPER_STATUS_OK = 1,
    LOOPER_STATUS_INVALID_ARGUMENT = 2,
    LOOPER_STATUS_UNSUPPORTED = 3
} looper_status_t;

/** @brief What a client asks the simulation to do. */
typedef enum {
    LOOPER_COMMAND_UNSPECIFIED = 0,
    LOOPER_COMMAND_HELLO = 1,
    LOOPER_COMMAND_READ_TIME = 2,
    LOOPER_COMMAND_LIST_PINS = 3,
    LOOPER_COMMAND_DRIVE_PIN = 4,
    LOOPER_COMMAND_ADVANCE_TIME = 5,
    LOOPER_COMMAND_READ_JOURNAL = 6,
    LOOPER_COMMAND_CLEAR_JOURNAL = 7,
    LOOPER_COMMAND_LIST_CHANNELS = 8,
    LOOPER_COMMAND_SET_SIGNAL = 9,
    LOOPER_COMMAND_READ_TRACE = 10,
    LOOPER_COMMAND_CLEAR_TRACE = 11,
    LOOPER_COMMAND_LIST_WATCHES = 12,
    LOOPER_COMMAND_WATCH_VARIABLE = 13,
    LOOPER_COMMAND_REMOVE_WATCH = 14,
    LOOPER_COMMAND_WRITE_VARIABLE = 15,
    LOOPER_COMMAND_LIST_BUSES = 16,
    LOOPER_COMMAND_READ_BUS_TRACE = 17,
    LOOPER_COMMAND_CLEAR_BUS_TRACE = 18,
    LOOPER_COMMAND_SEND_BUS_MESSAGE = 19,
    LOOPER_COMMAND_LIST_COMPONENTS = 20,
    LOOPER_COMMAND_ATTACH_COMPONENT = 21,
    LOOPER_COMMAND_DETACH_COMPONENT = 22,
    LOOPER_COMMAND_READ_TIMINGS = 23,
    LOOPER_COMMAND_CLEAR_TIMINGS = 24,
    LOOPER_COMMAND_READ_PROFILE = 25,
    LOOPER_COMMAND_CLEAR_PROFILE = 26,
    LOOPER_COMMAND_READ_HEAP = 27,
    LOOPER_COMMAND_CLEAR_HEAP = 28,
    LOOPER_COMMAND_READ_PIN_TRACE = 29,
    LOOPER_COMMAND_CLEAR_PIN_TRACE = 30,
    LOOPER_COMMAND_READ_PROFILE_PATHS = 31,
    LOOPER_COMMAND_WATCH_PROFILE_PATHS = 32,
    LOOPER_COMMAND_SET_SIGNAL_GRAPH = 33
} looper_command_t;

/** @brief What the bytes at a watched variable's address mean. */
typedef enum {
    LOOPER_ENCODING_UNSPECIFIED = 0,
    LOOPER_ENCODING_SIGNED = 1,
    LOOPER_ENCODING_UNSIGNED = 2,
    LOOPER_ENCODING_FLOAT = 3,
    LOOPER_ENCODING_BOOLEAN = 4
} looper_encoding_t;

/** @brief Maximum number of characters in a pin or channel label. */
#define LOOPER_LABEL_CAPACITY 32u

/** @brief Maximum number of characters in a unit. */
#define LOOPER_UNIT_CAPACITY 8u

/** @brief Maximum number of characters in a firmware variable path. */
#define LOOPER_VARIABLE_PATH_CAPACITY 128u

/** @brief Maximum number of characters in a file system path. */
#define LOOPER_FILE_PATH_CAPACITY 260u

/** @brief One pin as the simulation currently models it. */
typedef struct {
    uint32_t id;
    char label[LOOPER_LABEL_CAPACITY];
    looper_pin_direction_t direction;
    looper_signal_level_t level;
    looper_active_polarity_t polarity;
} looper_pin_t;

/** @brief Shape of a generated signal. */
typedef enum {
    LOOPER_WAVEFORM_CONSTANT = 0,
    LOOPER_WAVEFORM_SINE = 1,
    LOOPER_WAVEFORM_SQUARE = 2,
    LOOPER_WAVEFORM_TRIANGLE = 3,
    LOOPER_WAVEFORM_SAWTOOTH = 4,
    /**
     * Uniformly distributed between offset - amplitude and offset +
     * amplitude, a new value on every evaluation. Drawn from a generator
     * with a fixed start, so a run on the virtual clock repeats exactly.
     */
    LOOPER_WAVEFORM_NOISE = 5
} looper_waveform_t;

/** @brief Whether the firmware reads an analog channel or writes it. */
typedef enum {
    LOOPER_CHANNEL_DIRECTION_UNSPECIFIED = 0,
    LOOPER_CHANNEL_DIRECTION_INPUT = 1,
    LOOPER_CHANNEL_DIRECTION_OUTPUT = 2
} looper_channel_direction_t;

/**
 * @brief Definition of a generated signal.
 *
 * A constant waveform uses the offset alone, which is how a DC level is
 * expressed. Every other waveform swings by the amplitude around the offset.
 */
typedef struct {
    looper_waveform_t waveform;
    double amplitude;
    double offset;
    double frequency_hz;
    double phase_degrees;
} looper_signal_t;

/** @brief One analog channel as the simulation currently models it. */
typedef struct {
    uint32_t id;
    char label[LOOPER_LABEL_CAPACITY];
    char unit[LOOPER_UNIT_CAPACITY];
    looper_channel_direction_t direction;
    looper_signal_t signal;
    double value;
    /**
     * @brief Whether anything has ever written to this channel.
     *
     * A channel the firmware registers but never publishes reads zero,
     * and so does one that is publishing zero. They mean entirely
     * different things -- the first is usually a feature the firmware
     * has switched off -- so the difference is kept rather than left
     * for a reader to guess at.
     */
    bool has_value;
    /**
     * @brief Whether the signal graph drives this channel rather than its
     * own signal, which then means nothing.
     */
    bool driven_by_graph;
} looper_channel_t;

/** @brief What one node of a signal graph does. */
typedef enum {
    LOOPER_SIGNAL_NODE_UNSPECIFIED = 0,
    /** Produces its signal from the simulated clock. */
    LOOPER_SIGNAL_NODE_GENERATOR = 1,
    /** The last value of an analog channel, as the firmware left it. */
    LOOPER_SIGNAL_NODE_CHANNEL = 2,
    /** A pin's level: one for high, zero for low. */
    LOOPER_SIGNAL_NODE_PIN = 3,
    /** The last value a watched variable was read with. */
    LOOPER_SIGNAL_NODE_WATCH = 4,
    /** Left plus right. */
    LOOPER_SIGNAL_NODE_ADD = 5,
    /** Left minus right. */
    LOOPER_SIGNAL_NODE_SUBTRACT = 6,
    /** Left times right. */
    LOOPER_SIGNAL_NODE_MULTIPLY = 7,
    /** Left divided by right, or zero where right is zero. */
    LOOPER_SIGNAL_NODE_DIVIDE = 8,
    /**
     * A discrete linear system, stepped on the simulated clock: a filter,
     * a plant, anything a state space describes. Its value is its first
     * output; @ref LOOPER_SIGNAL_NODE_PICK reads the others.
     */
    LOOPER_SIGNAL_NODE_LINEAR = 9,
    /** One output of the linear system at position left, numbered by source. */
    LOOPER_SIGNAL_NODE_PICK = 10
} looper_signal_node_kind_t;

/** @brief Most states one linear system has. */
#define LOOPER_LINEAR_STATES 8u

/** @brief Most inputs, and most outputs, one linear system has. */
#define LOOPER_LINEAR_PORTS 4u

/**
 * @brief A discrete linear system in state space.
 *
 * x[k+1] = A x[k] + B u[k] and y[k] = C x[k] + D u[k], one step every
 * step_microseconds of simulated time. Each matrix is row by row, as many
 * entries as its size takes; the counts say how many arrived, so that a
 * system whose matrices do not match its size is refused rather than read
 * past.
 */
typedef struct {
    uint32_t states;
    uint32_t inputs;
    uint32_t outputs;
    uint64_t step_microseconds;
    double a[LOOPER_LINEAR_STATES * LOOPER_LINEAR_STATES];
    double b[LOOPER_LINEAR_STATES * LOOPER_LINEAR_PORTS];
    double c[LOOPER_LINEAR_PORTS * LOOPER_LINEAR_STATES];
    double d[LOOPER_LINEAR_PORTS * LOOPER_LINEAR_PORTS];
    uint32_t a_count;
    uint32_t b_count;
    uint32_t c_count;
    uint32_t d_count;
} looper_linear_system_t;

/**
 * @brief One node of a signal graph.
 *
 * Which fields count depends on the kind: a generator its signal, a source
 * its channel, pin or watch number, an operation its two inputs. Inputs
 * name nodes by their position in the graph, and only earlier positions,
 * which is what keeps a graph free of cycles.
 */
typedef struct {
    looper_signal_node_kind_t kind;
    looper_signal_t signal;
    /** A source's number; a linear system's place among the systems; a pick's output. */
    uint32_t source;
    uint32_t left;
    uint32_t right;
    /** The inputs of a linear system, by position, as many as it takes. */
    uint32_t inputs[LOOPER_LINEAR_PORTS];
    uint32_t input_count;
} looper_signal_node_t;

/** @brief Which node drives which analog input. */
typedef struct {
    uint32_t channel;
    uint32_t node;
} looper_signal_target_t;

/** @brief One recorded value of one channel. */
typedef struct {
    uint32_t channel;
    uint64_t microseconds;
    double value;
    uint32_t sequence;
} looper_sample_t;

/**
 * @brief Where the running process put the image it was built as.
 *
 * Only what the process can know about itself. The base it was linked
 * against is deliberately absent: a relocating loader overwrites the header
 * field that held it, so a process asking itself would get its own runtime
 * base back and call it the link time one.
 */
typedef struct {
    uint64_t runtime_base;
    uint64_t size;
    char executable[LOOPER_FILE_PATH_CAPACITY];
} looper_image_t;

/**
 * @brief One firmware variable the simulation samples into the trace.
 *
 * A bit field shares its bytes with the bits around it, so it carries where
 * in those bytes it starts and how many bits it occupies. A bit width of
 * zero means the value is the whole of the bytes at the address.
 */
typedef struct {
    uint32_t channel;
    char path[LOOPER_VARIABLE_PATH_CAPACITY];
    uint64_t address;
    looper_encoding_t encoding;
    uint32_t byte_width;
    uint32_t bit_offset;
    uint32_t bit_width;
    bool writable;
    double value;
} looper_watch_t;

/** @brief Which kind of bus, which is what gives an address its meaning. */
typedef enum {
    LOOPER_BUS_KIND_UNSPECIFIED = 0,
    LOOPER_BUS_KIND_SPI = 1,
    LOOPER_BUS_KIND_I2C = 2,
    LOOPER_BUS_KIND_UART = 3,
    LOOPER_BUS_KIND_CAN = 4
} looper_bus_kind_t;

/**
 * @brief Which way an exchange crossed a bus.
 *
 * Named from the firmware's point of view, because that is whose behaviour
 * a recording is read to understand.
 */
typedef enum {
    LOOPER_BUS_DIRECTION_UNSPECIFIED = 0,
    LOOPER_BUS_DIRECTION_OUTBOUND = 1,
    LOOPER_BUS_DIRECTION_INBOUND = 2
} looper_bus_direction_t;

/** @brief Longest run of bytes one recorded exchange keeps of each half. */
#define LOOPER_BUS_PAYLOAD_CAPACITY 64u

/** @brief Maximum number of characters in a component's model name. */
#define LOOPER_MODEL_CAPACITY 24u

/** @brief Values one component may publish beside what it does on its bus. */
#define LOOPER_PROPERTY_CAPACITY 6u

/**
 * @brief Where something sits on its bus.
 *
 * A chip select, a device address, a port or an identifier, depending on
 * the kind of bus. The extended flag means something only on CAN, where the
 * same value sent as 11 or as 29 bits is two different identifiers.
 */
typedef struct {
    uint32_t value;
    bool extended;
} looper_bus_address_t;

/** @brief One bus as the simulation currently models it. */
typedef struct {
    uint32_t id;
    looper_bus_kind_t kind;
    char label[LOOPER_LABEL_CAPACITY];
    uint32_t bitrate_bps;
} looper_bus_t;

/**
 * @brief One simulated device attached to a bus, as a client sees it.
 *
 * The behaviour is not here. A component answers the firmware at the speed
 * the firmware runs, so it lives in this process behind a vtable of its
 * kind; what a client is told is what it is and where it sits.
 */
typedef struct {
    uint32_t bus;
    looper_bus_address_t address;
    char label[LOOPER_LABEL_CAPACITY];
    char model[LOOPER_MODEL_CAPACITY];
    void *instance;
    const void *operations;
    /*
     * The analog channels this component publishes.
     *
     * A property is not a new kind of thing: it is a value with a name and
     * a unit that a client reads, plots and sometimes drives, which is what
     * an analog channel already is. What is recorded here is only which of
     * them belong to this component, so a client can group them.
     */
    uint32_t properties[LOOPER_PROPERTY_CAPACITY];
    uint8_t property_count;
    /*
     * Carried beside the vtable rather than inside it. Every kind has one,
     * but each kind's vtable has its own shape, and reaching a member by
     * assuming it sits at the same offset in four different structures is
     * exactly the kind of cleverness that breaks quietly.
     */
    void (*run)(void *instance, uint64_t microseconds);
} looper_component_t;

/**
 * @brief One exchange as it was recorded.
 *
 * SPI clocks both ways at once and I2C reads back, so an exchange on those
 * has two halves; a UART run of bytes and a CAN frame fill one of them. An
 * exchange nothing took is kept with `accepted` false, so a client that
 * sent something and saw no reaction can tell which of the two happened.
 */
typedef struct {
    uint32_t bus;
    looper_bus_kind_t kind;
    looper_bus_direction_t direction;
    looper_bus_address_t address;
    uint8_t payload[LOOPER_BUS_PAYLOAD_CAPACITY];
    uint8_t payload_length;
    uint8_t response[LOOPER_BUS_PAYLOAD_CAPACITY];
    uint8_t response_length;
    bool accepted;
    uint64_t microseconds;
    uint32_t sequence;
} looper_bus_record_t;

/** @brief Maximum payload of a Classic CAN frame. */
#define LOOPER_CAN_PAYLOAD_CAPACITY 8u

/** @brief One Classic CAN data frame. */
typedef struct {
    uint32_t id;
    bool extended;
    uint8_t length;
    uint8_t payload[LOOPER_CAN_PAYLOAD_CAPACITY];
} looper_can_frame_t;

/** @brief One moment a pin took a new level. */
typedef struct {
    uint32_t pin;
    looper_signal_level_t level;
    uint64_t microseconds;
    uint32_t sequence;
} looper_pin_change_t;

/** @brief One recorded adapter call. */
typedef struct {
    const char *function;
    uint64_t microseconds;
    uint32_t sequence;
} looper_journal_entry_t;

#ifdef __cplusplus
}
#endif

#endif /* LOOPER_TYPES_H */
