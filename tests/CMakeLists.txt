cmake_minimum_required(VERSION 3.16)
project(ungula_command_lib_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

option(USE_LOCAL_DEPS "Use local sibling lib/ directory for UngulaCore" ON)

if(USE_LOCAL_DEPS)
    set(UNGULA_CORE_SRC ${CMAKE_CURRENT_LIST_DIR}/../../lib/src)
else()
    FetchContent_Declare(
        ungula_core
        GIT_REPOSITORY https://github.com/alexconesap/ungula-core.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(ungula_core)
    set(UNGULA_CORE_SRC ${ungula_core_SOURCE_DIR}/src)
endif()

set(LIB_COMMAND_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${LIB_COMMAND_SRC}
    ${UNGULA_CORE_SRC}
)

add_executable(test_ungula_command
    test_command_types.cpp
    test_command_envelope.cpp
    ${LIB_COMMAND_SRC}/ungula/command/command_selftest.cpp
)
target_link_libraries(test_ungula_command gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_ungula_command)
