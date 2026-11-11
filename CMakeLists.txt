# ninjam-balcony -- a listener bot for a Ninjam room. See README.md, DESIGN.md.

cmake_minimum_required(VERSION 3.22)
project(ninjam_balcony VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(BALCONY_LIBRARY_TESTS "Build and run the chalkwalk libraries' own suites" ON)

# The libraries, as submodules under libs/, each overridable with
# CHALKWALK_<NAME>_DIR (cmake/ChalkwalkLibrary.cmake). Order matters only in
# that the first to add a library wins; jambot's nested copies are reused.
include(cmake/ChalkwalkLibrary.cmake)
chalkwalk_add_library(ninjam libs/ninjam)
chalkwalk_add_library(dsp libs/dsp)
chalkwalk_add_library(jambot libs/jambot)

# Opus, vendored (modules/opus, a release tag; owner's call on D4): the build
# needs nothing installed. Static, float API, no tests, programs or install.
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(modules/opus EXCLUDE_FROM_ALL)

# Everything but main(), so the tests link the same code the bot runs.
add_library(balcony_core STATIC
    src/Archive.cpp
    src/Balcony.cpp
    src/Json.cpp
    src/Listeners.cpp
    src/Room.cpp
    src/Mixer.cpp
    src/OpusStream.cpp
    src/WebServer.cpp
    src/Version.cpp)
target_include_directories(balcony_core PUBLIC src)
target_link_libraries(balcony_core
    PUBLIC chalkwalk::ninjam chalkwalk::dsp chalkwalk::jambot Opus::opus)
if(NOT MSVC)
    target_compile_options(balcony_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

target_link_libraries(balcony_core PUBLIC chalkwalk::dsp::measure)

add_executable(balcony src/main.cpp)
target_link_libraries(balcony PRIVATE balcony_core)

add_executable(balcony-render tools/render.cpp)
target_link_libraries(balcony-render PRIVATE balcony_core)

# A stand-in player for tests against a local ninjamsrv (never a public room).
add_executable(balcony-fake-player tools/fake-player.cpp)
target_link_libraries(balcony-fake-player PRIVATE chalkwalk::jambot)

# Our own switch, not BUILD_TESTING: chalkwalk-ninjam forces that OFF in the
# cache so xiph's projects do not register their tests.
option(BALCONY_TESTS "Build the balcony's tests" ON)
if(BALCONY_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
