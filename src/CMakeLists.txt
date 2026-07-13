cmake_minimum_required(VERSION 3.16)

set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hidpad)

execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE HIDPAD_LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "-gcc" "-strip" HIDPAD_STRIP "${CMAKE_C_COMPILER}")

set(HIDPAD_COMPILE_FLAGS
    -O2
    -fPIC
    -fvisibility=hidden
    -fdata-sections
    -ffunction-sections
    -fno-jump-tables
    -fno-tree-switch-conversion
    -fconserve-stack
    -fno-builtin-memcpy
    -fno-builtin-memmove
    -fno-builtin-memset
    -fno-builtin-strlen
    -Wframe-larger-than=256
)

set(HIDPAD_LINK_FLAGS
    -shared
    -fPIC
    -static-libgcc
    -nostdlib
    -nostartfiles
    -Wl,--gc-sections
    -Wl,--strip-debug
    "-Wl,-Bsymbolic"
    "-Wl,--exclude-libs,ALL"
    "-Wl,-z,notext"
    "-Wl,--no-check-sections"
    "-Wl,--version-script=${CMAKE_CURRENT_LIST_DIR}/exports.map"
    "-Wl,-Map,${CMAKE_BINARY_DIR}/hidpad_so.map"
)

set(HIDPAD_OBJECT_DIR "${CMAKE_BINARY_DIR}/hidpad_objects")
set(HIDPAD_OBJECTS
    "${HIDPAD_OBJECT_DIR}/hidpad_module.o"
    "${HIDPAD_OBJECT_DIR}/hid_report_parser.o"
)

add_custom_command(
    OUTPUT ${HIDPAD_OBJECTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${HIDPAD_OBJECT_DIR}"
    COMMAND ${CMAKE_C_COMPILER} ${HIDPAD_COMPILE_FLAGS}
        -I"${MODULE_ABI_DIR}" -I"${CMAKE_CURRENT_LIST_DIR}/main"
        -c "${CMAKE_CURRENT_LIST_DIR}/main/hidpad_module.c"
        -o "${HIDPAD_OBJECT_DIR}/hidpad_module.o"
    COMMAND ${CMAKE_C_COMPILER} ${HIDPAD_COMPILE_FLAGS}
        -I"${MODULE_ABI_DIR}" -I"${CMAKE_CURRENT_LIST_DIR}/main"
        -c "${CMAKE_CURRENT_LIST_DIR}/main/hid_report_parser.c"
        -o "${HIDPAD_OBJECT_DIR}/hid_report_parser.o"
    DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/main/hidpad_module.c"
        "${CMAKE_CURRENT_LIST_DIR}/main/hid_report_parser.c"
        "${CMAKE_CURRENT_LIST_DIR}/main/hid_report_parser.h"
        "${MODULE_ABI_DIR}/module_abi.h"
    COMMENT "Compile hidpad module objects"
)

add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/hidpad.so"
    COMMAND ${CMAKE_C_COMPILER} ${HIDPAD_LINK_FLAGS}
        -o "${CMAKE_BINARY_DIR}/hidpad.so"
        ${HIDPAD_OBJECTS} "${HIDPAD_LIBGCC}"
    COMMAND ${HIDPAD_STRIP}
        --strip-unneeded
        --remove-section=.comment
        --remove-section=.got.loc
        --remove-section=.dynamic
        --remove-section=.xt.lit
        --remove-section=.xt.prop
        --remove-section=.xtensa.info
        "${CMAKE_BINARY_DIR}/hidpad.so"
    DEPENDS ${HIDPAD_OBJECTS} "${CMAKE_CURRENT_LIST_DIR}/exports.map"
    COMMENT "Link hidpad.so"
)

add_custom_target(so DEPENDS "${CMAKE_BINARY_DIR}/hidpad.so")
add_custom_command(TARGET so POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_LIST_DIR}/../package/modules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/hidpad.so"
        "${CMAKE_CURRENT_LIST_DIR}/../package/modules/hidpad.so"
    COMMENT "Copy hidpad.so into package/modules"
)
