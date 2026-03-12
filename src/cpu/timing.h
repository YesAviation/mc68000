/* ===========================================================================
 *  timing.h — Cycle timing tables for the MC68000
 *
 *  All timings from the M68000 Programmer's Reference Manual.
 *  Cycle counts include memory accesses (4 cycles per bus read/write).
 * =========================================================================== */
#ifndef M68K_TIMING_H
#define M68K_TIMING_H

#include "common/types.h"

/* ── Effective address calculation times ──
 *  Index by [addressing_mode][SIZE_BYTE/WORD or SIZE_LONG]
 *  Values represent cycles added by the EA calculation. */
extern const u32 timingEA[12][2];

/* ── Move instruction timing ──
 *  Indexed by [source_mode][dest_mode] for byte/word and long */
extern const u32 timingMoveBW[12][9];
extern const u32 timingMoveL[12][9];

/* ── Standard instruction timing by category ── */
u32 timingStandard(u16 opcode, OperationSize sz);

/* ── Shift/rotate timing (depends on shift count) ── */
u32 timingShift(int shiftCount, OperationSize sz, bool isMemory);

/* ── Multiply/divide timing ── */
u32 timingMulu(u16 src);   /* 38 + 2n cycles (n = number of 1-bits in src) */
u32 timingMuls(u16 src);   /* 38 + 2n cycles (more complex for signed)     */
u32 timingDivu(u32 dividend, u16 divisor);
u32 timingDivs(s32 dividend, s16 divisor);

#endif /* M68K_TIMING_H */
