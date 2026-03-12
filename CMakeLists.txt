# ===========================================================================
#  M68000 — Motorola 68000 Emulator, Assembler, Compiler & Toolchain
#  Zero external dependencies. Everything built from scratch.
# ===========================================================================
cmake_minimum_required(VERSION 3.20)

project(m68000 VERSION 0.1.0 LANGUAGES C)

# Enable Objective-C on macOS (needed for native GUI)
if(APPLE)
    enable_language(OBJC)
endif()

# ── C Standard ──
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler Warnings ──
add_compile_options(-Wall -Wextra -Wpedantic)

# ── Build Type ──
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(M68K_DEBUG)
endif()

# ── Global Include Path ──
include_directories(${CMAKE_SOURCE_DIR}/src)

# ===========================================================================
#  Libraries
# ===========================================================================

# ── Common utilities ──
file(GLOB COMMON_SOURCES src/common/*.c)
add_library(m68k_common STATIC ${COMMON_SOURCES})

# ── Core emulator (CPU + bus + devices + machine) ──
file(GLOB_RECURSE CPU_SOURCES     src/cpu/*.c)
file(GLOB_RECURSE BUS_SOURCES     src/bus/*.c)
file(GLOB_RECURSE DEVICE_SOURCES  src/devices/*.c)
file(GLOB_RECURSE MACHINE_SOURCES src/machine/*.c)
add_library(m68k_core STATIC
    ${CPU_SOURCES}
    ${BUS_SOURCES}
    ${DEVICE_SOURCES}
    ${MACHINE_SOURCES}
)
target_link_libraries(m68k_core m68k_common)

# ── Assembler ──
file(GLOB_RECURSE ASM_SOURCES src/assembler/*.c)
add_library(m68k_asm_lib STATIC ${ASM_SOURCES})
target_link_libraries(m68k_asm_lib m68k_common)

# ── Disassembler ──
file(GLOB_RECURSE DIS_SOURCES src/disassembler/*.c)
add_library(m68k_dis_lib STATIC ${DIS_SOURCES})
target_link_libraries(m68k_dis_lib m68k_common)

# ── Compiler ──
file(GLOB_RECURSE CC_SOURCES src/compiler/*.c)
add_library(m68k_cc_lib STATIC ${CC_SOURCES})
target_link_libraries(m68k_cc_lib m68k_asm_lib m68k_common)

# ── GUI (platform-specific) ──
if(APPLE)
    add_library(m68k_gui STATIC src/gui/gui_macos.m)
    target_link_libraries(m68k_gui "-framework Cocoa" "-framework QuartzCore")
elseif(UNIX)
    add_library(m68k_gui STATIC src/gui/gui_linux.c)
    target_link_libraries(m68k_gui X11)
elseif(WIN32)
    add_library(m68k_gui STATIC src/gui/gui_win32.c)
    target_link_libraries(m68k_gui gdi32 user32)
endif()
target_link_libraries(m68k_gui m68k_common)

# ===========================================================================
#  Executables
# ===========================================================================

# Main emulator
add_executable(m68k_emu src/main.c)
target_link_libraries(m68k_emu m68k_core m68k_gui m68k_asm_lib m68k_dis_lib)

# Standalone assembler
add_executable(m68k_as src/tools/assembler_main.c)
target_link_libraries(m68k_as m68k_asm_lib m68k_common)

# Standalone disassembler
add_executable(m68k_dis src/tools/disassembler_main.c)
target_link_libraries(m68k_dis m68k_dis_lib m68k_common)

# Standalone compiler
add_executable(m68k_cc src/tools/compiler_main.c)
target_link_libraries(m68k_cc m68k_cc_lib m68k_asm_lib m68k_common)

# ===========================================================================
#  Tests
# ===========================================================================
enable_testing()
add_subdirectory(tests)
