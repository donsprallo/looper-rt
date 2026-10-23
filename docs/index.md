# looper-rt

The C runtime a `looper` simulation is built from: the clock, the pin and
analog models, the journal, the profiler, the bus models and the server that
answers a client over the control protocol. Around ten thousand lines of C
with no third party dependency, so that it compiles wherever a firmware
project does.

This repository holds no application. The tool that reads a firmware's
adapter headers, generates the host implementation and drives the build is
{core}`looper-core <index.html>`, and the window that attaches to a running
simulation is `looper-gui`. Both are separate projects.

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

| Page | Covers |
| --- | --- |
| [The runtime](runtime.md) | the C clock, pin model, journal and server |
| [The control protocol](protocol.md) | the wire format and the version on it |

```{toctree}
:maxdepth: 2
:caption: Contents

runtime
protocol
```
