if(NOT TARGET Catch2::Catch2WithMain)
    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1)
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(balcony_tests
    BalconyTests.cpp
    JsonTests.cpp
    ListenersTests.cpp
    MixerTests.cpp
    OpusStreamTests.cpp
    RoomTests.cpp
    WebServerTests.cpp
    VersionTests.cpp)
target_link_libraries(balcony_tests PRIVATE balcony_core Catch2::Catch2WithMain)
target_include_directories(balcony_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(balcony_tests
    PRIVATE BALCONY_WORK_DIR="${PROJECT_SOURCE_DIR}/work")
add_test(NAME balcony COMMAND balcony_tests)

# The page's own logic, where node is there to run it.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME web COMMAND ${NODE_EXECUTABLE} --test
             ${PROJECT_SOURCE_DIR}/test/web/schedule.test.js)
endif()
