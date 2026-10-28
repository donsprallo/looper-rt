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
