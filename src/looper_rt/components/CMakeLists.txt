cmake_minimum_required(VERSION 3.20)
project(looperComponents C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Simulated devices looper ships ready to use, and the two bridges.
#
# A device is a component in the sense the bus model means: it answers on a
# bus at an address and knows nothing about the board it is on. A project
# gives it what is board specific -- a reference voltage, where an interrupt
# goes -- and attaches it.
#
# A bridge is a component that answers nothing. It carries what crosses a
# bus to a socket and back, so that something outside the process can take
# the part of a peer.
add_library(looperComponents STATIC
    src/looper_as8510.c
    src/looper_as8510_conversion.c
    src/looper_mcp23x08.c
    src/looper_tcp.c
    src/looper_udp.c
)

target_include_directories(looperComponents PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(looperComponents PUBLIC looperRuntime)

if (MSVC)
    target_compile_options(looperComponents PRIVATE /Zi)
else ()
    target_compile_options(looperComponents PRIVATE -g)
endif ()

add_library(looper::components ALIAS looperComponents)
