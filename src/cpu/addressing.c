/* ===========================================================================
 *  addressing.c — Effective address calculation and read/write
 * =========================================================================== */
#include "cpu/addressing.h"
#include "bus/bus.h"
#include "common/log.h"

/* ── Decode mode/reg to AddressingMode enum ── */
AddressingMode addrDecodeMode(int mode, int reg) {
    switch (mode) {
        case 0: return ADDR_MODE_DATA_REG;
        case 1: return ADDR_MODE_ADDR_REG;
        case 2: return ADDR_MODE_ADDR_IND;
        case 3: return ADDR_MODE_ADDR_IND_POSTINC;
        case 4: return ADDR_MODE_ADDR_IND_PREDEC;
        case 5: return ADDR_MODE_ADDR_IND_DISP;
        case 6: return ADDR_MODE_ADDR_IND_INDEX;
        case 7:
            switch (reg) {
                case 0: return ADDR_MODE_ABS_SHORT;
                case 1: return ADDR_MODE_ABS_LONG;
                case 2: return ADDR_MODE_PC_DISP;
                case 3: return ADDR_MODE_PC_INDEX;
                case 4: return ADDR_MODE_IMMEDIATE;
                default: return ADDR_MODE_INVALID;
            }
        default: return ADDR_MODE_INVALID;
    }
}

/* ── Fetch a 16-bit extension word and advance PC ── */
static u16 fetchExtWord(Cpu *cpu) {
    u16 w = busReadWord(cpu->bus, cpu->pc);
    cpu->pc += 2;
    return w;
}

/* ── Fetch a 32-bit extension long and advance PC ── */
static u32 fetchExtLong(Cpu *cpu) {
    u32 hi = (u32)busReadWord(cpu->bus, cpu->pc) << 16;
    cpu->pc += 2;
    u32 lo = (u32)busReadWord(cpu->bus, cpu->pc);
    cpu->pc += 2;
    return hi | lo;
}

/* ── Parse brief extension word for indexed modes ──
 *  Bit 15:     D/A (0=Dn, 1=An)
 *  Bits 14-12: register number
 *  Bit 11:     W/L (0=sign-extended word, 1=long)
 *  Bits 7-0:   8-bit signed displacement */
static u32 calcIndex(Cpu *cpu, u32 baseAddr) {
    u16 ext = fetchExtWord(cpu);

    int  xnReg  = (ext >> 12) & 7;
    bool isAddr = (ext & 0x8000) != 0;
    bool isLong = (ext & 0x0800) != 0;
    s8   disp8  = (s8)(ext & 0xFF);

    s32 xnVal;
    if (isAddr) {
        xnVal = (s32)cpu->a[xnReg];
    } else {
        xnVal = isLong ? (s32)cpu->d[xnReg]
                       : signExtend16((u16)cpu->d[xnReg]);
    }

    return (u32)((s32)baseAddr + (s32)disp8 + xnVal);
}

/* ── Calculate effective address ── */
EffectiveAddress addrCalcEA(Cpu *cpu, int mode, int reg, OperationSize sz) {
    EffectiveAddress ea;
    ea.mode = addrDecodeMode(mode, reg);
    ea.reg  = reg;
    ea.address = 0;

    switch (ea.mode) {
        case ADDR_MODE_DATA_REG:
            /* Value is in d[reg], no memory address */
            break;

        case ADDR_MODE_ADDR_REG:
            /* Value is in a[reg], no memory address */
            break;

        case ADDR_MODE_ADDR_IND:
            ea.address = cpu->a[reg] & ADDR_MASK;
            break;

        case ADDR_MODE_ADDR_IND_POSTINC:
            ea.address = cpu->a[reg] & ADDR_MASK;
            cpu->a[reg] += addrIncrement(reg, sz);
            break;

        case ADDR_MODE_ADDR_IND_PREDEC:
            cpu->a[reg] -= addrIncrement(reg, sz);
            ea.address = cpu->a[reg] & ADDR_MASK;
            break;

        case ADDR_MODE_ADDR_IND_DISP: {
            s16 disp = (s16)fetchExtWord(cpu);
            ea.address = (u32)((s32)cpu->a[reg] + disp) & ADDR_MASK;
            break;
        }

        case ADDR_MODE_ADDR_IND_INDEX:
            ea.address = calcIndex(cpu, cpu->a[reg]) & ADDR_MASK;
            break;

        case ADDR_MODE_ABS_SHORT: {
            s16 addr = (s16)fetchExtWord(cpu);
            ea.address = (u32)(s32)addr & ADDR_MASK;
            break;
        }

        case ADDR_MODE_ABS_LONG:
            ea.address = fetchExtLong(cpu) & ADDR_MASK;
            break;

        case ADDR_MODE_PC_DISP: {
            u32 pcSave = cpu->pc;  /* PC of the extension word */
            s16 disp = (s16)fetchExtWord(cpu);
            ea.address = (u32)((s32)pcSave + disp) & ADDR_MASK;
            break;
        }

        case ADDR_MODE_PC_INDEX: {
            u32 pcSave = cpu->pc;  /* PC of the extension word */
            ea.address = calcIndex(cpu, pcSave) & ADDR_MASK;
            break;
        }

        case ADDR_MODE_IMMEDIATE:
            /* Immediate data follows the opcode */
            if (sz == SIZE_LONG) {
                ea.address = cpu->pc;
                cpu->pc += 4;
            } else {
                ea.address = cpu->pc;
                cpu->pc += 2;
            }
            break;

        case ADDR_MODE_INVALID:
            LOG_ERROR(MOD_CPU, "Invalid addressing mode: mode=%d reg=%d", mode, reg);
            break;
    }

    return ea;
}

/* ── Read through effective address ── */
u32 addrRead(Cpu *cpu, EffectiveAddress *ea, OperationSize sz) {
    switch (ea->mode) {
        case ADDR_MODE_DATA_REG:
            return cpuReadD(cpu, ea->reg, sz);

        case ADDR_MODE_ADDR_REG:
            return cpu->a[ea->reg] & sizeMask(sz);

        case ADDR_MODE_IMMEDIATE:
            if (sz == SIZE_LONG) {
                return busReadLong(cpu->bus, ea->address);
            } else {
                /* Byte and word: data is in the low bits of the extension word */
                u16 w = busReadWord(cpu->bus, ea->address);
                return (sz == SIZE_BYTE) ? (w & 0xFF) : w;
            }

        default:
            /* All memory addressing modes */
            switch (sz) {
                case SIZE_BYTE: return busReadByte(cpu->bus, ea->address);
                case SIZE_WORD: return busReadWord(cpu->bus, ea->address);
                case SIZE_LONG: return busReadLong(cpu->bus, ea->address);
            }
    }
    return 0;
}

/* ── Write through effective address ── */
void addrWrite(Cpu *cpu, EffectiveAddress *ea, u32 value, OperationSize sz) {
    switch (ea->mode) {
        case ADDR_MODE_DATA_REG:
            cpuWriteD(cpu, ea->reg, value, sz);
            return;

        case ADDR_MODE_ADDR_REG:
            /* Writes to An are always longword (sign-extended for word) */
            if (sz == SIZE_WORD) {
                cpu->a[ea->reg] = (u32)signExtend16((u16)value);
            } else {
                cpu->a[ea->reg] = value;
            }
            return;

        case ADDR_MODE_IMMEDIATE:
            /* Can't write to immediate — programming error */
            LOG_ERROR(MOD_CPU, "Attempt to write to immediate addressing mode");
            return;

        default:
            /* All memory addressing modes */
            switch (sz) {
                case SIZE_BYTE: busWriteByte(cpu->bus, ea->address, (u8)value);  return;
                case SIZE_WORD: busWriteWord(cpu->bus, ea->address, (u16)value); return;
                case SIZE_LONG: busWriteLong(cpu->bus, ea->address, value);      return;
            }
    }
}

/* ── EA calculation cycle costs ──
 *  From M68000 Programmer's Reference Manual, Effective Address Calculation Times */
u32 addrCalcCycles(AddressingMode mode, OperationSize sz) {
    bool isLong = (sz == SIZE_LONG);
    switch (mode) {
        case ADDR_MODE_DATA_REG:         return 0;
        case ADDR_MODE_ADDR_REG:         return 0;
        case ADDR_MODE_ADDR_IND:         return isLong ? 8 : 4;
        case ADDR_MODE_ADDR_IND_POSTINC: return isLong ? 8 : 4;
        case ADDR_MODE_ADDR_IND_PREDEC:  return isLong ? 10 : 6;
        case ADDR_MODE_ADDR_IND_DISP:    return isLong ? 12 : 8;
        case ADDR_MODE_ADDR_IND_INDEX:   return isLong ? 14 : 10;
        case ADDR_MODE_ABS_SHORT:        return isLong ? 12 : 8;
        case ADDR_MODE_ABS_LONG:         return isLong ? 16 : 12;
        case ADDR_MODE_PC_DISP:          return isLong ? 12 : 8;
        case ADDR_MODE_PC_INDEX:         return isLong ? 14 : 10;
        case ADDR_MODE_IMMEDIATE:        return isLong ? 8 : 4;
        case ADDR_MODE_INVALID:          return 0;
    }
    return 0;
}
