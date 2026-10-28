# The control protocol

A simulation started with `--control` speaks the protocol over its standard
input and output. Its one client is the tool that started it: `looper-cli run`
writes a request to the simulation's standard input and reads exactly one
response, carrying the same id, from its standard output. Everybody else --
windows, tests -- talks to that tool, not to the simulation.

The schema is `proto/looper_control.proto`.

## Framing

Protobuf messages are not self delimiting, so every message is preceded by its
length:

```text
+--------------------+---------------------------+
| length, 4 bytes LE | encoded message, N bytes   |
+--------------------+---------------------------+
```

A frame is at most 32768 bytes. Both sides reject anything larger rather than
allocating on a peer's word.

## Commands

| Command | Reads | Writes |
| --- | --- | --- |
| `COMMAND_HELLO` | firmware name, time mode, where the image is | |
| `COMMAND_READ_TIME` | elapsed microseconds | |
| `COMMAND_LIST_PINS` | every registered pin | |
| `COMMAND_DRIVE_PIN` | | one input pin |
| `COMMAND_ADVANCE_TIME` | | the virtual clock |
| `COMMAND_READ_JOURNAL` | recorded adapter calls | |
| `COMMAND_CLEAR_JOURNAL` | | the journal |
| `COMMAND_LIST_CHANNELS` | every registered analog channel | |
| `COMMAND_SET_SIGNAL` | | the generator on one input channel |
| `COMMAND_SET_SIGNAL_GRAPH` | | the signal graph and the inputs it drives |
| `COMMAND_READ_TRACE` | recorded samples after a sequence | |
| `COMMAND_CLEAR_TRACE` | | the trace |
| `COMMAND_LIST_WATCHES` | every watched firmware variable | |
| `COMMAND_WATCH_VARIABLE` | the new watch | the set of watches |
| `COMMAND_REMOVE_WATCH` | | the set of watches |
| `COMMAND_WRITE_VARIABLE` | | one firmware variable |
| `COMMAND_LIST_BUSES` | every registered bus | |
| `COMMAND_LIST_COMPONENTS` | every attached component | |
| `COMMAND_READ_BUS_TRACE` | recorded exchanges after a sequence | |
| `COMMAND_CLEAR_BUS_TRACE` | | the bus recording |
| `COMMAND_SEND_BUS_MESSAGE` | | one message onto a bus |
| `COMMAND_ATTACH_COMPONENT` | | takes a peer's part on a bus |
| `COMMAND_DETACH_COMPONENT` | | gives it up |

Every response carries the elapsed time and the time mode, so a client never
has to ask twice to know when a reading was taken.

Four refusals are part of the contract rather than errors:

- `COMMAND_DRIVE_PIN` on an output answers `STATUS_INVALID_ARGUMENT`. Only the
  firmware drives its own outputs.
- `COMMAND_ADVANCE_TIME` in real time mode answers `STATUS_UNSUPPORTED`. The
  host clock is not a thing a client steers.

## Running in step

A simulation started with `--virtual` and `--control` is held: the firmware
does not run until its client lets time pass. `COMMAND_ADVANCE_TIME` releases
it for that many microseconds, and the answer comes once the firmware has run
through them -- carrying the time the clock reached, which is the time asked
for or at most one idle pass beyond it. Then the firmware is held again.

While it is held the simulation still answers everything else: the client
reads what happened while the clock stands still, and nothing moves under it.
That makes a run exact and repeatable -- one second asked for is one second
run, however fast or loaded the host -- and two runs of the same test see the
same thing.

Without `--control` nobody could let the firmware go, so it runs freely: on a
virtual clock each pass of the loop costs its idle time and the clock moves
as fast as the host computes.
- `COMMAND_WATCH_VARIABLE` with an address outside the running image answers
  `STATUS_INVALID_ARGUMENT`. An address from a stale build would otherwise
  take the firmware down.
- `COMMAND_WRITE_VARIABLE` on a `const` variable, or with a value the variable
  cannot hold, answers `STATUS_INVALID_ARGUMENT`.

## One client

The simulation serves one request per pass of the firmware loop. A client
asking faster than the firmware runs therefore waits for the firmware rather
than the firmware standing still for it.

Several windows and tests may look at one simulation, but they do so through
the tool, which is the only client the simulation has. Whatever they do acts
on the one simulation: whoever clears a recording clears it for everybody.

Nothing blocks on reading. A request that has only partly arrived is kept
and completed on a later pass, so a tool that says nothing costs the
firmware nothing. When the tool goes away its end of the pipe closes, and
the simulation ends its loop: there is nobody left to answer.

Opening the channel takes standard output away from the firmware. What it
prints goes to the error stream instead, so a stray `printf` never lands in
the middle of a frame.

## The signal graph on the wire

`COMMAND_SET_SIGNAL_GRAPH` (33) replaces the whole graph in one request. It
carries two lists:

| Message | Fields |
| --- | --- |
| `SignalNode` | `kind`, `signal` (what a generator produces), `source` (the channel, pin or watch a source reads), `left` and `right` (the inputs of an operation) |
| `SignalTarget` | `channel` (the analog input), `node` (the node it reads) |

Nodes go over the wire in an order where each node comes after the nodes it
reads, and `left` and `right` name those by position. The runtime refuses an
input that is not an earlier position. That alone keeps a graph free of
cycles, and the runtime needs no search for them.

A graph that is refused changes nothing: the inputs keep what drove them.
`COMMAND_SET_SIGNAL` still works;
setting a single signal on an input takes that input out of the graph.
`Channel.driven_by_graph` (field 8) tells a client that the graph drives an
input. `Waveform` has a sixth value, `WAVEFORM_NOISE` (5); see
[The signal graph](runtime.md#the-signal-graph).

## Watching a firmware variable

A watch is an address the simulation reads once per firmware loop iteration,
recording changes into the same trace the analog channels use. Its channel
number is allocated by the simulation from the top of the range, so it never
collides with a channel the firmware registered.

Addresses are the one thing a client cannot take at face value. The debug
information of a build names the addresses the linker chose, and both Windows
and Linux relocate an image when they load it. So the two halves come from
the two places that can answer honestly:

| Half | Source |
| --- | --- |
| the base the executable was linked against | the executable file |
| the base the process was loaded at | `COMMAND_HELLO` |

The process cannot report the first: a relocating loader overwrites the header
field that held it, so a process asking itself gets its own runtime base back.
The executable file still has it, and a client reads that file anyway to get
at the debug information -- which is also what keeps the two from describing
different builds.

## Two implementations, one wire

The Python side uses the reference protobuf runtime with bindings generated by
`protoc`. The C side does not: `src/looper_rt/runtime/src/looper_pb.c` implements the
varint and length delimited encodings by hand, in about 150 lines.

That was a deliberate trade, and what decides it is the response rather than
the request. A `Response` is one message carrying the answer to every command,
and twenty of its fields are repeated. A generator such as nanopb offers three
ways to hold those, and the runtime can take none of them:

| Way | Why not |
| --- | --- |
| pointer fields | they allocate, and nothing allocates in the answer path |
| static arrays | 512 samples, 512 pin changes, 192 profiles, 128 exchanges and 96 timings of 32 bands each, all present at once: over a hundred kilobytes in one structure, against a frame that may not exceed 32 KB |
| callbacks | the repeated fields stay written by hand, which is what `looper_protocol_encode.c` already is |

The second half of the reason is that `looper_pin_t`, `looper_channel_t` and
`looper_timing_t` are the model's own types, used by the pin model, the journal
and the profiler. A generated set of wire structures beside them would add a
copy where the response today holds a pointer.

What is left to write by hand is small: the schema uses scalars, enums, strings
and one level of nesting.

The risk of a hand written codec is that it is subtly wrong in a way unit tests
written by the same author do not catch. The mitigation is that the two
implementations never see each other, only the wire:
`tests/integration/test_blinky_simulation.py` in `looper-core` starts a real
simulation and exercises every command against it. A disagreement about the
encoding fails that test.

## Changing the schema

The schema lives here, with the runtime that serves it. The Python bindings
generated from it live in `looper-core`, which is what imports them, and are
regenerated there:

```console
$ uv run python scripts/generate_protocol.py
```

Changing a field number is a breaking change: the C side carries the same
numbers as `#define`s in `looper_protocol_fields.h` and has to be changed with
it. `tests/test_protocol_fields.py` reads both files as text and fails when
they drift, which is why it lives here rather than with the client: this is
the one repository that holds both halves.

The enums are the other half of the same agreement, and no test covers them.
`Command`, `Status`, `VariableEncoding` and eight others travel as numbers too,
and `looper_types.h` mirrors all seventy values by hand. A command number that
moved on one side only would be as silent as a field number that moved: nothing
would reject it, and only `tests/integration/test_blinky_simulation.py` in
`looper-core` would notice, and only for the commands it exercises.

## The version on the wire

A simulation answers `COMMAND_HELLO` with the protocol version it was built
against, and a client refuses one it cannot read rather than reading it
wrongly. The number lives in two places that have to agree:

| Side | Where |
| --- | --- |
| runtime | `LOOPER_PROTOCOL_VERSION` in `looper_protocol.h` |
| client | `PROTOCOL_VERSION` in `looper.infrastructure.ipc.protobuf_session` |

That pair spans two repositories, so it is checked from the side that has
both installed: `tests/infrastructure/ipc/test_protocol_version.py` in
`looper-core` reads this header out of the installed runtime and fails if
the two numbers differ.

Raise it when a field changes meaning or a number moves. Adding a field at a
fresh number does not need it: an older client ignores what it does not know,
and a newer one reads the default from an older runtime.

A simulation built before the protocol carried a version answers zero, because
proto3 cannot tell an unset number from a zero. That is reported as its own
case -- "too old to say" rather than "version zero" -- and the fix is to
rebuild the simulation.

## Talking to a simulation

Nobody but `looper-cli run` talks to a simulation directly. It starts the
executable with `--control`, holds the session on its pipes, and serves the
inspector contract over gRPC to everybody else.

One request is answered by one response, which makes the session unsafe to
share between threads. The tool keeps it on one thread and lets every other
caller queue for it.
