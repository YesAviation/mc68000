/* ===========================================================================
 *  arithmetic.c — ADD, ADDA, ADDI, ADDQ, ADDX, SUB, SUBA, SUBI, SUBQ, SUBX
 *                 MULS, MULU, DIVS, DIVU, NEG, NEGX, CLR, CMP, CMPA, CMPI,
 *                 CMPM, EXT, TST
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── ADD <ea>,Dn / ADD Dn,<ea> ── */
static void instrAdd(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg     = opDstReg(op);
    int eaMode  = opSrcMode(op);
    int eaReg   = opSrcReg(op);
    int opmode  = (op >> 6) & 7;

    OperationSize sz;
    bool toReg; /* true = <ea> + Dn -> Dn, false = Dn + <ea> -> <ea> */

    switch (opmode) {
        case 0: sz = SIZE_BYTE; toReg = true;  break;
        case 1: sz = SIZE_WORD; toReg = true;  break;
        case 2: sz = SIZE_LONG; toReg = true;  break;
        case 4: sz = SIZE_BYTE; toReg = false; break;
        case 5: sz = SIZE_WORD; toReg = false; break;
        case 6: sz = SIZE_LONG; toReg = false; break;
        default: return; /* ADDA handled separately */
    }

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);

    if (toReg) {
        u32 src = addrRead(cpu, &ea, sz);
        u32 dst = cpuReadD(cpu, reg, sz);
        u32 result = aluAdd(cpu, src, dst, sz);
        cpuWriteD(cpu, reg, result, sz);
        /* PRM: ADD <ea>,Dn: B/W=4+ea, L=6+ea (8+ea if ea is Dn/An/#imm) */
        if (sz == SIZE_LONG) {
            u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
            cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
        } else {
            cpuAddCycles(cpu, 4 + addrCalcCycles(ea.mode, sz));
        }
    } else {
        u32 src = cpuReadD(cpu, reg, sz);
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluAdd(cpu, src, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: ADD Dn,<ea>(mem): B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ADDA <ea>,An ── */
static void instrAdda(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg  = opDstReg(op);
    int eaMode  = opSrcMode(op);
    int eaReg   = opSrcReg(op);
    OperationSize sz = ((op >> 8) & 1) ? SIZE_LONG : SIZE_WORD;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 src = addrRead(cpu, &ea, sz);

    /* Word operations are sign-extended */
    if (sz == SIZE_WORD) src = (u32)signExtend16((u16)src);

    cpu->a[dstReg] += src;
    /* No flags affected */

    /* PRM: ADDA.W always 8+ea; ADDA.L = 6+ea (8+ea if Dn/An/imm) */
    if (sz == SIZE_WORD) {
        cpuAddCycles(cpu, 8 + addrCalcCycles(ea.mode, sz));
    } else {
        u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
        cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ADDI #imm,<ea> ── */
static void instrAddi(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm;
    if (sz == SIZE_LONG) {
        imm = instrFetchLong(cpu);
    } else {
        imm = instrFetchWord(cpu);
        if (sz == SIZE_BYTE) imm &= 0xFF;
    }

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluAdd(cpu, imm, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: ADDI Dn: B/W=8, L=16; mem: B/W=12+ea, L=20+ea */
    if (ea.mode == ADDR_MODE_DATA_REG) {
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 16 : 8);
    } else {
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 + addrCalcCycles(ea.mode, sz)
                                            : 12 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ADDQ #imm3,<ea> ── */
static void instrAddq(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int imm    = opDstReg(op);
    if (imm == 0) imm = 8;  /* 0 encodes as 8 */

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);

    if (ea.mode == ADDR_MODE_ADDR_REG) {
        /* ADDQ to An: no flags, full 32-bit add */
        cpu->a[ea.reg] += (u32)imm;
        cpuAddCycles(cpu, 8); /* PRM: ADDQ An = 8 for all sizes */
    } else if (ea.mode == ADDR_MODE_DATA_REG) {
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluAdd(cpu, (u32)imm, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 8 : 4); /* PRM: ADDQ Dn */
    } else {
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluAdd(cpu, (u32)imm, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: ADDQ mem: B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ADDX Dy,Dx / ADDX -(Ay),-(Ax) ── */
static void instrAddx(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int rx = opDstReg(op);
    int ry = opSrcReg(op);
    bool isMemory = (op & 0x08) != 0;

    if (isMemory) {
        u32 stepSrc = addrIncrement(ry, sz);
        u32 stepDst = addrIncrement(rx, sz);
        cpu->a[ry] -= stepSrc;
        cpu->a[rx] -= stepDst;

        u32 src, dst;
        switch (sz) {
            case SIZE_BYTE: src = busReadByte(cpu->bus, cpu->a[ry]); dst = busReadByte(cpu->bus, cpu->a[rx]); break;
            case SIZE_WORD: src = busReadWord(cpu->bus, cpu->a[ry]); dst = busReadWord(cpu->bus, cpu->a[rx]); break;
            case SIZE_LONG: src = busReadLong(cpu->bus, cpu->a[ry]); dst = busReadLong(cpu->bus, cpu->a[rx]); break;
        }

        u32 result = aluAddX(cpu, src, dst, sz);

        switch (sz) {
            case SIZE_BYTE: busWriteByte(cpu->bus, cpu->a[rx], (u8)result); break;
            case SIZE_WORD: busWriteWord(cpu->bus, cpu->a[rx], (u16)result); break;
            case SIZE_LONG: busWriteLong(cpu->bus, cpu->a[rx], result); break;
        }
    } else {
        u32 src = cpuReadD(cpu, ry, sz);
        u32 dst = cpuReadD(cpu, rx, sz);
        u32 result = aluAddX(cpu, src, dst, sz);
        cpuWriteD(cpu, rx, result, sz);
    }

    /* PRM: ADDX Dn: B/W=4, L=8; mem: B/W=18, L=30 */
    if (isMemory)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 30 : 18);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 8 : 4);
}

/* ── SUB <ea>,Dn / SUB Dn,<ea> ── */
static void instrSub(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg     = opDstReg(op);
    int eaMode  = opSrcMode(op);
    int eaReg   = opSrcReg(op);
    int opmode  = (op >> 6) & 7;

    OperationSize sz;
    bool toReg;

    switch (opmode) {
        case 0: sz = SIZE_BYTE; toReg = true;  break;
        case 1: sz = SIZE_WORD; toReg = true;  break;
        case 2: sz = SIZE_LONG; toReg = true;  break;
        case 4: sz = SIZE_BYTE; toReg = false; break;
        case 5: sz = SIZE_WORD; toReg = false; break;
        case 6: sz = SIZE_LONG; toReg = false; break;
        default: return;
    }

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);

    if (toReg) {
        u32 src = addrRead(cpu, &ea, sz);
        u32 dst = cpuReadD(cpu, reg, sz);
        u32 result = aluSub(cpu, src, dst, sz);
        cpuWriteD(cpu, reg, result, sz);
        /* PRM: SUB <ea>,Dn: B/W=4+ea, L=6+ea (8+ea if Dn/An/#imm) */
        if (sz == SIZE_LONG) {
            u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
            cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
        } else {
            cpuAddCycles(cpu, 4 + addrCalcCycles(ea.mode, sz));
        }
    } else {
        u32 src = cpuReadD(cpu, reg, sz);
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluSub(cpu, src, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: SUB Dn,<ea>(mem): B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── SUBA <ea>,An ── */
static void instrSuba(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    OperationSize sz = ((op >> 8) & 1) ? SIZE_LONG : SIZE_WORD;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 src = addrRead(cpu, &ea, sz);
    if (sz == SIZE_WORD) src = (u32)signExtend16((u16)src);

    cpu->a[dstReg] -= src;
    /* PRM: SUBA.W always 8+ea; SUBA.L = 6+ea (8+ea if Dn/An/imm) */
    if (sz == SIZE_WORD) {
        cpuAddCycles(cpu, 8 + addrCalcCycles(ea.mode, sz));
    } else {
        u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
        cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
    }
}

/* ── SUBI #imm,<ea> ── */
static void instrSubi(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm;
    if (sz == SIZE_LONG) {
        imm = instrFetchLong(cpu);
    } else {
        imm = instrFetchWord(cpu);
        if (sz == SIZE_BYTE) imm &= 0xFF;
    }

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluSub(cpu, imm, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: SUBI Dn: B/W=8, L=16; mem: B/W=12+ea, L=20+ea */
    if (ea.mode == ADDR_MODE_DATA_REG) {
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 16 : 8);
    } else {
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 + addrCalcCycles(ea.mode, sz)
                                            : 12 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── SUBQ #imm3,<ea> ── */
static void instrSubq(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int imm    = opDstReg(op);
    if (imm == 0) imm = 8;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);

    if (ea.mode == ADDR_MODE_ADDR_REG) {
        cpu->a[ea.reg] -= (u32)imm;
        cpuAddCycles(cpu, 8); /* PRM: SUBQ An = 8 for all sizes */
    } else if (ea.mode == ADDR_MODE_DATA_REG) {
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluSub(cpu, (u32)imm, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 8 : 4); /* PRM: SUBQ Dn */
    } else {
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluSub(cpu, (u32)imm, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: SUBQ mem: B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── SUBX ── */
static void instrSubx(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int rx = opDstReg(op);
    int ry = opSrcReg(op);
    bool isMemory = (op & 0x08) != 0;

    if (isMemory) {
        u32 stepSrc = addrIncrement(ry, sz);
        u32 stepDst = addrIncrement(rx, sz);
        cpu->a[ry] -= stepSrc;
        cpu->a[rx] -= stepDst;

        u32 src = 0, dst = 0;
        switch (sz) {
            case SIZE_BYTE: src = busReadByte(cpu->bus, cpu->a[ry]); dst = busReadByte(cpu->bus, cpu->a[rx]); break;
            case SIZE_WORD: src = busReadWord(cpu->bus, cpu->a[ry]); dst = busReadWord(cpu->bus, cpu->a[rx]); break;
            case SIZE_LONG: src = busReadLong(cpu->bus, cpu->a[ry]); dst = busReadLong(cpu->bus, cpu->a[rx]); break;
        }
        u32 result = aluSubX(cpu, src, dst, sz);
        switch (sz) {
            case SIZE_BYTE: busWriteByte(cpu->bus, cpu->a[rx], (u8)result); break;
            case SIZE_WORD: busWriteWord(cpu->bus, cpu->a[rx], (u16)result); break;
            case SIZE_LONG: busWriteLong(cpu->bus, cpu->a[rx], result); break;
        }
    } else {
        u32 src = cpuReadD(cpu, ry, sz);
        u32 dst = cpuReadD(cpu, rx, sz);
        u32 result = aluSubX(cpu, src, dst, sz);
        cpuWriteD(cpu, rx, result, sz);
    }

    /* PRM: SUBX Dn: B/W=4, L=8; mem: B/W=18, L=30 */
    if (isMemory)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 30 : 18);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 8 : 4);
}

/* ── MULU <ea>,Dn ── */
static void instrMulu(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    u16 src = (u16)addrRead(cpu, &ea, SIZE_WORD);
    u16 dst = (u16)cpu->d[dstReg];

    cpu->d[dstReg] = aluMulu(cpu, src, dst);
    cpuAddCycles(cpu, timingMulu(src) + addrCalcCycles(ea.mode, SIZE_WORD));
}

/* ── MULS <ea>,Dn ── */
static void instrMuls(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    s16 src = (s16)addrRead(cpu, &ea, SIZE_WORD);
    s16 dst = (s16)cpu->d[dstReg];

    cpu->d[dstReg] = aluMuls(cpu, src, dst);
    cpuAddCycles(cpu, timingMuls((u16)src) + addrCalcCycles(ea.mode, SIZE_WORD));
}

/* ── DIVU <ea>,Dn ── */
static void instrDivu(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    u16 src = (u16)addrRead(cpu, &ea, SIZE_WORD);
    u32 dst = cpu->d[dstReg];

    if (src == 0) {
        exceptionZeroDivide(cpu);
        return;
    }

    u32 result;
    aluDivu(cpu, dst, src, &result);
    cpu->d[dstReg] = result;
    cpuAddCycles(cpu, timingDivu(dst, src) + addrCalcCycles(ea.mode, SIZE_WORD));
}

/* ── DIVS <ea>,Dn ── */
static void instrDivs(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    s16 src = (s16)addrRead(cpu, &ea, SIZE_WORD);
    s32 dst = (s32)cpu->d[dstReg];

    if (src == 0) {
        exceptionZeroDivide(cpu);
        return;
    }

    u32 result;
    aluDivs(cpu, dst, src, &result);
    cpu->d[dstReg] = result;
    cpuAddCycles(cpu, timingDivs(dst, src) + addrCalcCycles(ea.mode, SIZE_WORD));
}

/* ── NEG <ea> ── */
static void instrNeg(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 val = addrRead(cpu, &ea, sz);
    u32 result = aluNeg(cpu, val, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: NEG Dn: B/W=4, L=6; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 : 4);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── NEGX <ea> ── */
static void instrNegx(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 val = addrRead(cpu, &ea, sz);
    u32 result = aluNegX(cpu, val, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: NEGX Dn: B/W=4, L=6; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 : 4);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── CLR <ea> ── */
static void instrClr(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    /* Note: 68000 reads before writing (quirk) but result is always 0 */
    addrRead(cpu, &ea, sz);
    addrWrite(cpu, &ea, 0, sz);
    aluSetClrFlags(cpu);

    /* PRM: CLR Dn: B/W=4, L=6; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 : 4);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── CMP <ea>,Dn ── */
static void instrCmp(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int reg    = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 src = addrRead(cpu, &ea, sz);
    u32 dst = cpuReadD(cpu, reg, sz);

    aluCmp(cpu, src, dst, sz);
    /* PRM: CMP <ea>,Dn: B/W=4+ea, L=6+ea */
    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 + addrCalcCycles(ea.mode, sz)
                                        : 4 + addrCalcCycles(ea.mode, sz));
}

/* ── CMPA <ea>,An ── */
static void instrCmpa(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dstReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    OperationSize sz = ((op >> 8) & 1) ? SIZE_LONG : SIZE_WORD;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 src = addrRead(cpu, &ea, sz);
    if (sz == SIZE_WORD) src = (u32)signExtend16((u16)src);

    aluCmp(cpu, src, cpu->a[dstReg], SIZE_LONG);
    /* PRM: CMPA = 6+ea always (performs 32-bit comparison) */
    cpuAddCycles(cpu, 6 + addrCalcCycles(ea.mode, sz));
}

/* ── CMPI #imm,<ea> ── */
static void instrCmpi(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm;
    if (sz == SIZE_LONG) {
        imm = instrFetchLong(cpu);
    } else {
        imm = instrFetchWord(cpu);
        if (sz == SIZE_BYTE) imm &= 0xFF;
    }

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);

    aluCmp(cpu, imm, dst, sz);
    /* PRM: CMPI Dn: B/W=8, L=14; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 14 : 8);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── CMPM (An)+,(An)+ ── */
static void instrCmpm(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int ax = opDstReg(op);
    int ay = opSrcReg(op);

    u32 step = addrIncrement(ay, sz);

    u32 src = 0, dst = 0;
    switch (sz) {
        case SIZE_BYTE: src = busReadByte(cpu->bus, cpu->a[ay]); break;
        case SIZE_WORD: src = busReadWord(cpu->bus, cpu->a[ay]); break;
        case SIZE_LONG: src = busReadLong(cpu->bus, cpu->a[ay]); break;
    }
    cpu->a[ay] += step;

    step = addrIncrement(ax, sz);
    switch (sz) {
        case SIZE_BYTE: dst = busReadByte(cpu->bus, cpu->a[ax]); break;
        case SIZE_WORD: dst = busReadWord(cpu->bus, cpu->a[ax]); break;
        case SIZE_LONG: dst = busReadLong(cpu->bus, cpu->a[ax]); break;
    }
    cpu->a[ax] += step;

    aluCmp(cpu, src, dst, sz);
    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 : 12);
}

/* ── EXT Dn (sign-extend byte->word or word->long) ── */
static void instrExt(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg = opSrcReg(op);
    int opmode = (op >> 6) & 7;

    if (opmode == 2) {
        /* Byte to word */
        s16 val = signExtend8((u8)cpu->d[reg]);
        cpuWriteD(cpu, reg, (u32)(u16)val, SIZE_WORD);
        aluSetLogicFlags(cpu, (u32)(u16)val, SIZE_WORD);
    } else if (opmode == 3) {
        /* Word to long */
        s32 val = signExtend16((u16)cpu->d[reg]);
        cpu->d[reg] = (u32)val;
        aluSetLogicFlags(cpu, (u32)val, SIZE_LONG);
    }

    cpuAddCycles(cpu, 4);
}

/* ── TST <ea> ── */
static void instrTst(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 val = addrRead(cpu, &ea, sz);

    aluSetTstFlags(cpu, val, sz);
    /* PRM: TST Dn=4; mem=4+ea */
    cpuAddCycles(cpu, 4 + addrCalcCycles(ea.mode, sz));
}

/* ── Opcode registration ── */
void opcodesRegisterArithmetic(void) {
    /* ADD: 1101 rrr ooo mmm sss */
    for (int reg = 0; reg < 8; reg++) {
        for (int opmode = 0; opmode < 7; opmode++) {
            if (opmode == 3) continue; /* ADDA.W */
            for (int eaMode = 0; eaMode < 8; eaMode++) {
                for (int eaReg = 0; eaReg < 8; eaReg++) {
                    u16 opcode = 0xD000 | (reg << 9) | (opmode << 6) | (eaMode << 3) | eaReg;
                    if (opmode == 3 || opmode == 7) {
                        opcodeRegister(opcode, instrAdda, "ADDA", 8);
                    } else {
                        opcodeRegister(opcode, instrAdd, "ADD", 4);
                    }
                }
            }
        }
        /* ADDA.W / ADDA.L */
        for (int eaMode = 0; eaMode < 8; eaMode++) {
            for (int eaReg = 0; eaReg < 8; eaReg++) {
                opcodeRegister(0xD000 | (reg << 9) | (3 << 6) | (eaMode << 3) | eaReg, instrAdda, "ADDA.W", 8);
                opcodeRegister(0xD000 | (reg << 9) | (7 << 6) | (eaMode << 3) | eaReg, instrAdda, "ADDA.L", 8);
            }
        }
    }

    /* SUB: 1001 rrr ooo mmm sss (same pattern as ADD) */
    for (int reg = 0; reg < 8; reg++) {
        for (int opmode = 0; opmode < 7; opmode++) {
            if (opmode == 3) continue;
            for (int eaMode = 0; eaMode < 8; eaMode++) {
                for (int eaReg = 0; eaReg < 8; eaReg++) {
                    u16 opcode = 0x9000 | (reg << 9) | (opmode << 6) | (eaMode << 3) | eaReg;
                    opcodeRegister(opcode, instrSub, "SUB", 4);
                }
            }
        }
        for (int eaMode = 0; eaMode < 8; eaMode++) {
            for (int eaReg = 0; eaReg < 8; eaReg++) {
                opcodeRegister(0x9000 | (reg << 9) | (3 << 6) | (eaMode << 3) | eaReg, instrSuba, "SUBA.W", 8);
                opcodeRegister(0x9000 | (reg << 9) | (7 << 6) | (eaMode << 3) | eaReg, instrSuba, "SUBA.L", 8);
            }
        }
    }

    /* ADDI: 0000 0110 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0600 | (sz << 6) | (mode << 3) | reg, instrAddi, "ADDI", 8);
            }
        }
    }

    /* SUBI: 0000 0100 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0400 | (sz << 6) | (mode << 3) | reg, instrSubi, "SUBI", 8);
            }
        }
    }

    /* ADDQ: 0101 ddd0 ss mmm rrr */
    for (int data = 0; data < 8; data++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int mode = 0; mode < 8; mode++) {
                for (int reg = 0; reg < 8; reg++) {
                    opcodeRegister(0x5000 | (data << 9) | (sz << 6) | (mode << 3) | reg,
                                   instrAddq, "ADDQ", 4);
                }
            }
        }
    }

    /* SUBQ: 0101 ddd1 ss mmm rrr */
    for (int data = 0; data < 8; data++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int mode = 0; mode < 8; mode++) {
                for (int reg = 0; reg < 8; reg++) {
                    opcodeRegister(0x5100 | (data << 9) | (sz << 6) | (mode << 3) | reg,
                                   instrSubq, "SUBQ", 4);
                }
            }
        }
    }

    /* MULU: 1100 rrr0 11mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0xC0C0 | (reg << 9) | (mode << 3) | ereg, instrMulu, "MULU", 38);
            }
        }
    }

    /* MULS: 1100 rrr1 11mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0xC1C0 | (reg << 9) | (mode << 3) | ereg, instrMuls, "MULS", 38);
            }
        }
    }

    /* DIVU: 1000 rrr0 11mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0x80C0 | (reg << 9) | (mode << 3) | ereg, instrDivu, "DIVU", 108);
            }
        }
    }

    /* DIVS: 1000 rrr1 11mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0x81C0 | (reg << 9) | (mode << 3) | ereg, instrDivs, "DIVS", 140);
            }
        }
    }

    /* NEG: 0100 0100 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x4400 | (sz << 6) | (mode << 3) | reg, instrNeg, "NEG", 4);
            }
        }
    }

    /* NEGX: 0100 0000 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x4000 | (sz << 6) | (mode << 3) | reg, instrNegx, "NEGX", 4);
            }
        }
    }

    /* CLR: 0100 0010 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x4200 | (sz << 6) | (mode << 3) | reg, instrClr, "CLR", 4);
            }
        }
    }

    /* CMP: 1011 rrr0 ss mmm RRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int mode = 0; mode < 8; mode++) {
                for (int ereg = 0; ereg < 8; ereg++) {
                    opcodeRegister(0xB000 | (reg << 9) | (sz << 6) | (mode << 3) | ereg, instrCmp, "CMP", 4);
                }
            }
        }
    }

    /* CMPA: 1011 rrr0 11mm mRRR / 1011 rrr1 11mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0xB0C0 | (reg << 9) | (mode << 3) | ereg, instrCmpa, "CMPA.W", 6);
                opcodeRegister(0xB1C0 | (reg << 9) | (mode << 3) | ereg, instrCmpa, "CMPA.L", 6);
            }
        }
    }

    /* CMPI: 0000 1100 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0C00 | (sz << 6) | (mode << 3) | reg, instrCmpi, "CMPI", 8);
            }
        }
    }

    /* CMPM: 1011 xxx1 ss00 1yyy */
    for (int ax = 0; ax < 8; ax++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int ay = 0; ay < 8; ay++) {
                opcodeRegister(0xB108 | (ax << 9) | (sz << 6) | ay, instrCmpm, "CMPM", 12);
            }
        }
    }

    /* EXT: 0100 100o oo00 0rrr (opmode 010 = byte->word, 011 = word->long) */
    for (int reg = 0; reg < 8; reg++) {
        opcodeRegister(0x4880 | reg, instrExt, "EXT.W", 4);
        opcodeRegister(0x48C0 | reg, instrExt, "EXT.L", 4);
    }

    /* TST: 0100 1010 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x4A00 | (sz << 6) | (mode << 3) | reg, instrTst, "TST", 4);
            }
        }
    }

    /* ADDX: 1101 xxx1 ss00 ryyy (reg) / 1101 xxx1 ss00 1yyy (mem) */
    for (int rx = 0; rx < 8; rx++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int ry = 0; ry < 8; ry++) {
                opcodeRegister(0xD100 | (rx << 9) | (sz << 6) | ry, instrAddx, "ADDX", 4);
                opcodeRegister(0xD108 | (rx << 9) | (sz << 6) | ry, instrAddx, "ADDX", 4);
            }
        }
    }

    /* SUBX: 1001 xxx1 ss00 ryyy / 1001 xxx1 ss00 1yyy */
    for (int rx = 0; rx < 8; rx++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int ry = 0; ry < 8; ry++) {
                opcodeRegister(0x9100 | (rx << 9) | (sz << 6) | ry, instrSubx, "SUBX", 4);
                opcodeRegister(0x9108 | (rx << 9) | (sz << 6) | ry, instrSubx, "SUBX", 4);
            }
        }
    }
}
