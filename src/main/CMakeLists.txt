set(MODULE_ABI_DIR "" CACHE PATH "Directory containing module_abi.h")

if(NOT MODULE_ABI_DIR AND DEFINED ENV{CUBICLUA_ROOT} AND EXISTS "$ENV{CUBICLUA_ROOT}/src/dynmod/module_abi.h")
    set(MODULE_ABI_DIR "$ENV{CUBICLUA_ROOT}/src/dynmod")
endif()

if(NOT MODULE_ABI_DIR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../cubic_lua/cubic_arduino/cubic-develop/src/dynmod/module_abi.h")
    set(MODULE_ABI_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../cubic_lua/cubic_arduino/cubic-develop/src/dynmod")
endif()

if(NOT EXISTS "${MODULE_ABI_DIR}/module_abi.h")
    message(FATAL_ERROR "module_abi.h not found. Set MODULE_ABI_DIR or CUBICLUA_ROOT.")
endif()

idf_component_register(
    SRCS
        "hidpad_module.c"
        "hid_report_parser.c"
    INCLUDE_DIRS
        "."
        "${MODULE_ABI_DIR}"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -fPIC
    -fvisibility=hidden
    -fdata-sections
    -ffunction-sections
    -fno-jump-tables
    -fno-tree-switch-conversion
    -fno-tree-loop-distribute-patterns
    -fconserve-stack
    -fno-builtin-memcpy
    -fno-builtin-memmove
    -fno-builtin-memset
    -fno-builtin-strlen
    -Wframe-larger-than=256
)
