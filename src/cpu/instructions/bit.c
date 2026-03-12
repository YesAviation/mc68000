/* ===========================================================================
 *  bit.c — BTST, BSET, BCLR, BCHG (bit manipulation)
 *
 *  Two forms: dynamic (bit number in Dn) and static (bit number in ext word)
 *  Register operands: 32-bit (bit 0-31)
 *  Memory operands: 8-bit (bit 0-7)
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── Dynamic bit operations (bit number in Dn) ── */
static void instrBtstDyn(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int bitReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    int bitNum = cpu->d[bitReg] & (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);

    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 6 : 4);
}

static void instrBchgDyn(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int bitReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    int bitNum = cpu->d[bitReg] & (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val ^= (1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, 8);
}

static void instrBclrDyn(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int bitReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    int bitNum = cpu->d[bitReg] & (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val &= ~(1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 10 : 8);
}

static void instrBsetDyn(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int bitReg = opDstReg(op);
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    int bitNum = cpu->d[bitReg] & (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val |= (1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, 8);
}

/* ── Static bit operations (bit number in extension word) ── */
static void instrBtstStatic(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int bitNum = instrFetchWord(cpu) & 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    bitNum &= (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);

    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 10 : 8);
}

static void instrBchgStatic(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int bitNum = instrFetchWord(cpu) & 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    bitNum &= (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val ^= (1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, 12);
}

static void instrBclrStatic(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int bitNum = instrFetchWord(cpu) & 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    bitNum &= (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val &= ~(1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, (sz == SIZE_LONG) ? 14 : 12);
}

static void instrBsetStatic(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);
    int bitNum = instrFetchWord(cpu) & 0xFF;

    EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, SIZE_BYTE);
    OperationSize sz = (ea.mode == ADDR_MODE_DATA_REG) ? SIZE_LONG : SIZE_BYTE;
    bitNum &= (sz == SIZE_LONG ? 31 : 7);

    u32 val = addrRead(cpu, &ea, sz);
    cpuSetFlag(cpu, SR_Z, (val & (1u << bitNum)) == 0);
    val |= (1u << bitNum);
    addrWrite(cpu, &ea, val, sz);

    cpuAddCycles(cpu, 12);
}

/* ── Registration ── */
void opcodesRegisterBit(void) {
    /* Dynamic: 0000 rrr1 00mm mRRR (BTST/BCHG/BCLR/BSET) */
    for (int bitReg = 0; bitReg < 8; bitReg++) {
        for (int mode = 0; mode < 8; mode++) {
            for (int reg = 0; reg < 8; reg++) {
                /* BTST: 0000 rrr1 00mm mRRR */
                opcodeRegister(0x0100 | (bitReg << 9) | (mode << 3) | reg,
                               instrBtstDyn, "BTST", 4);
                /* BCHG: 0000 rrr1 01mm mRRR */
                opcodeRegister(0x0140 | (bitReg << 9) | (mode << 3) | reg,
                               instrBchgDyn, "BCHG", 8);
                /* BCLR: 0000 rrr1 10mm mRRR */
                opcodeRegister(0x0180 | (bitReg << 9) | (mode << 3) | reg,
                               instrBclrDyn, "BCLR", 8);
                /* BSET: 0000 rrr1 11mm mRRR */
                opcodeRegister(0x01C0 | (bitReg << 9) | (mode << 3) | reg,
                               instrBsetDyn, "BSET", 8);
            }
        }
    }

    /* Static: 0000 1000 00mm mrrr (BTST) etc. */
    for (int mode = 0; mode < 8; mode++) {
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(0x0800 | (mode << 3) | reg, instrBtstStatic, "BTST", 8);
            opcodeRegister(0x0840 | (mode << 3) | reg, instrBchgStatic, "BCHG", 12);
            opcodeRegister(0x0880 | (mode << 3) | reg, instrBclrStatic, "BCLR", 12);
            opcodeRegister(0x08C0 | (mode << 3) | reg, instrBsetStatic, "BSET", 12);
        }
    }
}
