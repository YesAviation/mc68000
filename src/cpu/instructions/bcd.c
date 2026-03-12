/* ===========================================================================
 *  bcd.c — ABCD, SBCD, NBCD (Binary-Coded Decimal operations)
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── ABCD Dy,Dx / ABCD -(Ay),-(Ax) ── */
static void instrAbcd(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int rx = opDstReg(op);
    int ry = opSrcReg(op);
    bool isMemory = (op & 0x08) != 0;

    u8 src, dst;

    if (isMemory) {
        cpu->a[ry] -= 1;
        cpu->a[rx] -= 1;
        src = busReadByte(cpu->bus, cpu->a[ry] & ADDR_MASK);
        dst = busReadByte(cpu->bus, cpu->a[rx] & ADDR_MASK);
        u8 result = aluAbcd(cpu, src, dst);
        busWriteByte(cpu->bus, cpu->a[rx] & ADDR_MASK, result);
        cpuAddCycles(cpu, 18);
    } else {
        src = (u8)cpu->d[ry];
        dst = (u8)cpu->d[rx];
        u8 result = aluAbcd(cpu, src, dst);
        cpuWriteD(cpu, rx, result, SIZE_BYTE);
        cpuAddCycles(cpu, 6);
    }
}

/* ── SBCD Dy,Dx / SBCD -(Ay),-(Ax) ── */
static void instrSbcd(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int rx = opDstReg(op);
    int ry = opSrcReg(op);
    bool isMemory = (op & 0x08) != 0;

    u8 src, dst;

    if (isMemory) {
        cpu->a[ry] -= 1;
        cpu->a[rx] -= 1;
        src = busReadByte(cpu->bus, cpu->a[ry] & ADDR_MASK);
        dst = busReadByte(cpu->bus, cpu->a[rx] & ADDR_MASK);
        u8 result = aluSbcd(cpu, src, dst);
        busWriteByte(cpu->bus, cpu->a[rx] & ADDR_MASK, result);
        cpuAddCycles(cpu, 18);
    } else {
        src = (u8)cpu->d[ry];
        dst = (u8)cpu->d[rx];
        u8 result = aluSbcd(cpu, src, dst);
        cpuWriteD(cpu, rx, result, SIZE_BYTE);
        cpuAddCycles(cpu, 6);
    }
}

/* ── NBCD <ea> ── */
static void instrNbcd(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    u8 val = (u8)addrRead(cpu, &ea, SIZE_BYTE);
    u8 result = aluNbcd(cpu, val);
    addrWrite(cpu, &ea, result, SIZE_BYTE);

    cpuAddCycles(cpu, (ea.mode == ADDR_MODE_DATA_REG) ? 6 : 8);
}

/* ── Registration ── */
void opcodesRegisterBcd(void) {
    /* ABCD: 1100 xxx1 0000 ryyy (reg) / 1100 xxx1 0000 1yyy (mem) */
    for (int rx = 0; rx < 8; rx++) {
        for (int ry = 0; ry < 8; ry++) {
            opcodeRegister(0xC100 | (rx << 9) | ry,       instrAbcd, "ABCD", 6);
            opcodeRegister(0xC108 | (rx << 9) | ry,       instrAbcd, "ABCD", 18);
        }
    }

    /* SBCD: 1000 xxx1 0000 ryyy (reg) / 1000 xxx1 0000 1yyy (mem) */
    for (int rx = 0; rx < 8; rx++) {
        for (int ry = 0; ry < 8; ry++) {
            opcodeRegister(0x8100 | (rx << 9) | ry,       instrSbcd, "SBCD", 6);
            opcodeRegister(0x8108 | (rx << 9) | ry,       instrSbcd, "SBCD", 18);
        }
    }

    /* NBCD: 0100 1000 00mm mrrr */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x4800 | (mode << 3) | reg, instrNbcd, "NBCD", 6);
        }
    }
}
