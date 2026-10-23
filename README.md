# looper-rt

The C runtime a [`looper`](../looper-core) simulation is built from: the
clock, the pin and analog models, the journal, the profiler, the bus models,
the components, and the server that answers a client over the control
protocol.

Around ten thousand lines of C with no third party dependency, so that it
compiles wherever a firmware project does. It holds no Python beyond a single
module whose only job is to let the tooling find these sources in an
installed wheel the same way it finds them in a checkout.

## Taking it into a CMake project

```cmake
include(FetchContent)
FetchContent_Declare(looper_rt
    GIT_REPOSITORY https://example.invalid/looper-rt.git
    GIT_TAG        v0.1.0)
FetchContent_MakeAvailable(looper_rt)

looper_add_simulation(my-simulation
    DEFINITION ${CMAKE_CURRENT_SOURCE_DIR}/looper.toml
    GENERATED  ${CMAKE_CURRENT_BINARY_DIR}/generated
    SOURCES    src/main.c src/device_model.c
    INCLUDES   Inc)
```

Three targets come out of it:

| Target | Contents |
| --- | --- |
| `looper::runtime` | the model, the protocol and the server; says nothing to any stream |
| `looper::components` | the simulated devices looper ships |
| `looper::app` | the entry point every simulation shares, which has to print |

Generating the adapter from a firmware's headers needs `looper-cli`, which is
a separate project. A project that commits what it generated configures with
`-DLOOPER_GENERATE=OFF` and needs nothing but this repository.

## Documentation

```console
$ uv run --extra docs sphinx-build -b html -W docs docs/_build/html
```

`docs/runtime.md` describes the clock, the models, the journal and the
server; `docs/protocol.md` the wire format and the version on it. The tooling
that generates an adapter and the window that attaches to a running
simulation document themselves in their own repositories.

## Layout

| Path | Contents |
| --- | --- |
| `src/looper_rt/runtime/` | the runtime: clock, models, journal, protocol, server |
| `src/looper_rt/components/` | the simulated devices that ship with looper |
| `src/looper_rt/proto/` | the control protocol schema |
| `cmake/` | `looper_add_simulation` |
| `tests/` | that the schema and the runtime number the fields alike |

## The wire contract

The schema lives here because the runtime is what serves it. Every field
number in `src/looper_rt/proto/looper_control.proto` has one counterpart as a
`#define` in `looper_protocol_fields.h`, and a number that moved on one side
only is not a parse error but a value read into the wrong field.

`tests/test_protocol_fields.py` reads both files as text and fails when they
disagree. It lives here rather than with the client because this is the one
repository holding both halves.

A simulation also answers `COMMAND_HELLO` with `LOOPER_PROTOCOL_VERSION` from
`looper_protocol.h`, so a client can refuse a runtime it cannot read rather
than misreading it. Raise it when a field changes meaning or a number moves.

```console
$ uv run pytest
$ uv run ruff check .
```
