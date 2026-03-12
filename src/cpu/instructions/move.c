/* ===========================================================================
 *  move.c — MOVE, MOVEA, MOVEQ, MOVEM, MOVEP, LEA, PEA, EXG, LINK, UNLK, SWAP
 *
 *  MOVE is the single most common 68000 instruction.
 *  Opcode lines: 0001 (MOVE.B), 0010 (MOVE.L/MOVEA.L), 0011 (MOVE.W/MOVEA.W)
 *  Plus MOVEQ (line 0111), and various misc in line 0100.
 * =========================================================================== */
#include "cpu/instructions/instructions.h"
#include "cpu/opcodes.h"

/* ── MOVE <ea>,<ea> ──
 *  Opcode: 00ss dddDDD SSSsss
 *    ss   = size (01=byte, 11=word, 10=long)
 *    DDD  = dest mode,  ddd = dest register
 *    SSS  = src mode,   sss = src register */
static void instrMove(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSizeMove(op);

    int srcMode = opSrcMode(op);
    int srcReg  = opSrcReg(op);
    int dstMode = opDstMode(op);
    int dstReg  = opDstReg(op);

    EffectiveAddress src = addrCalcEA(cpu, srcMode, srcReg, sz);
    u32 value = addrRead(cpu, &src, sz);

    EffectiveAddress dst = addrCalcEA(cpu, dstMode, dstReg, sz);
    addrWrite(cpu, &dst, value, sz);

    /* MOVE sets N and Z, clears V and C */
    aluSetLogicFlags(cpu, value, sz);

    /* Cycle timing from M68000 PRM MOVE timing tables.
     * The tables are indexed by [src_addr_mode_index][dst_addr_mode_index]
     * and already include ALL cycle costs (no separate EA add). */
    int srcIdx = (int)addrDecodeMode(srcMode, srcReg);
    /* Destination modes for MOVE table: Dn=0, An=1, (An)=2, (An)+=3,
     * -(An)=4, d(An)=5, d(An,Xi)=6, abs.W=7, abs.L=8 */
    int dstIdx;
    AddressingMode dm = addrDecodeMode(dstMode, dstReg);
    if (dm <= ADDR_MODE_ADDR_IND_INDEX)  dstIdx = (int)dm;
    else if (dm == ADDR_MODE_ABS_SHORT)  dstIdx = 7;
    else /* ABS_LONG */                  dstIdx = 8;
    if (sz == SIZE_LONG)
        cpuAddCycles(cpu, timingMoveL[srcIdx][dstIdx]);
    else
        cpuAddCycles(cpu, timingMoveBW[srcIdx][dstIdx]);
}

/* ── MOVEA <ea>,An ──
 *  Like MOVE but destination is address register, no flags affected.
 *  Word operations are sign-extended to 32 bits. */
static void instrMovea(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    OperationSize sz = opSizeMove(op);

    int srcMode = opSrcMode(op);
    int srcReg  = opSrcReg(op);
    int dstReg  = opDstReg(op);

    EffectiveAddress src = addrCalcEA(cpu, srcMode, srcReg, sz);
    u32 value = addrRead(cpu, &src, sz);

    /* Sign-extend word to long */
    if (sz == SIZE_WORD) {
        value = (u32)signExtend16((u16)value);
    }

    cpu->a[dstReg] = value;
    /* No flags affected */

    /* MOVEA timing: same tables as MOVE, destination column = An (index 1) */
    int srcIdx = (int)addrDecodeMode(srcMode, srcReg);
    if (sz == SIZE_LONG)
        cpuAddCycles(cpu, timingMoveL[srcIdx][1]);
    else
        cpuAddCycles(cpu, timingMoveBW[srcIdx][1]);
}

/* ── MOVEQ #imm8,Dn ──
 *  Opcode: 0111 rrr0 dddddddd
 *  Sign-extends 8-bit immediate to 32-bit, stores in Dn */
static void instrMoveq(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg = opDstReg(op);
    s32 value = signExtend8((u8)(op & 0xFF));

    cpu->d[reg] = (u32)value;

    aluSetLogicFlags(cpu, (u32)value, SIZE_LONG);
    cpuAddCycles(cpu, 4);
}

/* ── MOVEM (register list to/from memory) ──
 *  Opcode: 0100 1d00 1s mmmrrr
 *    d   = direction (0 = register-to-memory, 1 = memory-to-register)
 *    s   = size (0 = word, 1 = long)
 *    For predecrement mode, the register mask is reversed:
 *      bits 0-7 = A7..A0, bits 8-15 = D7..D0
 *    Otherwise:
 *      bits 0-7 = D0..D7, bits 8-15 = A0..A7 */
static void instrMovemRegToMem(Cpu *cpu) {
    u16 op   = cpu->currentOpcode;
    u16 mask = instrFetchWord(cpu);
    OperationSize sz = (op & 0x0040) ? SIZE_LONG : SIZE_WORD;
    u32 step = sizeInBytes(sz);

    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    if (eaMode == 4) {
        /* Predecrement -(An): register mask is reversed, writes go downward.
         * The register order in the mask is A7(bit0)..A0(bit7) D7(bit8)..D0(bit15) */
        u32 addr = cpu->a[eaReg];
        for (int i = 15; i >= 0; i--) {
            if (mask & (1 << i)) {
                addr -= step;
                /* In reversed order: bit 0 = A7, bit 7 = A0, bit 8 = D7, bit 15 = D0 */
                int reg;
                if (i < 8) {
                    reg = 7 - i;  /* A7..A0 */
                    if (sz == SIZE_LONG)
                        busWriteLong(cpu->bus, addr & ADDR_MASK, cpu->a[reg]);
                    else
                        busWriteWord(cpu->bus, addr & ADDR_MASK, (u16)cpu->a[reg]);
                } else {
                    reg = 15 - i; /* D7..D0 */
                    if (sz == SIZE_LONG)
                        busWriteLong(cpu->bus, addr & ADDR_MASK, cpu->d[reg]);
                    else
                        busWriteWord(cpu->bus, addr & ADDR_MASK, (u16)cpu->d[reg]);
                }
            }
        }
        cpu->a[eaReg] = addr;
    } else {
        /* Normal mode: mask is D0(bit0)..D7(bit7) A0(bit8)..A7(bit15) */
        EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
        u32 addr = ea.address;
        for (int i = 0; i < 16; i++) {
            if (mask & (1 << i)) {
                u32 val = (i < 8) ? cpu->d[i] : cpu->a[i - 8];
                if (sz == SIZE_LONG)
                    busWriteLong(cpu->bus, addr & ADDR_MASK, val);
                else
                    busWriteWord(cpu->bus, addr & ADDR_MASK, (u16)val);
                addr += step;
            }
        }
    }

    /* Count set bits for timing */
    int count = 0;
    for (u16 m = mask; m; m >>= 1) count += (m & 1);
    cpuAddCycles(cpu, 8 + count * ((sz == SIZE_LONG) ? 8 : 4));
}

static void instrMovemMemToReg(Cpu *cpu) {
    u16 op   = cpu->currentOpcode;
    u16 mask = instrFetchWord(cpu);
    OperationSize sz = (op & 0x0040) ? SIZE_LONG : SIZE_WORD;
    u32 step = sizeInBytes(sz);

    int eaMode = opSrcMode(op);
    int eaReg  = opSrcReg(op);

    /* Mask is always D0(bit0)..D7(bit7) A0(bit8)..A7(bit15) */
    u32 addr;
    if (eaMode == 3) {
        /* Postincrement (An)+ */
        addr = cpu->a[eaReg];
    } else {
        EffectiveAddress ea = addrCalcEA(cpu, eaMode, eaReg, sz);
        addr = ea.address;
    }

    for (int i = 0; i < 16; i++) {
        if (mask & (1 << i)) {
            u32 val;
            if (sz == SIZE_LONG) {
                val = busReadLong(cpu->bus, addr & ADDR_MASK);
            } else {
                /* Word: sign-extend to 32 bits */
                val = (u32)signExtend16(busReadWord(cpu->bus, addr & ADDR_MASK));
            }
            if (i < 8)
                cpu->d[i] = val;
            else
                cpu->a[i - 8] = val;
            addr += step;
        }
    }

    /* Update address register for postincrement */
    if (eaMode == 3) {
        cpu->a[eaReg] = addr;
    }

    int count = 0;
    for (u16 m = mask; m; m >>= 1) count += (m & 1);
    cpuAddCycles(cpu, 12 + count * ((sz == SIZE_LONG) ? 8 : 4));
}

/* ── MOVEP (register to/from memory, alternating bytes) ──
 *  Opcode: 0000 ddd ooo 001 aaa
 *    ddd = data register, aaa = address register
 *    ooo = opmode: 100=mem→reg.W, 101=mem→reg.L, 110=reg→mem.W, 111=reg→mem.L
 *  Extension word: signed 16-bit displacement
 *  Transfers bytes at addr, addr+2, addr+4, ... (alternating, skipping odd bytes) */
static void instrMovep(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int dataReg = opDstReg(op);
    int addrReg = opSrcReg(op);
    int opmode  = (op >> 6) & 7;
    s16 disp    = (s16)instrFetchWord(cpu);
    u32 addr    = (u32)((s32)cpu->a[addrReg] + disp);

    switch (opmode) {
        case 4: /* Memory → Register, Word */
        {
            u8 hi = busReadByte(cpu->bus, addr & ADDR_MASK);
            u8 lo = busReadByte(cpu->bus, (addr + 2) & ADDR_MASK);
            cpuWriteD(cpu, dataReg, (u32)((hi << 8) | lo), SIZE_WORD);
            cpuAddCycles(cpu, 16);
            break;
        }
        case 5: /* Memory → Register, Long */
        {
            u8 b3 = busReadByte(cpu->bus, addr & ADDR_MASK);
            u8 b2 = busReadByte(cpu->bus, (addr + 2) & ADDR_MASK);
            u8 b1 = busReadByte(cpu->bus, (addr + 4) & ADDR_MASK);
            u8 b0 = busReadByte(cpu->bus, (addr + 6) & ADDR_MASK);
            cpu->d[dataReg] = ((u32)b3 << 24) | ((u32)b2 << 16) |
                              ((u32)b1 << 8)  | (u32)b0;
            cpuAddCycles(cpu, 24);
            break;
        }
        case 6: /* Register → Memory, Word */
        {
            u32 val = cpu->d[dataReg];
            busWriteByte(cpu->bus, addr & ADDR_MASK, (u8)(val >> 8));
            busWriteByte(cpu->bus, (addr + 2) & ADDR_MASK, (u8)val);
            cpuAddCycles(cpu, 16);
            break;
        }
        case 7: /* Register → Memory, Long */
        {
            u32 val = cpu->d[dataReg];
            busWriteByte(cpu->bus, addr & ADDR_MASK, (u8)(val >> 24));
            busWriteByte(cpu->bus, (addr + 2) & ADDR_MASK, (u8)(val >> 16));
            busWriteByte(cpu->bus, (addr + 4) & ADDR_MASK, (u8)(val >> 8));
            busWriteByte(cpu->bus, (addr + 6) & ADDR_MASK, (u8)val);
            cpuAddCycles(cpu, 24);
            break;
        }
        default:
            break;
    }
}

/* ── LEA <ea>,An ── */
static void instrLea(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int srcMode = opSrcMode(op);
    int srcReg  = opSrcReg(op);
    int dstReg  = opDstReg(op);

    /* LEA only needs the address, not the value */
    EffectiveAddress ea = addrCalcEA(cpu, srcMode, srcReg, SIZE_LONG);
    cpu->a[dstReg] = ea.address;
    /* No flags affected */

    /* LEA timing from PRM:
     * (An)=4, d(An)=8, d(An,Xi)=12, abs.W=8, abs.L=12, d(PC)=8, d(PC,Xi)=12 */
    switch (ea.mode) {
        case ADDR_MODE_ADDR_IND:       cpuAddCycles(cpu, 4);  break;
        case ADDR_MODE_ADDR_IND_DISP:  cpuAddCycles(cpu, 8);  break;
        case ADDR_MODE_ADDR_IND_INDEX: cpuAddCycles(cpu, 12); break;
        case ADDR_MODE_ABS_SHORT:      cpuAddCycles(cpu, 8);  break;
        case ADDR_MODE_ABS_LONG:       cpuAddCycles(cpu, 12); break;
        case ADDR_MODE_PC_DISP:        cpuAddCycles(cpu, 8);  break;
        case ADDR_MODE_PC_INDEX:       cpuAddCycles(cpu, 12); break;
        default:                       cpuAddCycles(cpu, 4);  break;
    }
}

/* ── PEA <ea> ── */
static void instrPea(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int srcMode = opSrcMode(op);
    int srcReg  = opSrcReg(op);

    EffectiveAddress ea = addrCalcEA(cpu, srcMode, srcReg, SIZE_LONG);

    /* Push address onto stack */
    cpu->a[7] -= 4;
    busWriteLong(cpu->bus, cpu->a[7] & ADDR_MASK, ea.address);

    /* PEA timing from PRM:
     * (An)=12, d(An)=16, d(An,Xi)=20, abs.W=16, abs.L=20, d(PC)=16, d(PC,Xi)=20 */
    switch (ea.mode) {
        case ADDR_MODE_ADDR_IND:       cpuAddCycles(cpu, 12); break;
        case ADDR_MODE_ADDR_IND_DISP:  cpuAddCycles(cpu, 16); break;
        case ADDR_MODE_ADDR_IND_INDEX: cpuAddCycles(cpu, 20); break;
        case ADDR_MODE_ABS_SHORT:      cpuAddCycles(cpu, 16); break;
        case ADDR_MODE_ABS_LONG:       cpuAddCycles(cpu, 20); break;
        case ADDR_MODE_PC_DISP:        cpuAddCycles(cpu, 16); break;
        case ADDR_MODE_PC_INDEX:       cpuAddCycles(cpu, 20); break;
        default:                       cpuAddCycles(cpu, 12); break;
    }
}

/* ── EXG Rx,Ry ── */
static void instrExg(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int rx = opDstReg(op);
    int ry = opSrcReg(op);
    int opmode = (op >> 3) & 0x1F;

    u32 tmp;
    switch (opmode) {
        case 0x08: /* Data registers */
            tmp = cpu->d[rx]; cpu->d[rx] = cpu->d[ry]; cpu->d[ry] = tmp;
            break;
        case 0x09: /* Address registers */
            tmp = cpu->a[rx]; cpu->a[rx] = cpu->a[ry]; cpu->a[ry] = tmp;
            break;
        case 0x11: /* Data register and address register */
            tmp = cpu->d[rx]; cpu->d[rx] = cpu->a[ry]; cpu->a[ry] = tmp;
            break;
        default:
            break;
    }
    cpuAddCycles(cpu, 6);
}

/* ── LINK An,#disp ── */
static void instrLink(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg = opSrcReg(op);
    s16 disp = (s16)instrFetchWord(cpu);

    /* Push An onto stack */
    cpu->a[7] -= 4;
    busWriteLong(cpu->bus, cpu->a[7] & ADDR_MASK, cpu->a[reg]);

    /* An = SP */
    cpu->a[reg] = cpu->a[7];

    /* SP += displacement */
    cpu->a[7] = (u32)((s32)cpu->a[7] + disp);

    cpuAddCycles(cpu, 16);
}

/* ── UNLK An ── */
static void instrUnlk(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg = opSrcReg(op);

    /* SP = An */
    cpu->a[7] = cpu->a[reg];

    /* Pop An from stack */
    cpu->a[reg] = busReadLong(cpu->bus, cpu->a[7] & ADDR_MASK);
    cpu->a[7] += 4;

    cpuAddCycles(cpu, 12);
}

/* ── SWAP Dn ── */
static void instrSwap(Cpu *cpu) {
    u16 op = cpu->currentOpcode;
    int reg = opSrcReg(op);

    u32 val = cpu->d[reg];
    cpu->d[reg] = (val >> 16) | (val << 16);

    aluSetLogicFlags(cpu, cpu->d[reg], SIZE_LONG);
    cpuAddCycles(cpu, 4);
}

/* ── Register all MOVE-family opcodes ── */
void opcodesRegisterMove(void) {
    /* MOVEQ: 0111 rrr0 dddddddd */
    for (int reg = 0; reg < 8; reg++) {
        for (int data = 0; data < 256; data++) {
            u16 opcode = 0x7000 | (reg << 9) | data;
            opcodeRegister(opcode, instrMoveq, "MOVEQ", 4);
        }
    }

    /* MOVE.B (line 0001), MOVE.W (line 0011), MOVE.L (line 0010) */
    u16 lines[] = { 0x1000, 0x3000, 0x2000 };
    for (int line = 0; line < 3; line++) {
        for (int dstMode = 0; dstMode < 8; dstMode++) {
            for (int dstReg = 0; dstReg < 8; dstReg++) {
                for (int srcMode = 0; srcMode < 8; srcMode++) {
                    for (int srcReg = 0; srcReg < 8; srcReg++) {
                        u16 opcode = lines[line] |
                                     (dstReg << 9) | (dstMode << 6) |
                                     (srcMode << 3) | srcReg;

                        /* MOVEA: dest mode 001 */
                        if (dstMode == 1) {
                            /* MOVEA not valid for byte size */
                            if (line == 0) continue;
                            opcodeRegister(opcode, instrMovea, "MOVEA", 4);
                        } else {
                            opcodeRegister(opcode, instrMove, "MOVE", 4);
                        }
                    }
                }
            }
        }
    }

    /* LEA: 0100 rrr1 11ss sSSS (line 4, opmode 111) */
    for (int reg = 0; reg < 8; reg++) {
        for (int srcMode = 2; srcMode < 8; srcMode++) {
            for (int srcReg = 0; srcReg < 8; srcReg++) {
                u16 opcode = 0x41C0 | (reg << 9) | (srcMode << 3) | srcReg;
                opcodeRegister(opcode, instrLea, "LEA", 4);
            }
        }
    }

    /* PEA: 0100 1000 01ss sSSS */
    for (int srcMode = 2; srcMode < 8; srcMode++) {
        for (int srcReg = 0; srcReg < 8; srcReg++) {
            u16 opcode = 0x4840 | (srcMode << 3) | srcReg;
            opcodeRegister(opcode, instrPea, "PEA", 12);
        }
    }

    /* SWAP: 0100 1000 0100 0rrr */
    for (int reg = 0; reg < 8; reg++) {
        opcodeRegister(0x4840 | reg, instrSwap, "SWAP", 4);
    }

    /* EXG: 1100 xxx1 pppp pyyy  (p=opmode) */
    for (int rx = 0; rx < 8; rx++) {
        for (int ry = 0; ry < 8; ry++) {
            opcodeRegister(0xC140 | (rx << 9) | ry, instrExg, "EXG", 6); /* Dn,Dn */
            opcodeRegister(0xC148 | (rx << 9) | ry, instrExg, "EXG", 6); /* An,An */
            opcodeRegister(0xC188 | (rx << 9) | ry, instrExg, "EXG", 6); /* Dn,An */
        }
    }

    /* LINK: 0100 1110 0101 0rrr */
    for (int reg = 0; reg < 8; reg++) {
        opcodeRegister(0x4E50 | reg, instrLink, "LINK", 16);
    }

    /* UNLK: 0100 1110 0101 1rrr */
    for (int reg = 0; reg < 8; reg++) {
        opcodeRegister(0x4E58 | reg, instrUnlk, "UNLK", 12);
    }

    /* MOVEM register-to-memory: 0100 1000 1s mmmrrr */
    for (int sz = 0; sz < 2; sz++) {         /* 0=word, 1=long */
        u16 base = 0x4880 | ((u16)sz << 6);
        /* Predecrement -(An) — mode 4 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (4 << 3) | reg, instrMovemRegToMem, "MOVEM", 8);
        }
        /* (An) — mode 2 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (2 << 3) | reg, instrMovemRegToMem, "MOVEM", 8);
        }
        /* d16(An) — mode 5 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (5 << 3) | reg, instrMovemRegToMem, "MOVEM", 8);
        }
        /* d8(An,Xn) — mode 6 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (6 << 3) | reg, instrMovemRegToMem, "MOVEM", 8);
        }
        /* abs.W — mode 7, reg 0 */
        opcodeRegister(base | (7 << 3) | 0, instrMovemRegToMem, "MOVEM", 8);
        /* abs.L — mode 7, reg 1 */
        opcodeRegister(base | (7 << 3) | 1, instrMovemRegToMem, "MOVEM", 8);
    }

    /* MOVEM memory-to-register: 0100 1100 1s mmmrrr */
    for (int sz = 0; sz < 2; sz++) {
        u16 base = 0x4C80 | ((u16)sz << 6);
        /* Postincrement (An)+ — mode 3 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (3 << 3) | reg, instrMovemMemToReg, "MOVEM", 12);
        }
        /* (An) — mode 2 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (2 << 3) | reg, instrMovemMemToReg, "MOVEM", 12);
        }
        /* d16(An) — mode 5 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (5 << 3) | reg, instrMovemMemToReg, "MOVEM", 12);
        }
        /* d8(An,Xn) — mode 6 */
        for (int reg = 0; reg < 8; reg++) {
            opcodeRegister(base | (6 << 3) | reg, instrMovemMemToReg, "MOVEM", 12);
        }
        /* abs.W — mode 7, reg 0 */
        opcodeRegister(base | (7 << 3) | 0, instrMovemMemToReg, "MOVEM", 12);
        /* abs.L — mode 7, reg 1 */
        opcodeRegister(base | (7 << 3) | 1, instrMovemMemToReg, "MOVEM", 12);
        /* d16(PC) — mode 7, reg 2 */
        opcodeRegister(base | (7 << 3) | 2, instrMovemMemToReg, "MOVEM", 12);
        /* d8(PC,Xn) — mode 7, reg 3 */
        opcodeRegister(base | (7 << 3) | 3, instrMovemMemToReg, "MOVEM", 12);
    }

    /* MOVEP: 0000 ddd mmm 001 aaa  (mmm = 100/101/110/111) */
    for (int dReg = 0; dReg < 8; dReg++) {
        for (int aReg = 0; aReg < 8; aReg++) {
            /* opmode 4: mem→reg word */
            opcodeRegister(0x0108 | (dReg << 9) | aReg, instrMovep, "MOVEP", 16);
            /* opmode 5: mem→reg long */
            opcodeRegister(0x0148 | (dReg << 9) | aReg, instrMovep, "MOVEP", 24);
            /* opmode 6: reg→mem word */
            opcodeRegister(0x0188 | (dReg << 9) | aReg, instrMovep, "MOVEP", 16);
            /* opmode 7: reg→mem long */
            opcodeRegister(0x01C8 | (dReg << 9) | aReg, instrMovep, "MOVEP", 24);
        }
    }
}
