/* ===========================================================================
 *  memory.h — Memory configuration constants and helpers
 * =========================================================================== */
#ifndef M68K_MEMORY_H
#define M68K_MEMORY_H

#include "common/types.h"

/* ── Default memory map ── */
#define MEM_RAM_DEFAULT_SIZE   (1 * 1024 * 1024)   /* 1 MB RAM */
#define MEM_ROM_DEFAULT_SIZE   (64 * 1024)          /* 64 KB ROM */
#define MEM_ROM_DEFAULT_BASE   0xF00000             /* ROM at top of memory */
#define MEM_IO_DEFAULT_BASE    0xE00000             /* I/O devices */

/* ── Vector table addresses ── */
#define MEM_VEC_RESET_SSP      0x000000
#define MEM_VEC_RESET_PC       0x000004

/* ── ROM overlay latch ──
 * Writing any value to this address disables the ROM overlay at $000000.
 * On reset, ROM is mirrored to $000000 so the CPU can read the initial
 * SSP and PC vectors from ROM.  The bootloader writes here to unmap
 * the overlay and reveal RAM at $000000.  (Like Amiga/Gary chip.) */
#define MEM_ROM_OVERLAY_LATCH  0xE00FFE

/* ── Address space limits ── */
#define MEM_ADDR_SPACE_SIZE    (16 * 1024 * 1024)   /* 16 MB (24-bit) */

#endif /* M68K_MEMORY_H */
