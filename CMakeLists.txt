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

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED IMPORTED_TARGET opus)

# Everything but main(), so the tests link the same code the bot runs.
add_library(balcony_core STATIC
    src/Archive.cpp
    src/Json.cpp
    src/Listeners.cpp
    src/Mixer.cpp
    src/OpusStream.cpp
    src/WebServer.cpp
    src/Version.cpp)
target_include_directories(balcony_core PUBLIC src)
target_link_libraries(balcony_core
    PUBLIC chalkwalk::ninjam chalkwalk::dsp chalkwalk::jambot PkgConfig::OPUS)
if(NOT MSVC)
    target_compile_options(balcony_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

target_link_libraries(balcony_core PUBLIC chalkwalk::dsp::measure)

add_executable(balcony src/main.cpp)
target_link_libraries(balcony PRIVATE balcony_core)

add_executable(balcony-render tools/render.cpp)
target_link_libraries(balcony-render PRIVATE balcony_core)

# Our own switch, not BUILD_TESTING: chalkwalk-ninjam forces that OFF in the
# cache so xiph's projects do not register their tests.
option(BALCONY_TESTS "Build the balcony's tests" ON)
if(BALCONY_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
