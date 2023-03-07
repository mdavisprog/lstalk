cmake_minimum_required(VERSION 3.21)

project(lstalk)

set(CMAKE_C_STANDARD 99)
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()

message("Building with configuration: ${CMAKE_BUILD_TYPE}")

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

add_library(lstalk_static STATIC lstalk.c)
add_library(lstalk_dynamic SHARED lstalk.c)

target_compile_definitions(
    lstalk_static
    PUBLIC LSTALK_STATIC
    PRIVATE LSTALK_LIB
)

target_compile_definitions(
    lstalk_dynamic
    PUBLIC LSTALK_EXPORT
    PRIVATE LSTALK_LIB
)

set_target_properties(
    lstalk_static
    PROPERTIES
    PUBLIC_HEADER lstalk.h
    ARCHIVE_OUTPUT_NAME lstalk
    RUNTIME_OUTPUT_NAME lstalk
)

set(LIBRARY_NAME lstalkshared)
if(WIN32)
    set(LIBRARY_NAME lstalk)
endif()

set_target_properties(
    lstalk_dynamic
    PROPERTIES
    PUBLIC_HEADER lstalk.h
    ARCHIVE_OUTPUT_NAME lstalkdll
    LIBRARY_OUTPUT_NAME ${LIBRARY_NAME}
    RUNTIME_OUTPUT_NAME lstalk
)

if(WITH_TESTS)
    target_compile_definitions(
        lstalk_static
        PUBLIC LSTALK_TESTS
    )

    target_compile_definitions(
        lstalk_dynamic
        PUBLIC LSTALK_TESTS
    )
endif()

if(BUILD_SHARED_LIBS)
    add_library(lstalk ALIAS lstalk_dynamic)
    message("Building apps and examples with dynamic linkage")
else()
    add_library(lstalk ALIAS lstalk_static)
    message("Building apps and examples with static linkage")
endif()

install(
    TARGETS lstalk_static lstalk_dynamic
    DESTINATION ${LIB_DIR}
)

if(NO_LIB)
    add_executable(
        lstalk_console
        console.c
        lstalk.c
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

if(NO_LIB)
    add_executable(
        lstalk_tests
        lstalk.c
        tests.c
    )

    set_output_properties(
        lstalk_tests
        tests
    )

    add_executable(
        lstalk_test_server
        lstalk.c
        test_server.c
    )

    set_output_properties(
        lstalk_test_server
        test_server
    )
else()
    add_program(
        lstalk_tests
        tests
        tests.c
    )

    add_program(
        lstalk_test_server
        test_server
        test_server.c
    )
endif()

if(WITH_EXAMPLES)
add_program(
    example_basic
    example_basic
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic.c
)

add_program(
    example_document_symbols
    example_document_symbols
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/document_symbols.c
)
endif()
