cmake_minimum_required(VERSION 3.10)
project(proto LANGUAGES C VERSION 1.0.0)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_VISIBILITY_PRESET hidden)

option(PROTO_BUILD_TESTS "Build the protocol test suite" ON)
option(PROTO_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(PROTO_ENABLE_HARDENING "Enable compiler hardening flags" ON)
option(PROTO_CRC_TABLE "Use a 256-entry lookup table for CRC16/Modbus" OFF)
option(PROTO_FIND_MEMCHR "Use memchr to prefilter frame header candidates" OFF)

add_library(proto STATIC
    src/protocol.c
    src/crc16.c
)

if(PROTO_CRC_TABLE)
    target_compile_definitions(proto PRIVATE PROTO_CRC_USE_TABLE=1)
endif()

if(PROTO_FIND_MEMCHR)
    target_compile_definitions(proto PRIVATE PROTO_FIND_USE_MEMCHR=1)
endif()

target_compile_features(proto PUBLIC c_std_11)
target_include_directories(proto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(proto PRIVATE /W4 /WX)
else()
    target_compile_options(proto PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Werror
    )
    if(PROTO_ENABLE_HARDENING AND NOT PROTO_ENABLE_SANITIZERS
       AND NOT MSVC AND NOT MINGW)
        target_compile_options(proto PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>
            $<$<NOT:$<CONFIG:Debug>>:-fstack-protector-strong>
        )
    endif()

    if(PROTO_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(proto PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer -g
        )
        set_target_properties(proto PROPERTIES
            LINK_FLAGS "-fsanitize=address,undefined"
        )
    endif()
endif()

if(PROTO_BUILD_TESTS)
    enable_testing()

    set(PROTO_TESTS
        test_protocol
        test_crc16
        test_crc16_diff
        test_bounds
        test_properties
        test_fuzz
    )

    foreach(test_name IN LISTS PROTO_TESTS)
        add_executable(${test_name}
            tests/${test_name}.c
            tests/test_support.c
        )
        target_link_libraries(${test_name} PRIVATE proto)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )

        if(MSVC)
            target_compile_options(${test_name} PRIVATE /W4 /WX)
        else()
            target_compile_options(${test_name} PRIVATE
                -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Werror
            )
        endif()

        if(PROTO_ENABLE_SANITIZERS AND NOT MSVC)
            target_compile_options(${test_name} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer -g
            )
            set_target_properties(${test_name} PROPERTIES
                LINK_FLAGS "-fsanitize=address,undefined"
            )
        endif()

        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
