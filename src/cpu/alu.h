/* ===========================================================================
 *  alu.h — Arithmetic Logic Unit: operations and condition code computation
 *
 *  The MC68000 CCR flags:
 *    X (extend)   — Set the same as C for arithmetic; unaffected by logic ops
 *    N (negative) — Set if result MSB is set
 *    Z (zero)     — Set if result is zero
 *    V (overflow) — Set on signed overflow
 *    C (carry)    — Set on unsigned carry/borrow
 *
 *  The 16 branch/set conditions (Bcc, Scc, DBcc):
 *    T  (true)           0000    F  (false)          0001
 *    HI (high)           0010    LS (low or same)    0011
 *    CC (carry clear)    0100    CS (carry set)      0101
 *    NE (not equal)      0110    EQ (equal)          0111
 *    VC (overflow clear) 1000    VS (overflow set)   1001
 *    PL (plus)           1010    MI (minus)          1011
 *    GE (greater/equal)  1100    LT (less than)      1101
 *    GT (greater than)   1110    LE (less/equal)     1111
 * =========================================================================== */
#ifndef M68K_ALU_H
#define M68K_ALU_H

#include "cpu/cpu.h"

/* ── Condition codes (for Bcc/Scc/DBcc) ── */
typedef enum {
    COND_T  = 0,  COND_F  = 1,
    COND_HI = 2,  COND_LS = 3,
    COND_CC = 4,  COND_CS = 5,
    COND_NE = 6,  COND_EQ = 7,
    COND_VC = 8,  COND_VS = 9,
    COND_PL = 10, COND_MI = 11,
    COND_GE = 12, COND_LT = 13,
    COND_GT = 14, COND_LE = 15
} ConditionCode;

/* Evaluate a condition code against the current SR */
bool aluTestCondition(const Cpu *cpu, ConditionCode cc);

/* ── Arithmetic operations (set flags, return result) ── */
u32 aluAdd(Cpu *cpu, u32 src, u32 dst, OperationSize sz);
u32 aluAddX(Cpu *cpu, u32 src, u32 dst, OperationSize sz);  /* with extend */
u32 aluSub(Cpu *cpu, u32 src, u32 dst, OperationSize sz);
u32 aluSubX(Cpu *cpu, u32 src, u32 dst, OperationSize sz);  /* with extend */
u32 aluNeg(Cpu *cpu, u32 val, OperationSize sz);
u32 aluNegX(Cpu *cpu, u32 val, OperationSize sz);            /* with extend */

/* Compare: like SUB but only sets flags, doesn't return result */
void aluCmp(Cpu *cpu, u32 src, u32 dst, OperationSize sz);

/* ── Logical operations ── */
u32 aluAnd(Cpu *cpu, u32 src, u32 dst, OperationSize sz);
u32 aluOr(Cpu *cpu, u32 src, u32 dst, OperationSize sz);
u32 aluEor(Cpu *cpu, u32 src, u32 dst, OperationSize sz);
u32 aluNot(Cpu *cpu, u32 val, OperationSize sz);

/* ── Shift / rotate operations ── */
u32 aluAsl(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluAsr(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluLsl(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluLsr(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluRol(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluRor(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluRoxl(Cpu *cpu, u32 val, int count, OperationSize sz);
u32 aluRoxr(Cpu *cpu, u32 val, int count, OperationSize sz);

/* ── Multiplication / Division ── */
u32 aluMulu(Cpu *cpu, u16 src, u16 dst);   /* unsigned 16x16 -> 32 */
u32 aluMuls(Cpu *cpu, s16 src, s16 dst);   /* signed   16x16 -> 32 */
bool aluDivu(Cpu *cpu, u32 dividend, u16 divisor, u32 *result); /* unsigned */
bool aluDivs(Cpu *cpu, s32 dividend, s16 divisor, u32 *result); /* signed   */

/* ── BCD operations ── */
u8 aluAbcd(Cpu *cpu, u8 src, u8 dst);   /* Add BCD with extend */
u8 aluSbcd(Cpu *cpu, u8 src, u8 dst);   /* Subtract BCD with extend */
u8 aluNbcd(Cpu *cpu, u8 val);           /* Negate BCD with extend */

/* ── Flag-setting helpers ── */

/* Set N and Z flags from a result */
void aluSetNZ(Cpu *cpu, u32 result, OperationSize sz);

/* Set all logical-operation flags (N, Z set; V, C cleared) */
void aluSetLogicFlags(Cpu *cpu, u32 result, OperationSize sz);

/* Clear a value, setting flags appropriately (for CLR instruction) */
void aluSetClrFlags(Cpu *cpu);

/* Set flags for TST instruction (same as logic flags) */
void aluSetTstFlags(Cpu *cpu, u32 value, OperationSize sz);

#endif /* M68K_ALU_H */
