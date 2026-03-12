/* ===========================================================================
 *  logic.c — AND, ANDI, OR, ORI, EOR, EORI, NOT
 *            Plus ANDI/ORI/EORI to CCR and SR
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── AND <ea>,Dn / AND Dn,<ea> ── */
static void instrAnd(Cpu *cpu) {
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
        u32 result = aluAnd(cpu, src, dst, sz);
        cpuWriteD(cpu, reg, result, sz);
        /* PRM: AND <ea>,Dn: B/W=4+ea, L=6+ea (8+ea if Dn/imm) */
        if (sz == SIZE_LONG) {
            u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
            cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
        } else {
            cpuAddCycles(cpu, 4 + addrCalcCycles(ea.mode, sz));
        }
    } else {
        u32 src = cpuReadD(cpu, reg, sz);
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluAnd(cpu, src, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: AND Dn,<ea>: B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ANDI #imm,<ea> ── */
static void instrAndi(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm = (sz == SIZE_LONG) ? instrFetchLong(cpu) : instrFetchWord(cpu);
    if (sz == SIZE_BYTE) imm &= 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluAnd(cpu, imm, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: ANDI Dn: B/W=8, L=16; mem: B/W=12+ea, L=20+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 16 : 8);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 + addrCalcCycles(ea.mode, sz)
                                            : 12 + addrCalcCycles(ea.mode, sz));
}

/* ── ANDI to CCR ── */
static void instrAndiCcr(Cpu *cpu) {
    u16 imm = instrFetchWord(cpu);
    cpu->sr = (cpu->sr & 0xFF00) | (cpu->sr & (imm & 0xFF));
    cpuAddCycles(cpu, 20);
}

/* ── ANDI to SR (privileged) ── */
static void instrAndiSr(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }
    u16 imm = instrFetchWord(cpu);
    u16 newSR = cpu->sr & imm;
    /* Check for mode change */
    bool wasSuper = cpuIsSupervisor(cpu);
    cpu->sr = newSR;
    if (wasSuper && !(newSR & SR_S)) {
        cpuSetSupervisor(cpu, false);
    }
    cpuAddCycles(cpu, 20);
}

/* ── OR <ea>,Dn / OR Dn,<ea> ── */
static void instrOr(Cpu *cpu) {
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
        u32 result = aluOr(cpu, src, dst, sz);
        cpuWriteD(cpu, reg, result, sz);
        /* PRM: OR <ea>,Dn: B/W=4+ea, L=6+ea (8+ea if Dn/imm) */
        if (sz == SIZE_LONG) {
            u32 base = (ea.mode <= ADDR_MODE_ADDR_REG || ea.mode == ADDR_MODE_IMMEDIATE) ? 8 : 6;
            cpuAddCycles(cpu, base + addrCalcCycles(ea.mode, sz));
        } else {
            cpuAddCycles(cpu, 4 + addrCalcCycles(ea.mode, sz));
        }
    } else {
        u32 src = cpuReadD(cpu, reg, sz);
        u32 dst = addrRead(cpu, &ea, sz);
        u32 result = aluOr(cpu, src, dst, sz);
        addrWrite(cpu, &ea, result, sz);
        /* PRM: OR Dn,<ea>: B/W=8+ea, L=12+ea */
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
    }
}

/* ── ORI #imm,<ea> ── */
static void instrOri(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm = (sz == SIZE_LONG) ? instrFetchLong(cpu) : instrFetchWord(cpu);
    if (sz == SIZE_BYTE) imm &= 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluOr(cpu, imm, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: ORI Dn: B/W=8, L=16; mem: B/W=12+ea, L=20+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 16 : 8);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 + addrCalcCycles(ea.mode, sz)
                                            : 12 + addrCalcCycles(ea.mode, sz));
}

/* ── ORI to CCR ── */
static void instrOriCcr(Cpu *cpu) {
    u16 imm = instrFetchWord(cpu);
    cpu->sr = (cpu->sr & 0xFF00) | (cpu->sr | (imm & 0x1F));
    cpuAddCycles(cpu, 20);
}

/* ── ORI to SR (privileged) ── */
static void instrOriSr(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }
    u16 imm = instrFetchWord(cpu);
    cpu->sr |= imm;
    cpuAddCycles(cpu, 20);
}

/* ── EOR Dn,<ea> ── */
static void instrEor(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg     = opDstReg(op);
    int eaMode  = opSrcMode(op);
    int eaReg   = opSrcReg(op);
    OperationSize sz = opSize76(op);

    u32 src = cpuReadD(cpu, reg, sz);
    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluEor(cpu, src, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: EOR Dn: B/W=4, L=8; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 8 : 4);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── EORI #imm,<ea> ── */
static void instrEori(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    u32 imm = (sz == SIZE_LONG) ? instrFetchLong(cpu) : instrFetchWord(cpu);
    if (sz == SIZE_BYTE) imm &= 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 dst = addrRead(cpu, &ea, sz);
    u32 result = aluEor(cpu, imm, dst, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: EORI Dn: B/W=8, L=16; mem: B/W=12+ea, L=20+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 16 : 8);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 20 + addrCalcCycles(ea.mode, sz)
                                            : 12 + addrCalcCycles(ea.mode, sz));
}

/* ── EORI to CCR ── */
static void instrEoriCcr(Cpu *cpu) {
    u16 imm = instrFetchWord(cpu);
    cpu->sr ^= (imm & 0x1F);
    cpuAddCycles(cpu, 20);
}

/* ── EORI to SR (privileged) ── */
static void instrEoriSr(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }
    u16 imm = instrFetchWord(cpu);
    bool wasSuper = cpuIsSupervisor(cpu);
    cpu->sr ^= imm;
    if (wasSuper && !cpuIsSupervisor(cpu)) {
        cpuSetSupervisor(cpu, false);
    }
    cpuAddCycles(cpu, 20);
}

/* ── NOT <ea> ── */
static void instrNot(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSize76(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
    u32 val = addrRead(cpu, &ea, sz);
    u32 result = aluNot(cpu, val, sz);
    addrWrite(cpu, &ea, result, sz);

    /* PRM: NOT Dn: B/W=4, L=6; mem: B/W=8+ea, L=12+ea */
    if (ea.mode == ADDR_MODE_DATA_REG)
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 : 4);
    else
        cpuAddCycles(cpu, (sz == SIZE_LONG) ? 12 + addrCalcCycles(ea.mode, sz)
                                            : 8 + addrCalcCycles(ea.mode, sz));
}

/* ── Registration ── */
void opcodesRegisterLogic(void) {
    /* AND: 1100 rrr ooo mmm sss */
    for (int reg = 0; reg < 8; reg++) {
        for (int opmode = 0; opmode < 7; opmode++) {
            if (opmode == 3 || opmode == 7) continue; /* MULU/MULS */
            for (int mode = 0; mode < 8; mode++) {
                for (int ereg = 0; ereg < 8; ereg++) {
                    opcodeRegister(0xC000 | (reg << 9) | (opmode << 6) | (mode << 3) | ereg,
                                   instrAnd, "AND", 4);
                }
            }
        }
    }

    /* OR: 1000 rrr ooo mmm sss */
    for (int reg = 0; reg < 8; reg++) {
        for (int opmode = 0; opmode < 7; opmode++) {
            if (opmode == 3 || opmode == 7) continue; /* DIVU/DIVS */
            for (int mode = 0; mode < 8; mode++) {
                for (int ereg = 0; ereg < 8; ereg++) {
                    opcodeRegister(0x8000 | (reg << 9) | (opmode << 6) | (mode << 3) | ereg,
                                   instrOr, "OR", 4);
                }
            }
        }
    }

    /* EOR: 1011 rrr1 ss mmm rrr (note: EOR is always Dn to EA) */
    for (int reg = 0; reg < 8; reg++) {
        for (int sz = 0; sz < 3; sz++) {
            for (int mode = 0; mode < 8; mode++) {
                for (int ereg = 0; ereg < 8; ereg++) {
                    opcodeRegister(0xB100 | (reg << 9) | (sz << 6) | (mode << 3) | ereg,
                                   instrEor, "EOR", 4);
                }
            }
        }
    }

    /* ANDI: 0000 0010 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0200 | (sz << 6) | (mode << 3) | reg, instrAndi, "ANDI", 8);
            }
        }
    }

    /* ORI: 0000 0000 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0000 | (sz << 6) | (mode << 3) | reg, instrOri, "ORI", 8);
            }
        }
    }

    /* EORI: 0000 1010 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x0A00 | (sz << 6) | (mode << 3) | reg, instrEori, "EORI", 8);
            }
        }
    }

    /* NOT: 0100 0110 ss mmm rrr */
    for (int sz = 0; sz < 3; sz++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                opcodeRegister(0x4600 | (sz << 6) | (mode << 3) | reg, instrNot, "NOT", 4);
            }
        }
    }

    /* ANDI/ORI/EORI to CCR and SR (special cases) */
    opcodeRegister(0x023C, instrAndiCcr, "ANDI_CCR", 20);
    opcodeRegister(0x027C, instrAndiSr,  "ANDI_SR",  20);
    opcodeRegister(0x003C, instrOriCcr,  "ORI_CCR",  20);
    opcodeRegister(0x007C, instrOriSr,   "ORI_SR",   20);
    opcodeRegister(0x0A3C, instrEoriCcr, "EORI_CCR", 20);
    opcodeRegister(0x0A7C, instrEoriSr,  "EORI_SR",  20);
}
