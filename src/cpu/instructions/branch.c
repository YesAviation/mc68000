/* ===========================================================================
 *  branch.c — Bcc, BRA, BSR, DBcc, Scc, JMP, JSR, RTS, RTE, RTR
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── Bcc (conditional branch, includes BRA=always and BSR=subroutine) ──
 *  Opcode: 0110 cccc dddddddd
 *  cc = condition (0000=BRA, 0001=BSR, 0010-1111=Bcc)
 *  d  = 8-bit displacement (0 = 16-bit ext word follows) */
static void instrBcc(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int cc = (op >> 8) & 0xF;
    s32 disp;

    s8 disp8 = (s8)(op & 0xFF);
    if (disp8 == 0) {
        /* 16-bit displacement follows */
        disp = signExtend16(instrFetchWord(cpu));
    } else {
        disp = (s32)disp8;
    }

    if (cc == 1) {
        /* BSR: push return address, then branch */
        cpu->a[7] -= 4;
        busWriteLong(cpu->bus, cpu->a[7] & ADDR_MASK, cpu->pc);
        cpu->pc = (u32)((s32)cpu->currentPC + 2 + disp) & ADDR_MASK;
        cpuAddCycles(cpu, 18);
        return;
    }

    bool condition;
    if (cc == 0) {
        condition = true;  /* BRA: always taken */
    } else {
        condition = aluTestCondition(cpu, (ConditionCode)cc);
    }

    if (condition) {
        cpu->pc = (u32)((s32)cpu->currentPC + 2 + disp) & ADDR_MASK;
        cpuAddCycles(cpu, 10);
    } else {
        cpuAddCycles(cpu, (disp8 == 0) ? 12 : 8);
    }
}

/* ── DBcc Dn,label (decrement and branch) ──
 *  Opcode: 0101 cccc 1100 1rrr
 *  If condition is FALSE: Dn = Dn - 1; if Dn != -1, branch.
 *  If condition is TRUE: no branch, no decrement. */
static void instrDbcc(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int cc  = (op >> 8) & 0xF;
    int reg = opSrcReg(op);

    s16 disp = (s16)instrFetchWord(cpu);

    bool condition = aluTestCondition(cpu, (ConditionCode)cc);

    if (!condition) {
        /* Decrement lower word of Dn */
        u16 counter = (u16)(cpu->d[reg] & 0xFFFF);
        counter--;
        cpuWriteD(cpu, reg, counter, SIZE_WORD);

        if (counter != 0xFFFF) {
            /* Branch */
            cpu->pc = (u32)((s32)(cpu->pc - 2) + disp) & ADDR_MASK;
            cpuAddCycles(cpu, 10);
        } else {
            /* Counter expired */
            cpuAddCycles(cpu, 14);
        }
    } else {
        /* Condition true: fall through */
        cpuAddCycles(cpu, 12);
    }
}

/* ── Scc <ea> (set byte based on condition) ──
 *  Opcode: 0101 cccc 11mm mrrr */
static void instrScc(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int cc     = (op >> 8) & 0xF;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    bool condition = aluTestCondition(cpu, (ConditionCode)cc);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    addrWrite(cpu, &ea, condition ? 0xFF : 0x00, SIZE_BYTE);

    if (ea.mode == ADDR_MODE_DATA_REG) {
        cpuAddCycles(cpu, condition ? 6 : 4);
    } else {
        cpuAddCycles(cpu, 8);
    }
}

/* ── JMP <ea> ── */
static void instrJmp(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_LONG);
    cpu->pc = ea.address & ADDR_MASK;

    /* PRM: JMP (An)=8, d(An)=10, d(An,Xi)=14, abs.W=10, abs.L=12, d(PC)=10, d(PC,Xi)=14 */
    switch (ea.mode) {
        case ADDR_MODE_ADDR_IND:       cpuAddCycles(cpu, 8);  break;
        case ADDR_MODE_ADDR_IND_DISP:  cpuAddCycles(cpu, 10); break;
        case ADDR_MODE_ADDR_IND_INDEX: cpuAddCycles(cpu, 14); break;
        case ADDR_MODE_ABS_SHORT:      cpuAddCycles(cpu, 10); break;
        case ADDR_MODE_ABS_LONG:       cpuAddCycles(cpu, 12); break;
        case ADDR_MODE_PC_DISP:        cpuAddCycles(cpu, 10); break;
        case ADDR_MODE_PC_INDEX:       cpuAddCycles(cpu, 14); break;
        default:                       cpuAddCycles(cpu, 8);  break;
    }
}

/* ── JSR <ea> ── */
static void instrJsr(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_LONG);

    /* Push return address */
    cpu->a[7] -= 4;
    busWriteLong(cpu->bus, cpu->a[7] & ADDR_MASK, cpu->pc);

    cpu->pc = ea.address & ADDR_MASK;

    /* PRM: JSR (An)=16, d(An)=18, d(An,Xi)=22, abs.W=18, abs.L=20, d(PC)=18, d(PC,Xi)=22 */
    switch (ea.mode) {
        case ADDR_MODE_ADDR_IND:       cpuAddCycles(cpu, 16); break;
        case ADDR_MODE_ADDR_IND_DISP:  cpuAddCycles(cpu, 18); break;
        case ADDR_MODE_ADDR_IND_INDEX: cpuAddCycles(cpu, 22); break;
        case ADDR_MODE_ABS_SHORT:      cpuAddCycles(cpu, 18); break;
        case ADDR_MODE_ABS_LONG:       cpuAddCycles(cpu, 20); break;
        case ADDR_MODE_PC_DISP:        cpuAddCycles(cpu, 18); break;
        case ADDR_MODE_PC_INDEX:       cpuAddCycles(cpu, 22); break;
        default:                       cpuAddCycles(cpu, 16); break;
    }
}

/* ── RTS (return from subroutine) ── */
static void instrRts(Cpu *cpu) {
    cpu->pc = busReadLong(cpu->bus, cpu->a[7] & ADDR_MASK) & ADDR_MASK;
    cpu->a[7] += 4;
    cpuAddCycles(cpu, 16);
}

/* ── RTE (return from exception — privileged) ── */
static void instrRte(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }

    /* Pop SR */
    u16 newSR = busReadWord(cpu->bus, cpu->a[7] & ADDR_MASK);
    cpu->a[7] += 2;

    /* Pop PC */
    u32 newPC = busReadLong(cpu->bus, cpu->a[7] & ADDR_MASK);
    cpu->a[7] += 4;

    /* Apply SR (may switch from supervisor to user mode) */
    bool wasSuper = cpuIsSupervisor(cpu);
    cpu->sr = newSR;
    if (wasSuper && !(newSR & SR_S)) {
        cpuSetSupervisor(cpu, false);
    }

    cpu->pc = newPC & ADDR_MASK;
    cpuAddCycles(cpu, 20);
}

/* ── RTR (return and restore CCR) ── */
static void instrRtr(Cpu *cpu) {
    /* Pop CCR (lower byte of SR only) */
    u16 ccr = busReadWord(cpu->bus, cpu->a[7] & ADDR_MASK);
    cpu->a[7] += 2;
    cpu->sr = (cpu->sr & 0xFF00) | (ccr & 0x1F);

    /* Pop PC */
    cpu->pc = busReadLong(cpu->bus, cpu->a[7] & ADDR_MASK) & ADDR_MASK;
    cpu->a[7] += 4;

    cpuAddCycles(cpu, 20);
}

/* ── Registration ── */
void opcodesRegisterBranch(void) {
    /* Bcc/BRA/BSR: 0110 cccc dddddddd */
    for (int cc = 0; cc < 16; cc++) {
        for (int disp = 0; disp < 256; disp++) {
            u16 opcode = 0x6000 | (cc << 8) | disp;
            if (cc == 1) {
                opcodeRegister(opcode, instrBcc, "BSR", 18);
            } else if (cc == 0) {
                opcodeRegister(opcode, instrBcc, "BRA", 10);
            } else {
                opcodeRegister(opcode, instrBcc, "Bcc", 8);
            }
        }
    }

    /* DBcc: 0101 cccc 1100 1rrr */
    for (int cc = 0; cc < 16; cc++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x50C8 | (cc << 8) | reg, instrDbcc, "DBcc", 10);
        }
    }

    /* Scc: 0101 cccc 11mm mrrr */
    for (int cc = 0; cc < 16; cc++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                u16 opcode = 0x50C0 | (cc << 8) | (mode << 3) | reg;
                /* Skip DBcc patterns (mode=1, which is the 001 for register encoding) */
                if (mode == 1) continue;
                opcodeRegister(opcode, instrScc, "Scc", 4);
            }
        }
    }

    /* JMP: 0100 1110 11mm mrrr */
    for (int mode = 2; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x4EC0 | (mode << 3) | reg, instrJmp, "JMP", 8);
        }
    }

    /* JSR: 0100 1110 10mm mrrr */
    for (int mode = 2; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x4E80 | (mode << 3) | reg, instrJsr, "JSR", 16);
        }
    }

    /* RTS: 0100 1110 0111 0101 */
    opcodeRegister(0x4E75, instrRts, "RTS", 16);

    /* RTE: 0100 1110 0111 0011 */
    opcodeRegister(0x4E73, instrRte, "RTE", 20);

    /* RTR: 0100 1110 0111 0111 */
    opcodeRegister(0x4E77, instrRtr, "RTR", 20);
}
