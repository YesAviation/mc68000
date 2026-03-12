/* ===========================================================================
 *  decoder.h — Instruction decoder (64K opcode lookup table)
 *
 *  The MC68000 uses a 16-bit opcode word.  We build a flat 65536-entry
 *  table at startup so every instruction dispatches in O(1).
 *
 *  Bits 15-12 of the opcode select the instruction "line":
 *    0000 = Bit/MOVEP/Immediate    0001 = MOVE.B
 *    0010 = MOVE.L/MOVEA.L         0011 = MOVE.W/MOVEA.W
 *    0100 = Miscellaneous           0101 = ADDQ/SUBQ/Scc/DBcc
 *    0110 = Bcc/BSR/BRA            0111 = MOVEQ
 *    1000 = OR/DIV/SBCD            1001 = SUB/SUBA/SUBX
 *    1010 = Line-A (unassigned)    1011 = CMP/EOR/CMPM/CMPA
 *    1100 = AND/MUL/ABCD/EXG       1101 = ADD/ADDA/ADDX
 *    1110 = Shift/Rotate            1111 = Line-F (coprocessor)
 * =========================================================================== */
#ifndef M68K_DECODER_H
#define M68K_DECODER_H

#include "cpu/cpu.h"

/* ── Instruction handler function pointer ── */
typedef void (*InstructionHandler)(Cpu *cpu);

/* ── Opcode table entry ── */
typedef struct {
    InstructionHandler handler;    /* Execution function          */
    const char        *mnemonic;   /* Human-readable name         */
    u32                baseCycles; /* Base cycle count            */
} OpcodeEntry;

/* ── The 64K opcode table ── */
extern OpcodeEntry opcodeTable[65536];

/* Build the opcode table (call once at startup) */
void decoderInit(void);

/* Decode and execute a single opcode */
void decoderExecute(Cpu *cpu, u16 opcode);

/* ── Helpers for decoding common opcode fields ── */

/* Extract bits [hi:lo] from a 16-bit opcode */
static inline u16 opBits(u16 opcode, int hi, int lo) {
    return (opcode >> lo) & ((1 << (hi - lo + 1)) - 1);
}

/* Source register (bits 2-0) */
static inline int opSrcReg(u16 opcode) { return opcode & 7; }

/* Source mode (bits 5-3) */
static inline int opSrcMode(u16 opcode) { return (opcode >> 3) & 7; }

/* Destination register (bits 11-9) */
static inline int opDstReg(u16 opcode) { return (opcode >> 9) & 7; }

/* Destination mode (bits 8-6) */
static inline int opDstMode(u16 opcode) { return (opcode >> 6) & 7; }

/* Operation size from bits 7-6 (common encoding) */
static inline OperationSize opSize76(u16 opcode) {
    return (OperationSize)((opcode >> 6) & 3);
}

/* Operation size from bits 13-12 (MOVE encoding) */
static inline OperationSize opSizeMove(u16 opcode) {
    /* MOVE encoding: 01=byte, 11=word, 10=long */
    static const OperationSize table[] = {
        SIZE_BYTE, /* 0b00 = invalid, but map to byte */
        SIZE_BYTE, /* 0b01 */
        SIZE_LONG, /* 0b10 */
        SIZE_WORD  /* 0b11 */
    };
    return table[(opcode >> 12) & 3];
}

#endif /* M68K_DECODER_H */
