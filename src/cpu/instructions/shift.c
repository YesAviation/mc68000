/* ===========================================================================
 *  shift.c — ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR
 *
 *  Two forms:
 *    Register: shift Dn by count (immediate 1-8 or from Dn)
 *    Memory:   shift <ea> by 1 (word-sized only)
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── Common register shift handler ── */
static void shiftRegister(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int countReg = opDstReg(op);
    int reg      = opSrcReg(op);
    OperationSize sz = opSize76(op);
    int dir   = (op >> 8) & 1;     /* 0=right, 1=left */
    int type  = (op >> 3) & 3;     /* 00=AS, 01=LS, 10=ROX, 11=RO */
    bool useReg = (op >> 5) & 1;   /* 0=immediate count, 1=register count */

    int count;
    if (useReg) {
        count = cpu->d[countReg] & 63; /* Mod 64 */
    } else {
        count = countReg;
        if (count == 0) count = 8;
    }

    u32 val = cpuReadD(cpu, reg, sz);
    u32 result = 0;

    if (dir) { /* Left */
        switch (type) {
            case 0: result = aluAsl(cpu, val, count, sz); break;
            case 1: result = aluLsl(cpu, val, count, sz); break;
            case 2: result = aluRoxl(cpu, val, count, sz); break;
            case 3: result = aluRol(cpu, val, count, sz); break;
        }
    } else { /* Right */
        switch (type) {
            case 0: result = aluAsr(cpu, val, count, sz); break;
            case 1: result = aluLsr(cpu, val, count, sz); break;
            case 2: result = aluRoxr(cpu, val, count, sz); break;
            case 3: result = aluRor(cpu, val, count, sz); break;
        }
    }

    cpuWriteD(cpu, reg, result, sz);
    /* PRM: Shift register = 6+2n (B/W) or 8+2n (L) — already in timingShift() */
    cpuAddCycles(cpu, timingShift(count, sz, false));
}

/* ── Memory shift (always word, always count=1) ── */
static void shiftMemory(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int dir    = (op >> 8) & 1;
    int type   = (op >> 9) & 3;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    u32 val = addrRead(cpu, &ea, SIZE_WORD);
    u32 result = 0;

    if (dir) {
        switch (type) {
            case 0: result = aluAsl(cpu, val, 1, SIZE_WORD); break;
            case 1: result = aluLsl(cpu, val, 1, SIZE_WORD); break;
            case 2: result = aluRoxl(cpu, val, 1, SIZE_WORD); break;
            case 3: result = aluRol(cpu, val, 1, SIZE_WORD); break;
        }
    } else {
        switch (type) {
            case 0: result = aluAsr(cpu, val, 1, SIZE_WORD); break;
            case 1: result = aluLsr(cpu, val, 1, SIZE_WORD); break;
            case 2: result = aluRoxr(cpu, val, 1, SIZE_WORD); break;
            case 3: result = aluRor(cpu, val, 1, SIZE_WORD); break;
        }
    }

    addrWrite(cpu, &ea, result, SIZE_WORD);
    /* PRM: Shift memory = 8+ea (always count=1) */
    cpuAddCycles(cpu, 8 + addrCalcCycles(ea.mode, SIZE_WORD));
}

/* ── Registration ── */
void opcodesRegisterShift(void) {
    /* Register shifts: 1110 ccc d ss i tt rrr
     *  c = count/reg, d = direction, ss = size, i = imm/reg, tt = type, r = reg */
    for (int cnt = 0; cnt < 8; cnt++) {
        for (int dir = 0; dir < 2; dir++) {
            for (int sz = 0; sz < 3; sz++) {
                for (int ir = 0; ir < 2; ir++) {
                    for (int type = 0; type < 4; type++) {
                        for (int reg = 0; reg < 8; reg++) {
                            u16 opcode = 0xE000 |
                                         (cnt << 9) | (dir << 8) |
                                         (sz << 6) | (ir << 5) |
                                         (type << 3) | reg;
                            opcodeRegister(opcode, shiftRegister, "SHIFT", 6);
                        }
                    }
                }
            }
        }
    }

    /* Memory shifts: 1110 0tt d 11 mmm rrr */
    for (int type = 0; type < 4; type++) {
        for (int dir = 0; dir < 2; dir++) {
            for (int mode = 2; mode < 8; mode++) {
                for (int reg = 0; reg < 8; reg++) {
                    u16 opcode = 0xE0C0 |
                                 (type << 9) | (dir << 8) |
                                 (mode << 3) | reg;
                    opcodeRegister(opcode, shiftMemory, "SHIFT_MEM", 8);
                }
            }
        }
    }
}
