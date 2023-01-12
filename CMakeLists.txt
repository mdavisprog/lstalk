cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

add_library(
    lstalk
    lstalk.c
)

add_executable(
    lstalk_main
    main.c
)

set_target_properties(
    lstalk_main
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)

target_link_libraries(
    lstalk_main
    lstalk
)

if(WITH_TESTS)
target_compile_definitions(
    lstalk_main
    PUBLIC
    LSTALK_TESTS
)
endif()
