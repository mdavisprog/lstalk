cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)

set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)
set(LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

function(set_output_properties TARGET_NAME OUTPUT_NAME)
    set_target_properties(
        ${TARGET_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_NAME ${OUTPUT_NAME}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}
    )
endfunction()

function(add_program TARGET_NAME OUTPUT_NAME SOURCES)
    add_executable(
        ${TARGET_NAME}
        ${SOURCES}
    )

    set_output_properties(
        ${TARGET_NAME}
        ${OUTPUT_NAME}
    )

    target_link_libraries(
        ${TARGET_NAME}
        lstalk
    )
endfunction()

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

    set_output_properties(
        lstalk_console
        console
    )
else()
    add_program(
        lstalk_console
        console
        console.c
    )
endif()

set_target_properties(
    lstalk_console
    PROPERTIES
    RUNTIME_OUTPUT_NAME console
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

    set_output_properties(
        lstalk_tests
        tests
    )
else()
    add_program(
        lstalk_tests
        tests
        tests.c
    )
endif()

if(WITH_EXAMPLES)
add_program(
    example_basic
    example_basic
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic.c
)
endif()
