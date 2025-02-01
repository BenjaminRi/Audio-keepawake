cmake_minimum_required(VERSION 3.5)
project(audio-keepawake)

# Create an option to switch between a system sdl library and a vendored SDL library
option(AUDIO_KEEPAWAKE_VENDORED "Use vendored libraries" OFF)

option(AUDIO_KEEPAWAKE_USE_ASAN "Enable AddressSanitizer" OFF)

if(AUDIO_KEEPAWAKE_VENDORED)
    # This assumes you have added SDL as a submodule in vendored/SDL
    add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
else()
    # 1. Look for a SDL3 package,
    # 2. look for the SDL3-shared component, and
    # 3. fail if the shared component cannot be found.
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif()

add_executable(audio-keepawake src/main.c)

target_link_libraries(audio-keepawake PRIVATE SDL3::SDL3)

# Apply AddressSanitizer flags if enabled
if(AUDIO_KEEPAWAKE_USE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    target_compile_options(audio-keepawake PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_libraries(audio-keepawake PRIVATE -fsanitize=address)
endif()
