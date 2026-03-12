/* ===========================================================================
 *  opcodes.h — Opcode table registration
 *
 *  Each instruction category registers its handlers into the global
 *  opcodeTable[65536].  The pattern-based registration iterates all
 *  valid opcode bit combinations for each instruction.
 * =========================================================================== */
#ifndef M68K_OPCODES_H
#define M68K_OPCODES_H

#include "cpu/decoder.h"

/* Master registration function — called by decoderInit() */
void opcodesRegisterAll(void);

/* ── Per-category registration functions ── */
void opcodesRegisterMove(void);        /* MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, etc. */
void opcodesRegisterArithmetic(void);  /* ADD, SUB, MUL, DIV, CMP, NEG, CLR, etc. */
void opcodesRegisterLogic(void);       /* AND, OR, EOR, NOT                       */
void opcodesRegisterShift(void);       /* ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR */
void opcodesRegisterBranch(void);      /* Bcc, BRA, BSR, DBcc, Scc, JMP, JSR, RTS, RTE */
void opcodesRegisterBcd(void);         /* ABCD, SBCD, NBCD                        */
void opcodesRegisterSystem(void);      /* TRAP, RTE, STOP, NOP, RESET, etc.       */
void opcodesRegisterBit(void);         /* BTST, BSET, BCLR, BCHG                  */

/* ── Helper to register an opcode entry ── */
static inline void opcodeRegister(u16 opcode, InstructionHandler handler,
                                  const char *mnemonic, u32 baseCycles) {
    opcodeTable[opcode].handler    = handler;
    opcodeTable[opcode].mnemonic   = mnemonic;
    opcodeTable[opcode].baseCycles = baseCycles;
}

#endif /* M68K_OPCODES_H */
