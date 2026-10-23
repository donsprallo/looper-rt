# looper's C runtime as a CMake dependency.
#
# A firmware project that already builds with CMake should not have to adopt
# a second build system to get a simulation. This exposes the library targets
# and the function that declares a simulation executable:
#
#   include(FetchContent)
#   FetchContent_Declare(looper_rt
#       GIT_REPOSITORY https://example.invalid/looper-rt.git
#       GIT_TAG        v0.1.0)
#   FetchContent_MakeAvailable(looper_rt)
#
#   looper_add_simulation(my-simulation
#       DEFINITION ${CMAKE_CURRENT_SOURCE_DIR}/looper.toml
#       GENERATED  ${CMAKE_CURRENT_BINARY_DIR}/generated
#       SOURCES    src/main.c src/device_model.c
#       INCLUDES   Inc)
#
# The targets:
#
#   looper::runtime     the clock, the models, the protocol and the server
#   looper::components  the simulated devices looper ships
#   looper::app         the entry point every simulation shares
#
# Generating the adapter needs looper-cli, which is a separate project. A
# project that commits what it generated configures with -DLOOPER_GENERATE=OFF
# and needs nothing but this repository.
cmake_minimum_required(VERSION 3.20)
project(looper_rt C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

add_subdirectory(src/looper_rt/runtime)
add_subdirectory(src/looper_rt/components)
include(LooperSimulation)
