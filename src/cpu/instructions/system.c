/* ===========================================================================
 *  system.c — TRAP, TRAPV, CHK, ILLEGAL, RESET, NOP, STOP
 *             MOVE to/from SR, MOVE to/from USP, TAS
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── NOP ── */
static void instrNop(Cpu *cpu) {
    cpuAddCycles(cpu, 4);
}

/* ── RESET (privileged — asserts RESET line on external bus) ── */
static void instrReset(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }
    /* External devices would be reset here.  CPU state is NOT affected. */
    LOG_INFO(MOD_CPU, "RESET instruction: external reset asserted");
    cpuAddCycles(cpu, 132);
}

/* ── STOP #imm (privileged) ── */
static void instrStop(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }
    u16 imm = instrFetchWord(cpu);
    bool wasSuper = cpuIsSupervisor(cpu);
    cpu->sr = imm;
    if (wasSuper && !(imm & SR_S)) {
        cpuSetSupervisor(cpu, false);
    }
    cpu->stopped = true;
    cpuAddCycles(cpu, 4);
}

/* ── TRAP #vector ── */
static void instrTrap(Cpu *cpu) {
    int vector = cpu->currentOpcode & 0xF;
    exceptionTrap(cpu, vector);
}

/* ── TRAPV ── */
static void instrTrapv(Cpu *cpu) {
    if (cpuGetFlag(cpu, SR_V)) {
        exceptionTrapV(cpu);
    } else {
        cpuAddCycles(cpu, 4);
    }
}

/* ── CHK <ea>,Dn ── */
static void instrChk(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg    = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    s16 upperBound = (s16)addrRead(cpu, &ea, SIZE_WORD);
    s16 value = (s16)(cpu->d[reg] & 0xFFFF);

    if (value < 0) {
        cpuSetFlag(cpu, SR_N, true);
        exceptionChk(cpu);
    } else if (value > upperBound) {
        cpuSetFlag(cpu, SR_N, false);
        exceptionChk(cpu);
    } else {
        cpuAddCycles(cpu, 10);
    }
}

/* ── ILLEGAL ── */
static void instrIllegal(Cpu *cpu) {
    exceptionIllegalInstruction(cpu);
}

/* ── MOVE from SR: 0100 0000 11mm mrrr ── */
static void instrMoveFromSr(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    addrWrite(cpu, &ea, cpu->sr, SIZE_WORD);

    cpuAddCycles(cpu, (ea.mode == ADDR_MODE_DATA_REG) ? 6 : 8);
}

/* ── MOVE to SR: 0100 0110 11mm mrrr (privileged) ── */
static void instrMoveToSr(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }

    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    u16 newSR = (u16)addrRead(cpu, &ea, SIZE_WORD);

    bool wasSuper = cpuIsSupervisor(cpu);
    cpu->sr = newSR;
    if (wasSuper && !(newSR & SR_S)) {
        cpuSetSupervisor(cpu, false);
    }

    cpuAddCycles(cpu, 12);
}

/* ── MOVE to CCR: 0100 0100 11mm mrrr ── */
static void instrMoveToCcr(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_WORD);
    u16 value = (u16)addrRead(cpu, &ea, SIZE_WORD);

    cpu->sr = (cpu->sr & 0xFF00) | (value & 0x1F);

    cpuAddCycles(cpu, 12);
}

/* ── MOVE USP,An / MOVE An,USP (privileged) ── */
static void instrMoveUsp(Cpu *cpu) {
    if (!cpuIsSupervisor(cpu)) {
        exceptionPrivilegeViolation(cpu);
        return;
    }

    u16 op = cpu->currentOpcode;
    int reg = opSrcReg(op);
    bool toUsp = (op & 0x08) == 0;  /* bit 3: 0=An->USP, 1=USP->An */

    if (toUsp) {
        cpu->usp = cpu->a[reg];
    } else {
        cpu->a[reg] = cpu->usp;
    }

    cpuAddCycles(cpu, 4);
}

/* ── TAS <ea> (test and set, atomic) ── */
static void instrTas(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    u8 val = (u8)addrRead(cpu, &ea, SIZE_BYTE);

    /* Set flags based on value */
    aluSetLogicFlags(cpu, val, SIZE_BYTE);

    /* Set MSB of operand */
    addrWrite(cpu, &ea, val | 0x80, SIZE_BYTE);

    cpuAddCycles(cpu, (ea.mode == ADDR_MODE_DATA_REG) ? 4 : 14);
}

/* ── Registration ── */
void opcodesRegisterSystem(void) {
    /* NOP: 0100 1110 0111 0001 */
    opcodeRegister(0x4E71, instrNop, "NOP", 4);

    /* RESET: 0100 1110 0111 0000 */
    opcodeRegister(0x4E70, instrReset, "RESET", 132);

    /* STOP: 0100 1110 0111 0010 */
    opcodeRegister(0x4E72, instrStop, "STOP", 4);

    /* ILLEGAL: 0100 1010 1111 1100 */
    opcodeRegister(0x4AFC, instrIllegal, "ILLEGAL", 34);

    /* TRAP: 0100 1110 0100 vvvv */
    for (int v = 0; v < 16; v++) {
        opcodeRegister(0x4E40 | v, instrTrap, "TRAP", 34);
    }

    /* TRAPV: 0100 1110 0111 0110 */
    opcodeRegister(0x4E76, instrTrapv, "TRAPV", 4);

    /* CHK: 0100 rrr1 10mm mRRR */
    for (int reg = 0; reg < 8; reg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int ereg = 0; ereg < 8; ereg++) {
                opcodeRegister(0x4180 | (reg << 9) | (mode << 3) | ereg,
                               instrChk, "CHK", 10);
            }
        }
    }

    /* MOVE from SR: 0100 0000 11mm mrrr */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x40C0 | (mode << 3) | reg, instrMoveFromSr, "MOVE_FROM_SR", 6);
        }
    }

    /* MOVE to SR: 0100 0110 11mm mrrr */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x46C0 | (mode << 3) | reg, instrMoveToSr, "MOVE_TO_SR", 12);
        }
    }

    /* MOVE to CCR: 0100 0100 11mm mrrr */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x44C0 | (mode << 3) | reg, instrMoveToCcr, "MOVE_TO_CCR", 12);
        }
    }

    /* MOVE USP: 0100 1110 0110 drrr (d=direction bit 3) */
    for (int reg = 0; reg < 8; reg++) {
        opcodeRegister(0x4E60 | reg, instrMoveUsp, "MOVE_USP", 4);  /* An -> USP */
        opcodeRegister(0x4E68 | reg, instrMoveUsp, "MOVE_USP", 4);  /* USP -> An */
    }

    /* TAS: 0100 1010 11mm mrrr */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x4AC0 | (mode << 3) | reg, instrTas, "TAS", 4);
        }
    }
}
