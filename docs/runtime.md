# The runtime

The generator produces adapter functions that forward to hooks. The runtime is
what those hooks usually talk to: a clock, a pin model, a call journal, and a
control channel. It ships inside the package as `looper/runtime/` and is
plain C11 with no dependencies beyond the sockets and pipes the platform
provides.

Generated code does **not** depend on the runtime. A test that only installs
hooks links the generated sources alone. The runtime enters the picture when a
simulation should be observable from outside the process.

## The device model

A hook can do anything. In practice most hooks do the same thing: map an
adapter call onto runtime state. That mapping is the device model, and it is
the one piece a project writes by hand.

```c
static void model_gpio_set(blinky_pin_t pin, blinky_pin_state_t state)
{
    looper_gpio_write(pin, level_for(pin, state));
}

static uint32_t model_millis(void)
{
    return looper_clock_milliseconds();
}

void device_model_install(void)
{
    looper_hooks_t *hooks = looper_hooks();

    hooks->adapter_blinky_gpio_set = model_gpio_set;
    hooks->adapter_blinky_millis = model_millis;
}
```

A test that wants different behaviour installs its own hooks over the top. The
device model is a default, not a fixture.

## The clock

```c
void looper_clock_init(looper_time_mode_t mode);
uint64_t looper_clock_microseconds(void);
uint32_t looper_clock_milliseconds(void);
bool looper_clock_advance(uint64_t microseconds);
void looper_clock_wait(uint64_t microseconds);
```

`looper_clock_wait` is the one to map a blocking `delay_ms()` onto. In real time
mode it sleeps; in virtual mode it moves the clock forward and returns at once,
which is why a firmware timeout of thirty seconds costs nothing in a test.

`looper_clock_advance` fails in real time mode rather than quietly doing
nothing, so a client that tries to steer the host clock is told it cannot.

## The pin model

The adapter addresses a pin by an opaque number; a person needs a name. The
firmware registers each pin once at start up, and that registration is what a
client sees:

```c
looper_gpio_register(
    17u, "LED_STATUS", LOOPER_PIN_DIRECTION_OUTPUT, LOOPER_ACTIVE_POLARITY_HIGH);
looper_gpio_register(
    4u, "BUTTON_MODE", LOOPER_PIN_DIRECTION_INPUT, LOOPER_ACTIVE_POLARITY_LOW);
```

Direction is what keeps a client honest. `looper_gpio_write` is for the
firmware and accepts any pin; `looper_gpio_drive_input` is for a client and
refuses outputs. A graphical client cannot pull an LED low by accident.

A freshly registered pin starts at the level its polarity calls **inactive**, so
nothing reads as asserted before the firmware or a client sets it.

### Where pin names come from

In the example the firmware passes them as literals. For a Harmony project they
already exist: `plib_gpio.h` contains

```c
#define LED_STAT_B_PIN  GPIO_PIN_RH7
#define GPIO_PIN_RH7    (119U)
```

Two macro resolutions turn `119` into `LED_STAT_B`, using the same libclang
front end looper already carries.

## Signals on analog inputs

An analog input is driven by a client with a generator, not with samples.
The runtime evaluates the generator at the moment the firmware reads the
input, against the simulated clock. The waveforms are constant, sine,
square, triangle, sawtooth and **noise**. Noise is spread evenly between
`offset - amplitude` and `offset + amplitude`, with a new value on every
read. It has an amplitude but no period. Its generator starts the same way
in every run, so a run on the virtual clock repeats exactly.

### The signal graph

`looper_signal_graph.h` builds signals out of other signals: generators,
what the firmware itself wrote to an output channel, a pin, a watched
variable, combined by add, subtract, multiply and divide. Division by zero
gives 0, so a firmware is never fed infinity.

```c
/* parameters abbreviated; see the header */
bool looper_signal_graph_set(nodes, node_count, targets, target_count, &reason);
void looper_signal_graph_clear(void);
void looper_signal_graph_release(uint32_t channel);
bool looper_signal_graph_value(uint32_t channel, uint64_t microseconds, double *value);
```

`looper_signal_graph_set` replaces the graph and the inputs it drives. When
it refuses a graph, it says why in `reason` and changes nothing. Inputs the
old graph drove and the new one does not go back to their own signal, as
after `looper_signal_graph_clear`. `looper_signal_graph_release` takes one
input out of the graph, which is what a client does when it sets a single
signal on that input. `looper_signal_graph_value` is what the analog model
asks when the firmware reads an input: it returns true and the value when
the graph drives that input, and false when the input's own signal applies.

A graph holds at most `LOOPER_SIGNAL_GRAPH_CAPACITY` nodes (64 by default),
and an input has at most one target.

A node names its inputs by position, and only earlier positions. That is the
whole of the protection against cycles. A value the firmware wrote is read as
it was left, so feeding an output back into an input is a source and not a
loop.

The graph is evaluated **per moment**. A node that two paths share, and a
noise generator, give one value for that moment, however many paths ask. An
`a - a` of a noisy node is therefore zero, as it should be. The wire form is
described in [The signal graph on the wire](protocol.md#the-signal-graph-on-the-wire).

### Linear systems

A node of kind `LINEAR` holds a discrete state space, `x[k+1] = A x[k] + B u[k]`
and `y = C x + D u`, and gives the output the firmware reads. A model is
turned into one by `looper-core` before it is sent; the runtime knows only
the matrices. The wire form is described in
[Linear systems on the wire](protocol.md#linear-systems-on-the-wire).

The system steps at its **own fixed step**, on the simulated clock, and not at
every firmware read. When the firmware reads, the system **catches up** to that moment, holding its
inputs at their current values, and answers with its latest outputs. A system
that several paths share steps once per moment, like any other node. On a
virtual clock the result is exact and repeatable.

A second output is read through a node of kind `PICK`. It takes output number
`source` of the linear node at position `left`.

`looper_signal_graph_set` replaces the graph, but a linear system keeps its
**state** when its size stays the same. Changing a parameter therefore does
not make a filter jump.

A system that is left unread for a very long time does not work off the
backlog. When more than 100 000 steps are due, it jumps to the present
instead.

### Model nodes

A node of kind `MODEL` holds a model that is not linear: friction,
saturation, a motor. Its `step` is written in Python in the project.
`looper-core` translates it to C and compiles it into a library of its own,
or, when that is not possible, has the model worker step it in Python. The
wire form is described in
[State models on the wire](protocol.md#state-models-on-the-wire).

The model steps at its **own fixed step**, like a linear system. When the
firmware reads, it **catches up** to that moment in one call, with its inputs
held at their current values, and answers with its latest outputs. Compiled,
that is one call across the library boundary for all the steps due, not one
per step. Interpreted, it is one request to the worker, whatever the number of
steps. A model that is left unread for a very long time jumps to the present
after 100 000 steps, as a linear system does.

A model whose step produces NaN or infinity **stops**. It holds its last
finite outputs, and `COMMAND_READ_SIGNAL_GRAPH` reports why. A reset with
`COMMAND_RESET_MODEL` puts the state back to its initial value and lets the
model run again.

#### A compiled model

The library is loaded with `LoadLibraryW` on Windows and `dlopen` elsewhere.
The path arrives as UTF-8, as the control protocol carries it, and is
converted for Windows. The library exports one function that returns a
description of its model. `looper_model_abi.h` defines it:

| Field | Meaning |
| --- | --- |
| `abi` | the version of the interface the library was built for, `LOOPER_MODEL_ABI` |
| `states`, `inputs`, `outputs`, `constants` | the size of the model |
| `advance` | the function that steps the model |

The runtime checks the description against the request before it uses the
library. A library built for another `abi`, or whose sizes differ from the
request, is refused with a reason, and the library is unloaded again.
`advance(state, constants, input, output, dt, steps)` takes all the steps in
one call, so that the compiler can inline the step into its own loop.

#### An interpreted model

An interpreted model is stepped by the **model worker**, a Python process that
`looper-core` starts beside the simulation, one per simulation. The two share
one region of memory and two named semaphores: one for the request and one for
the answer. The simulation writes a request into the memory and posts the first
semaphore. The worker steps the model, writes the answer and posts the second.
One request is out at a time, because the runtime has one thread.

The worker may be held at a breakpoint for as long as its author likes, so
there is no timeout. The simulation waits in rounds of 100 ms, and after each
it asks whether the worker still lives. Only a worker that has ended is an
error: every interpreted model then reports *the model worker ended*, and the
firmware keeps running.

The worker is written for Windows, Linux and macOS, but only the Windows side
has been tried. Linux and macOS are untested.

### Capacities

| Constant | Default | Limits |
| --- | --- | --- |
| `LOOPER_SIGNAL_GRAPH_CAPACITY` | 64 | nodes in a graph |
| `LOOPER_SIGNAL_GRAPH_SYSTEMS` | 8 | linear systems in a graph |
| `LOOPER_LINEAR_STATES` | 8 | states of one system |
| `LOOPER_LINEAR_PORTS` | 4 | inputs, and outputs, of one system |
| `LOOPER_MODEL_STATES` | 256 | states of one state model, a vector counting one per value |
| `LOOPER_MODEL_PORTS` | 8 | inputs, and outputs, of one state model |
| `LOOPER_MODEL_CONSTANTS` | 256 | constants of one state model |

## The journal

Hooks answer what an adapter does. The journal answers whether it was called.

```c
looper_journal_clear();
/* ... run the firmware ... */
assert(looper_journal_count_of("adapter_blinky_millis") > 0u);
```

Recording is opt in. Generated sources call `LOOPER_RECORD`, which expands to
nothing unless the translation unit is compiled with `-DLOOPER_JOURNAL`. Without
that define the generated code has no link time dependency on the runtime at
all.

The journal is a fixed size ring of `LOOPER_JOURNAL_CAPACITY` entries (256 by
default) and keeps the most recent calls.

## The control channel

```c
looper_channel_open("blinky");

while (looper_channel_is_open()) {
    looper_channel_poll();
    app_step();
}
```

The channel is the simulation's standard input and output, and its one client
is the tool that started it; see [One client](protocol.md#one-client). Opening
it points what the firmware prints at the error stream, so output and frames
never mix.

Command handling lives in `looper_commands.c`, separate from the transport, so
the behaviour of each command can be exercised by building a request and
inspecting the response without a process on the other end.

`looper_channel_poll` is deliberately not threaded and never blocks on
reading. It serves at most one pending request and returns. The firmware loop
stays the only thread in the process, which means a virtual clock cannot drift
under a client and a debugger shows one call stack. A request that has only
partly arrived is kept and completed on a later pass.

On a virtual clock with a client, the loop runs in step with that client
(`looper_pace.c`): the firmware is held until the client lets time pass, and
the answer to that request waits until the firmware has run through it. A
held loop keeps serving requests and rests on the host when there are none;
see [Running in step](protocol.md#running-in-step).

The cost is equally plain: a firmware that blocks for a second does not answer
for a second. That is a true picture of the firmware rather than an artefact of
the tooling.

`looper_socket.h` is the half dozen socket calls the bridges and the network
components use, with the platform difference settled in one place and
everything non blocking.

`looper_profile.h` and `looper_heap.h` are the two measuring parts: the first
is driven by the compiler's instrumentation hooks and sees time and stack,
the second by wrappers the linker puts in front of the allocator. Neither is
called by hand, and both report nothing in a build that did not ask for
them — which they say, rather than reporting zero.

The first keeps a second, costlier record on request: a tree of call paths,
each node a function reached one particular way, which is what a flame graph
is drawn from. It is held in a fixed number of nodes and is armed and
disarmed by the client, because it is paid for on every call whether or not
anybody is reading it. See
{core}`Which way the time went <checking.html#which-way-the-time-went>`.

## The entry point

Everything a simulation does around the firmware is the same in every
project: parse `--virtual`, `--control` and `--steps`, start the clock, open the
control channel when `--control` asks for it, and run a loop that polls it
until the channel closes. `looper_app.c` is that part,
and a project supplies only what is its own:

```c
static void prepare(void) { register_pins(); register_channels(); device_model_install(); }
static bool start(void)   { return bsp_init() && app_init(); }
static void step(void)    { bsp_run(); app_run(); }

int main(int argc, char **argv) {
    static const looper_application_t application = {
        .name = "my-firmware", .prepare = prepare, .start = start, .step = step};
    return looper_main(argc, argv, &application);
}
```

A `start` that returns false stops the loop from running the firmware but
not the process: the control channel stays open so the journal can be read,
and the journal is written to the error stream as well. A firmware that
refuses to start means it, and running it on half initialised state crashes
on pointers that were never set.

This is the one part of the runtime that writes to a stream, which is why it
is a target of its own. The model and the protocol say nothing; they answer
over the control channel.

## Building it

With CMake:

```cmake
target_link_libraries(my_simulation PRIVATE looper::app)
```

Or `looper::runtime` alone, for a project that writes its own loop. See
{core}`Building a simulation <building.html#using-cmake-instead>`.

Or directly:

```console
$ gcc -std=c11 -DLOOPER_JOURNAL -o sim \
      build/generated/*.c $LOOPER_RUNTIME/src/*.c src/*.c \
      -Ibuild/generated -I$LOOPER_RUNTIME/include -IInc -Isrc -lws2_32
```

`-lws2_32` is needed on Windows for the sockets the bridges and components use; on Linux and macOS nothing
extra is required.
