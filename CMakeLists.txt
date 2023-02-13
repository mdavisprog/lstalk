cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

add_library(
    lstalk
    lstalk.c
)

set_target_properties(
    lstalk
    PROPERTIES PUBLIC_HEADER lstalk.h
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(
        lstalk
        PUBLIC LSTALK_EXPORT
    )
else()
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

install(
    TARGETS lstalk
    DESTINATION ${LIB_DIR}
)

if(NO_LIB)
    add_executable(
        lstalk_console
        console.c
        lstalk.c
    )

    target_compile_definitions(
        lstalk_console
        PUBLIC LSTALK_STATIC
    )
else()
    add_executable(
        lstalk_console
        console.c
    )

    target_link_libraries(
        lstalk_console
        lstalk
    )
endif()

set_target_properties(
    lstalk_console
    PROPERTIES
    RUNTIME_OUTPUT_NAME console
    RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
)

if(BUILD_SHARED_LIBS)
    add_custom_command(
        TARGET lstalk_console
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:lstalk>
        $<TARGET_FILE_DIR:lstalk_console>
    )
endif()

if(WITH_TESTS)
target_compile_definitions(
    lstalk
    PUBLIC
    LSTALK_TESTS
)
endif()

if(NO_LIB)
    add_executable(
        lstalk_tests
        lstalk.c
        tests.c
    )

    target_compile_definitions(
        lstalk_tests
        PUBLIC LSTALK_STATIC
    )
else()
    add_executable(
        lstalk_tests
        tests.c
    )
endif()

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
