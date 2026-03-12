/* ===========================================================================
 *  types.h — Fundamental types, endian helpers, and common definitions
 *  for the entire M68000 project.
 *
 *  The MC68000 is a big-endian, 32-bit-register, 24-bit-address CPU.
 *  Host machine is assumed little-endian (x86/ARM).
 * =========================================================================== */
#ifndef M68K_TYPES_H
#define M68K_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Short type aliases ── */
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

/* ── Operation size (byte / word / long) ── */
typedef enum {
    SIZE_BYTE = 0,   /* 8-bit  */
    SIZE_WORD = 1,   /* 16-bit */
    SIZE_LONG = 2    /* 32-bit */
} OperationSize;

/* Byte count for each operation size */
static inline u32 sizeInBytes(OperationSize sz) {
    static const u32 table[] = { 1, 2, 4 };
    return table[sz];
}

/* Bit count for each operation size */
static inline u32 sizeInBits(OperationSize sz) {
    static const u32 table[] = { 8, 16, 32 };
    return table[sz];
}

/* Mask for each operation size */
static inline u32 sizeMask(OperationSize sz) {
    static const u32 table[] = { 0xFF, 0xFFFF, 0xFFFFFFFF };
    return table[sz];
}

/* MSB mask for each operation size */
static inline u32 sizeMsb(OperationSize sz) {
    static const u32 table[] = { 0x80, 0x8000, 0x80000000 };
    return table[sz];
}

/* ── Endian conversion (68000 is big-endian, host is little-endian) ── */
static inline u16 swap16(u16 v) {
    return (u16)((v << 8) | (v >> 8));
}

static inline u32 swap32(u32 v) {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x000000FFu) << 24);
}

/* ── Sign extension ── */
static inline s32 signExtend8(u8 v)  { return (s32)(s8)v;  }
static inline s32 signExtend16(u16 v) { return (s32)(s16)v; }

/* ── Bit manipulation ── */
#define BIT(n)          (1u << (n))
#define BITS(v, hi, lo) (((v) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1))

/* ── 24-bit address mask (MC68000 has 24-bit address bus) ── */
#define ADDR_MASK  0x00FFFFFFu

/* ── Result codes returned by emulator subsystems ── */
typedef enum {
    RESULT_OK = 0,
    RESULT_ERROR,
    RESULT_BUS_ERROR,
    RESULT_ADDRESS_ERROR,
    RESULT_ILLEGAL_INSTRUCTION,
    RESULT_PRIVILEGE_VIOLATION,
    RESULT_HALTED,
    RESULT_DOUBLE_FAULT
} ResultCode;

#endif /* M68K_TYPES_H */
