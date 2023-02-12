cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

add_library(
    lstalk
    lstalk.c
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(
        lstalk
        PUBLIC LSTALK_EXPORT
    )
elseif(BUILD_STATIC_LIBS)
    target_compile_definitions(
        lstalk
        PUBLIC LSTALK_STATIC
    )
endif()

if(NOT WIN32)
    target_link_libraries(
        lstalk
        PRIVATE
        m
    )
endif()

add_executable(
    lstalk_console
    console.c
)

target_link_libraries(
    lstalk_console
    lstalk
)

set_target_properties(
    lstalk_console
    PROPERTIES
    RUNTIME_OUTPUT_NAME console
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)

if(WITH_TESTS)
target_compile_definitions(
    lstalk
    PUBLIC
    LSTALK_TESTS
)
endif()

add_executable(
    lstalk_tests
    tests.c
)

set_target_properties(
    lstalk_tests
    PROPERTIES
    RUNTIME_OUTPUT_NAME tests
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)

target_link_libraries(
    lstalk_tests
    lstalk
)

