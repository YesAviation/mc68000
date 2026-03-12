/* ===========================================================================
 *  timing.c — Cycle timing tables for the MC68000
 *
 *  All values from the M68000 Programmer's Reference Manual.
 * =========================================================================== */
#include "cpu/timing.h"

/* ── EA calculation times: [mode][0=byte/word, 1=long] ── */
const u32 timingEA[12][2] = {
    /* DATA_REG         */ { 0,  0 },
    /* ADDR_REG         */ { 0,  0 },
    /* ADDR_IND         */ { 4,  8 },
    /* ADDR_IND_POSTINC */ { 4,  8 },
    /* ADDR_IND_PREDEC  */ { 6,  10 },
    /* ADDR_IND_DISP    */ { 8,  12 },
    /* ADDR_IND_INDEX   */ { 10, 14 },
    /* ABS_SHORT        */ { 8,  12 },
    /* ABS_LONG         */ { 12, 16 },
    /* PC_DISP          */ { 8,  12 },
    /* PC_INDEX         */ { 10, 14 },
    /* IMMEDIATE        */ { 4,  8 },
};

/* ── MOVE byte/word timing: [src_mode][dst_mode] ── */
const u32 timingMoveBW[12][9] = {
    /*                    Dn  An  (An) (An)+  -(An)  d(An) d(An,Xn)  xxx.W  xxx.L */
    /* Dn            */ {  4,  4,   8,    8,     8,    12,      14,    12,    16 },
    /* An            */ {  4,  4,   8,    8,     8,    12,      14,    12,    16 },
    /* (An)          */ {  8,  8,  12,   12,    12,    16,      18,    16,    20 },
    /* (An)+         */ {  8,  8,  12,   12,    12,    16,      18,    16,    20 },
    /* -(An)         */ { 10, 10,  14,   14,    14,    18,      20,    18,    22 },
    /* d16(An)       */ { 12, 12,  16,   16,    16,    20,      22,    20,    24 },
    /* d8(An,Xn)     */ { 14, 14,  18,   18,    18,    22,      24,    22,    26 },
    /* xxx.W         */ { 12, 12,  16,   16,    16,    20,      22,    20,    24 },
    /* xxx.L         */ { 16, 16,  20,   20,    20,    24,      26,    24,    28 },
    /* d16(PC)       */ { 12, 12,  16,   16,    16,    20,      22,    20,    24 },
    /* d8(PC,Xn)     */ { 14, 14,  18,   18,    18,    22,      24,    22,    26 },
    /* #imm          */ {  8,  8,  12,   12,    12,    16,      18,    16,    20 },
};

/* ── MOVE long timing: [src_mode][dst_mode] ── */
const u32 timingMoveL[12][9] = {
    /*                    Dn  An  (An) (An)+  -(An)  d(An) d(An,Xn)  xxx.W  xxx.L */
    /* Dn            */ {  4,  4,  12,   12,    12,    16,      18,    16,    20 },
    /* An            */ {  4,  4,  12,   12,    12,    16,      18,    16,    20 },
    /* (An)          */ { 12, 12,  20,   20,    20,    24,      26,    24,    28 },
    /* (An)+         */ { 12, 12,  20,   20,    20,    24,      26,    24,    28 },
    /* -(An)         */ { 14, 14,  22,   22,    22,    26,      28,    26,    30 },
    /* d16(An)       */ { 16, 16,  24,   24,    24,    28,      30,    28,    32 },
    /* d8(An,Xn)     */ { 18, 18,  26,   26,    26,    30,      32,    30,    34 },
    /* xxx.W         */ { 16, 16,  24,   24,    24,    28,      30,    28,    32 },
    /* xxx.L         */ { 20, 20,  28,   28,    28,    32,      34,    32,    36 },
    /* d16(PC)       */ { 16, 16,  24,   24,    24,    28,      30,    28,    32 },
    /* d8(PC,Xn)     */ { 18, 18,  26,   26,    26,    30,      32,    30,    34 },
    /* #imm          */ { 12, 12,  20,   20,    20,    24,      26,    24,    28 },
};

/* ── Standard instruction timing ── */
u32 timingStandard(u16 opcode, OperationSize sz) {
    (void)opcode;
    (void)sz;
    /* Each instruction handler now computes its own cycle timing directly
     * from the M68000 PRM tables (base + EA calculation cost).
     * This function is no longer used as a fallback. */
    return 4;
}

/* ── Shift/rotate timing ── */
u32 timingShift(int shiftCount, OperationSize sz, bool isMemory) {
    if (isMemory) {
        return 8; /* Memory shifts are always count=1, cost 8 cycles */
    }
    u32 base = (sz == SIZE_LONG) ? 8 : 6;
    return base + 2 * (u32)shiftCount;
}

/* ── Multiply timing ── */
u32 timingMulu(u16 src) {
    /* 38 + 2n cycles, n = number of set bits in source */
    int n = 0;
    u16 v = src;
    while (v) { n += v & 1; v >>= 1; }
    return 38 + 2 * (u32)n;
}

u32 timingMuls(u16 src) {
    /* Roughly 38 + 2n, where n relates to bit transitions */
    /* Simplified: count number of 01 or 10 transitions */
    int n = 0;
    u16 v = src;
    for (int i = 0; i < 15; i++) {
        if (((v >> i) & 1) != ((v >> (i + 1)) & 1)) n++;
    }
    return 38 + 2 * (u32)n;
}

u32 timingDivu(u32 dividend, u16 divisor) {
    (void)dividend;
    (void)divisor;
    /* Best case 76, worst case 140. Use average approximation. */
    return 108;
}

u32 timingDivs(s32 dividend, s16 divisor) {
    (void)dividend;
    (void)divisor;
    /* Best case 120, worst case 158. Use average approximation. */
    return 140;
}
