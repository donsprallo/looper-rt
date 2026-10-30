cmake_minimum_required(VERSION 3.20)
project(looperRuntime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The model, the protocol and the server. Writes to no stream: what it has
# to say, it says over the control channel.
add_library(looperRuntime STATIC
    src/looper_analog.c
    src/looper_binding.c
    src/looper_bridge.c
    src/looper_bus.c
    src/looper_bus_trace.c
    src/looper_can.c
    src/looper_can_wire.c
    src/looper_channel.c
    src/looper_channel_frames.c
    src/looper_channel_pipe.c
    src/looper_clock.c
    src/looper_commands.c
    src/looper_commands_bus.c
    src/looper_commands_graph.c
    src/looper_commands_profile.c
    src/looper_commands_watch.c
    src/looper_component_property.c
    src/looper_gpio_model.c
    src/looper_heap.c
    src/looper_heap_wrap.c
    src/looper_i2c.c
    src/looper_image.c
    src/looper_journal.c
    src/looper_pace.c
    src/looper_model_library.c
    src/looper_model_worker.c
    src/looper_model_worker_platform.c
    src/looper_pb.c
    src/looper_pin_trace.c
    src/looper_profile.c
    src/looper_profile_clock.c
    src/looper_profile_hooks.c
    src/looper_profile_paths.c
    src/looper_profile_stack.c
    src/looper_profile_table.c
    src/looper_protocol_decode.c
    src/looper_protocol_decode_graph.c
    src/looper_protocol_decode_linear.c
    src/looper_protocol_decode_model.c
    src/looper_protocol_elements.c
    src/looper_protocol_memory.c
    src/looper_protocol_encode.c
    src/looper_protocol_encode_memory.c
    src/looper_signal_graph.c
    src/looper_signal_graph_check.c
    src/looper_linear_system.c
    src/looper_state_model.c
    src/looper_socket.c
    src/looper_socket_datagram.c
    src/looper_spi.c
    src/looper_timing.c
    src/looper_trace.c
    src/looper_uart.c
    src/looper_watch.c
    src/looper_watch_memory.c
)

target_include_directories(looperRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32)
    target_link_libraries(looperRuntime PUBLIC ws2_32)
else ()
    target_link_libraries(looperRuntime PUBLIC m)
endif ()
# dlopen, for compiled state models: libdl before glibc 2.34, nothing after,
# and nothing on Windows or macOS, which is what CMAKE_DL_LIBS says.
target_link_libraries(looperRuntime PUBLIC ${CMAKE_DL_LIBS})
# shm_open and named semaphores for the model worker: librt and libpthread
# before glibc 2.34, part of the C library after and everywhere else.
if (UNIX AND NOT APPLE)
    target_link_libraries(looperRuntime PUBLIC rt pthread)
endif ()

# The entry point every simulation shares. Kept apart because it prints, and
# a project that wants the loop for itself simply does not link it.
add_library(looperApp STATIC
    src/looper_app.c
)

target_link_libraries(looperApp PUBLIC looperRuntime)

# Debug information, whatever the build type, for both targets.
#
# A simulation exists to be looked at, and the runtime is part of the
# program being looked at: its ring buffers and its socket are as much a
# part of what is running as the firmware is. Without this the variable
# browser would describe the firmware alone in a CMake build and the
# firmware plus the runtime in a direct one, which is two answers to one
# question.
#
# It costs size in a file nobody ships. A simulation is not firmware for a
# device; it is a program for looking at one.
foreach (target looperRuntime looperApp)
    if (MSVC)
        target_compile_options(${target} PRIVATE /Zi)
    else ()
        target_compile_options(${target} PRIVATE -g)
    endif ()
endforeach ()

add_library(looper::runtime ALIAS looperRuntime)
add_library(looper::app ALIAS looperApp)
