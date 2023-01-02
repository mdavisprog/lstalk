cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

add_executable(
    lstalk
    main.c
)

set_target_properties(
    lstalk
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)
