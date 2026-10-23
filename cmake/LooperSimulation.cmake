# Building a simulation from a CMake project.
#
# The generation step is the only part that needs the Python tool. It reads
# the adapter headers, which is how it knows what to write, and it writes a
# list of what it wrote so that this file can name the sources rather than
# glob for them.
#
# A project that would rather not depend on the tool commits the generated
# sources and configures with -DLOOPER_GENERATE=OFF. They are deterministic
# from the headers and the contract, so a committed copy is a copy of what
# the tool would produce, not a fork of it.

option(LOOPER_GENERATE "Regenerate the simulation sources with looper-cli" ON)

set(LOOPER_CLI "looper-cli" CACHE STRING
    "Command that runs the looper command line tool")

# Cached so that it survives the scope it is set in. A project brings looper
# in with add_subdirectory or FetchContent, which makes this a child scope,
# and a plain variable set here is not visible where the function is called.
set(LOOPER_GENERATED_MANIFEST "looper_generated.cmake" CACHE INTERNAL
    "Name of the file the generator lists its output in")

# Run the generator for one build definition.
#
# Generation happens while CMake configures rather than while it builds,
# because the set of files it produces is part of what has to be configured.
# A header added to the adapter changes that set, and CMake has to know
# before it writes a build graph rather than after.
function(_looper_generate definition generated_directory)
    message(STATUS "looper: generating from ${definition}")
    separate_arguments(command NATIVE_COMMAND "${LOOPER_CLI}")
    execute_process(
        COMMAND ${command} generate --definition "${definition}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if (NOT status EQUAL 0)
        message(FATAL_ERROR
            "looper-cli could not generate the simulation:\n${output}\n"
            "Install the tool, point -DLOOPER_CLI at it, or commit the "
            "generated sources and configure with -DLOOPER_GENERATE=OFF.")
    endif ()
    if (NOT EXISTS "${generated_directory}/${LOOPER_GENERATED_MANIFEST}")
        message(FATAL_ERROR
            "looper-cli wrote nothing to ${generated_directory}. Check that "
            "the generated directory in ${definition} is the one given here.")
    endif ()
endfunction()

# Declare a simulation executable.
#
#   looper_add_simulation(my-simulation
#       DEFINITION ${CMAKE_CURRENT_SOURCE_DIR}/looper.toml
#       GENERATED  ${CMAKE_CURRENT_SOURCE_DIR}/build/generated
#       SOURCES    src/device_model.c src/platform.c
#       INCLUDES   Inc src)
#
# DEFINITION  the build definition the generator reads
# GENERATED   where the generator writes, as the definition says
# SOURCES     sources to compile, instead of the ones the definition names
# INCLUDES    header directories to add to the ones the definition names
#
# Leaving SOURCES out is the usual case: the definition already says which
# directories the firmware lives in and which units are excluded, and
# repeating that in CMake is two places to keep in step. Passing SOURCES is
# for a project whose CMake already knows its firmware sources and would
# rather hand them over than name them twice.
function(looper_add_simulation target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "DEFINITION;GENERATED" "SOURCES;INCLUDES")

    if (NOT arg_DEFINITION)
        message(FATAL_ERROR "looper_add_simulation(${target}) needs a DEFINITION")
    endif ()
    if (NOT arg_GENERATED)
        message(FATAL_ERROR "looper_add_simulation(${target}) needs a GENERATED directory")
    endif ()

    if (LOOPER_GENERATE)
        _looper_generate("${arg_DEFINITION}" "${arg_GENERATED}")
    endif ()

    set(manifest "${arg_GENERATED}/${LOOPER_GENERATED_MANIFEST}")
    if (NOT EXISTS "${manifest}")
        message(FATAL_ERROR
            "No generated simulation at ${arg_GENERATED}. Configure with "
            "-DLOOPER_GENERATE=ON, or commit what looper-cli generate writes.")
    endif ()
    include("${manifest}")

    # Configuring again when the definition changes: the set of generated
    # files follows from it, and a stale set is the kind of wrong that still
    # builds.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_DEFINITION}")

    if (arg_SOURCES)
        set(project_sources ${arg_SOURCES})
    else ()
        set(project_sources ${LOOPER_PROJECT_SOURCES})
    endif ()

    add_executable(${target} ${project_sources} ${LOOPER_GENERATED_SOURCES})
    target_link_libraries(${target} PRIVATE looper::app looper::components)
    target_include_directories(${target} PRIVATE
        ${LOOPER_INCLUDE_DIRECTORIES}
        ${arg_INCLUDES}
    )
    target_compile_definitions(${target} PRIVATE ${LOOPER_COMPILE_DEFINITIONS})
    target_compile_options(${target} PRIVATE ${LOOPER_COMPILE_OPTIONS})

    # Instrumentation, when the definition asks for it. Safe to put on the
    # whole target: the options carry their own exclusion list, naming the
    # generated directory and looper's own sources by path, so the same
    # files are woven here as in a looper-cli build.
    target_compile_options(${target} PRIVATE ${LOOPER_INSTRUMENT_OPTIONS})

    # Putting looper in front of the allocator, when the definition asks.
    # A link option: it changes no code, only which malloc a call reaches.
    target_link_options(${target} PRIVATE ${LOOPER_LINK_OPTIONS})
    set_target_properties(${target} PROPERTIES
        C_STANDARD ${LOOPER_C_STANDARD}
        C_STANDARD_REQUIRED ON
    )
endfunction()
