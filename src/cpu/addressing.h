/* ===========================================================================
 *  addressing.h — MC68000 addressing modes
 *
 *  The 68000 encodes the effective address in a 6-bit field:
 *    bits 5-3 = mode,  bits 2-0 = register
 *
 *  Mode  Reg   Meaning
 *  ────  ───   ────────────────────────────────────────
 *  000   Dn    Data Register Direct
 *  001   An    Address Register Direct
 *  010   An    Address Register Indirect             (An)
 *  011   An    Address Register Indirect + Postinc   (An)+
 *  100   An    Address Register Indirect + Predec    -(An)
 *  101   An    Address Register Indirect + Disp      d16(An)
 *  110   An    Address Register Indirect + Index     d8(An,Xn)
 *  111   000   Absolute Short                        xxx.W
 *  111   001   Absolute Long                         xxx.L
 *  111   010   PC + Displacement                     d16(PC)
 *  111   011   PC + Index                            d8(PC,Xn)
 *  111   100   Immediate                             #xxx
 * =========================================================================== */
#ifndef M68K_ADDRESSING_H
#define M68K_ADDRESSING_H

#include "cpu/cpu.h"

/* ── Addressing mode enum (decoded from mode/reg fields) ── */
typedef enum {
    ADDR_MODE_DATA_REG,         /* Dn              */
    ADDR_MODE_ADDR_REG,         /* An              */
    ADDR_MODE_ADDR_IND,         /* (An)            */
    ADDR_MODE_ADDR_IND_POSTINC, /* (An)+           */
    ADDR_MODE_ADDR_IND_PREDEC,  /* -(An)           */
    ADDR_MODE_ADDR_IND_DISP,    /* d16(An)         */
    ADDR_MODE_ADDR_IND_INDEX,   /* d8(An,Xn)       */
    ADDR_MODE_ABS_SHORT,        /* xxx.W           */
    ADDR_MODE_ABS_LONG,         /* xxx.L           */
    ADDR_MODE_PC_DISP,          /* d16(PC)         */
    ADDR_MODE_PC_INDEX,         /* d8(PC,Xn)       */
    ADDR_MODE_IMMEDIATE,        /* #xxx            */
    ADDR_MODE_INVALID
} AddressingMode;

/* ── Resolved effective address ── */
typedef struct {
    AddressingMode mode;
    u32            address;    /* Computed memory address (if applicable) */
    int            reg;        /* Register number (if mode is reg direct) */
} EffectiveAddress;

/* ── Decode addressing mode from mode/reg fields ── */
AddressingMode addrDecodeMode(int mode, int reg);

/* ── Calculate effective address (may fetch extension words, advancing PC) ── */
EffectiveAddress addrCalcEA(Cpu *cpu, int mode, int reg, OperationSize sz);

/* ── Read a value through an effective address ── */
u32 addrRead(Cpu *cpu, EffectiveAddress *ea, OperationSize sz);

/* ── Write a value through an effective address ── */
void addrWrite(Cpu *cpu, EffectiveAddress *ea, u32 value, OperationSize sz);

/* ── Cycle cost for effective address calculation ── */
u32 addrCalcCycles(AddressingMode mode, OperationSize sz);

/* ── Predecrement / postincrement step for a register and size ──
 *  Note: A7 (SP) always moves by at least 2 to keep the stack word-aligned,
 *  even for byte operations. */
static inline u32 addrIncrement(int reg, OperationSize sz) {
    u32 step = sizeInBytes(sz);
    if (reg == 7 && step == 1) step = 2;  /* A7 byte ops use word-aligned SP */
    return step;
}

#endif /* M68K_ADDRESSING_H */
