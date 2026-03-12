/* disassembler.c — MC68000 disassembler
 *
 * Decodes raw bytes into Motorola-syntax assembly text.
 * Uses the same opcode grouping as the CPU decoder (bits 15-12).
 * All 16 opcode groups are handled.
 */
#include "disassembler/disassembler.h"
#include <stdio.h>
#include <string.h>

/* ── helpers ─────────────────────────────────────────── */

static u16 readWord(const u8 *p) { return (u16)((p[0] << 8) | p[1]); }
static u32 readLong(const u8 *p) { return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3]; }

static const char *condCodes[16] = {
    "T", "F", "HI","LS","CC","CS","NE","EQ",
    "VC","VS","PL","MI","GE","LT","GT","LE"
};

static const char *sizeChar(int sz) {
    switch (sz) { case 0: return ".B"; case 1: return ".W"; case 2: return ".L"; default: return ""; }
}

/* Format EA operand, advance *pos past extension words. */
static int formatEA(int mode, int reg, int size, const u8 *code, u32 *pos, u32 addr, char *buf) {
    (void)addr;
    char *p = buf;
    switch (mode) {
        case 0: p += sprintf(p, "D%d", reg); break;
        case 1: p += sprintf(p, "A%d", reg); break;
        case 2: p += sprintf(p, "(A%d)", reg); break;
        case 3: p += sprintf(p, "(A%d)+", reg); break;
        case 4: p += sprintf(p, "-(A%d)", reg); break;
        case 5: {
            s16 disp = (s16)readWord(code + *pos); *pos += 2;
            p += sprintf(p, "%d(A%d)", disp, reg);
            break;
        }
        case 6: {
            u16 ext = readWord(code + *pos); *pos += 2;
            int xreg = (ext >> 12) & 7;
            char xtype = (ext & 0x8000) ? 'A' : 'D';
            const char *xsz = (ext & 0x0800) ? ".L" : ".W";
            s8 disp8 = (s8)(ext & 0xFF);
            p += sprintf(p, "%d(A%d,%c%d%s)", disp8, reg, xtype, xreg, xsz);
            break;
        }
        case 7:
            switch (reg) {
                case 0: { u16 w = readWord(code + *pos); *pos += 2; p += sprintf(p, "$%04X.W", w); break; }
                case 1: { u32 l = readLong(code + *pos); *pos += 4; p += sprintf(p, "$%08X.L", l); break; }
                case 2: { s16 d = (s16)readWord(code + *pos); *pos += 2;
                           p += sprintf(p, "%d(PC)", d); break; }
                case 3: { u16 ext = readWord(code + *pos); *pos += 2;
                           int xreg = (ext >> 12) & 7;
                           char xtype = (ext & 0x8000) ? 'A' : 'D';
                           const char *xsz = (ext & 0x0800) ? ".L" : ".W";
                           s8 disp8 = (s8)(ext & 0xFF);
                           p += sprintf(p, "%d(PC,%c%d%s)", disp8, xtype, xreg, xsz);
                           break; }
                case 4: {
                    if (size == 2) {
                        u32 l = readLong(code + *pos); *pos += 4;
                        p += sprintf(p, "#$%08X", l);
                    } else {
                        u16 w = readWord(code + *pos); *pos += 2;
                        p += sprintf(p, "#$%04X", w);
                    }
                    break;
                }
                default: p += sprintf(p, "???"); break;
            }
            break;
        default: p += sprintf(p, "???"); break;
    }
    return (int)(p - buf);
}

/* Format register list mask for MOVEM */
static int formatRegList(u16 mask, char *buf) {
    char *p = buf;
    bool first = true;
    /* D0-D7 = bits 0-7, A0-A7 = bits 8-15 */
    for (int i = 0; i < 16; i++) {
        if (mask & (1 << i)) {
            if (!first) *p++ = '/';
            first = false;
            if (i < 8) p += sprintf(p, "D%d", i);
            else        p += sprintf(p, "A%d", i - 8);
        }
    }
    if (first) p += sprintf(p, "0");
    return (int)(p - buf);
}

/* ── group decoders ──────────────────────────────────── */

/* Group 0: Bit manipulation / MOVEP / Immediate */
static int disasmGroup0(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* MOVEP: 0000 ddd ooo 001 aaa, ooo in {100,101,110,111} */
    if ((opcode & 0x0138) == 0x0108) {
        int dReg = (opcode >> 9) & 7;
        int aReg = opcode & 7;
        int opmode = (opcode >> 6) & 7;
        s16 disp = (s16)readWord(code + *pos); *pos += 2;
        if (opmode >= 6) {
            /* reg → mem */
            const char *sz = (opmode == 7) ? ".L" : ".W";
            p += sprintf(p, "MOVEP%s  D%d,%d(A%d)", sz, dReg, disp, aReg);
        } else {
            /* mem → reg */
            const char *sz = (opmode == 5) ? ".L" : ".W";
            p += sprintf(p, "MOVEP%s  %d(A%d),D%d", sz, disp, aReg, dReg);
        }
        return (int)(p - start);
    }

    /* Dynamic bit ops: 0000 rrr 1tt mmmRRR, tt: 00=BTST, 01=BCHG, 10=BCLR, 11=BSET */
    if (opcode & 0x0100) {
        static const char *bitNames[] = {"BTST","BCHG","BCLR","BSET"};
        int bitOp = (opcode >> 6) & 3;
        int sReg = (opcode >> 9) & 7;
        int dMode = (opcode >> 3) & 7, dReg = opcode & 7;
        p += sprintf(p, "%s    D%d,", bitNames[bitOp], sReg);
        p += formatEA(dMode, dReg, 0, code, pos, addr, p);
        return (int)(p - start);
    }

    /* Static bit ops: 0000 1000 tt mmmRRR */
    if ((opcode & 0x0F00) == 0x0800) {
        static const char *bitNames[] = {"BTST","BCHG","BCLR","BSET"};
        int bitOp = (opcode >> 6) & 3;
        int dMode = (opcode >> 3) & 7, dReg = opcode & 7;
        u16 bitNum = readWord(code + *pos); *pos += 2;
        p += sprintf(p, "%s    #%d,", bitNames[bitOp], bitNum & 0xFF);
        p += formatEA(dMode, dReg, 0, code, pos, addr, p);
        return (int)(p - start);
    }

    /* Immediate operations: ORI, ANDI, SUBI, ADDI, EORI, CMPI */
    {
        int op = (opcode >> 9) & 7;
        int sz = (opcode >> 6) & 3;
        if (sz != 3) {
            static const char *immNames[] = {"ORI","ANDI","SUBI","ADDI","???","EORI","CMPI","???"};
            int dMode = (opcode >> 3) & 7, dReg = opcode & 7;

            /* Check for CCR/SR targets */
            if (dMode == 7 && dReg == 4) {
                /* Shouldn't happen for immediate ops - fallthrough */
            }
            if ((opcode & 0x003F) == 0x003C) {
                /* xxxI #imm,CCR */
                u16 imm = readWord(code + *pos); *pos += 2;
                p += sprintf(p, "%s    #$%02X,CCR", immNames[op], imm & 0xFF);
                return (int)(p - start);
            }
            if ((opcode & 0x003F) == 0x007C) {
                /* xxxI #imm,SR */
                u16 imm = readWord(code + *pos); *pos += 2;
                p += sprintf(p, "%s    #$%04X,SR", immNames[op], imm);
                return (int)(p - start);
            }

            /* Normal immediate op */
            u32 imm;
            if (sz == 2) {
                imm = readLong(code + *pos); *pos += 4;
            } else {
                imm = readWord(code + *pos); *pos += 2;
            }
            p += sprintf(p, "%s%s   #$%X,", immNames[op], sizeChar(sz), imm);
            p += formatEA(dMode, dReg, sz, code, pos, addr, p);
            return (int)(p - start);
        }
    }

    p += sprintf(p, "DC.W    $%04X", opcode);
    return (int)(p - start);
}

/* Group 4: Miscellaneous */
static int disasmGroup4(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* Fixed opcodes */
    if (opcode == 0x4E71) { p += sprintf(p, "NOP"); return (int)(p - start); }
    if (opcode == 0x4E75) { p += sprintf(p, "RTS"); return (int)(p - start); }
    if (opcode == 0x4E73) { p += sprintf(p, "RTE"); return (int)(p - start); }
    if (opcode == 0x4E77) { p += sprintf(p, "RTR"); return (int)(p - start); }
    if (opcode == 0x4E70) { p += sprintf(p, "RESET"); return (int)(p - start); }
    if (opcode == 0x4E76) { p += sprintf(p, "TRAPV"); return (int)(p - start); }
    if (opcode == 0x4AFC) { p += sprintf(p, "ILLEGAL"); return (int)(p - start); }

    /* STOP */
    if (opcode == 0x4E72) {
        u16 imm = readWord(code + *pos); *pos += 2;
        p += sprintf(p, "STOP    #$%04X", imm);
        return (int)(p - start);
    }

    /* TRAP #vector */
    if ((opcode & 0xFFF0) == 0x4E40) {
        p += sprintf(p, "TRAP    #%d", opcode & 0xF);
        return (int)(p - start);
    }

    /* LINK */
    if ((opcode & 0xFFF8) == 0x4E50) {
        s16 d = (s16)readWord(code + *pos); *pos += 2;
        p += sprintf(p, "LINK    A%d,#%d", opcode & 7, d);
        return (int)(p - start);
    }

    /* UNLK */
    if ((opcode & 0xFFF8) == 0x4E58) {
        p += sprintf(p, "UNLK    A%d", opcode & 7);
        return (int)(p - start);
    }

    /* MOVE An,USP */
    if ((opcode & 0xFFF8) == 0x4E60) {
        p += sprintf(p, "MOVE    A%d,USP", opcode & 7);
        return (int)(p - start);
    }

    /* MOVE USP,An */
    if ((opcode & 0xFFF8) == 0x4E68) {
        p += sprintf(p, "MOVE    USP,A%d", opcode & 7);
        return (int)(p - start);
    }

    /* SWAP */
    if ((opcode & 0xFFF8) == 0x4840) {
        p += sprintf(p, "SWAP    D%d", opcode & 7);
        return (int)(p - start);
    }

    /* EXT.W */
    if ((opcode & 0xFFF8) == 0x4880) {
        p += sprintf(p, "EXT.W   D%d", opcode & 7);
        return (int)(p - start);
    }

    /* EXT.L */
    if ((opcode & 0xFFF8) == 0x48C0) {
        p += sprintf(p, "EXT.L   D%d", opcode & 7);
        return (int)(p - start);
    }

    /* MOVEM */
    if ((opcode & 0xFB80) == 0x4880) {
        u16 mask = readWord(code + *pos); *pos += 2;
        int dir = (opcode >> 10) & 1;
        int sz = (opcode >> 6) & 1;
        int eaMode = (opcode >> 3) & 7, eaReg = opcode & 7;
        const char *szStr = sz ? ".L" : ".W";

        /* For predecrement, reverse the mask for display */
        u16 dispMask = mask;
        if (!dir && eaMode == 4) {
            dispMask = 0;
            for (int i = 0; i < 16; i++)
                if (mask & (1 << i)) dispMask |= (1 << (15 - i));
        }

        if (dir) {
            /* Memory to register */
            p += sprintf(p, "MOVEM%s ", szStr);
            p += formatEA(eaMode, eaReg, sz ? 2 : 1, code, pos, addr, p);
            *p++ = ',';
            p += formatRegList(dispMask, p);
        } else {
            /* Register to memory */
            p += sprintf(p, "MOVEM%s ", szStr);
            p += formatRegList(dispMask, p);
            *p++ = ',';
            p += formatEA(eaMode, eaReg, sz ? 2 : 1, code, pos, addr, p);
        }
        return (int)(p - start);
    }

    /* PEA */
    if ((opcode & 0xFFC0) == 0x4840) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "PEA     ");
        p += formatEA(m, r, 2, code, pos, addr, p);
        return (int)(p - start);
    }

    /* LEA */
    if ((opcode & 0xF1C0) == 0x41C0) {
        int dReg = (opcode >> 9) & 7;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "LEA     ");
        p += formatEA(m, r, 2, code, pos, addr, p);
        p += sprintf(p, ",A%d", dReg);
        return (int)(p - start);
    }

    /* CHK */
    if ((opcode & 0xF1C0) == 0x4180) {
        int dReg = (opcode >> 9) & 7;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "CHK.W   ");
        p += formatEA(m, r, 1, code, pos, addr, p);
        p += sprintf(p, ",D%d", dReg);
        return (int)(p - start);
    }

    /* JMP */
    if ((opcode & 0xFFC0) == 0x4EC0) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "JMP     ");
        p += formatEA(m, r, 2, code, pos, addr, p);
        return (int)(p - start);
    }

    /* JSR */
    if ((opcode & 0xFFC0) == 0x4E80) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "JSR     ");
        p += formatEA(m, r, 2, code, pos, addr, p);
        return (int)(p - start);
    }

    /* MOVE from SR: 0100 0000 11 mmmrrr */
    if ((opcode & 0xFFC0) == 0x40C0) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "MOVE    SR,");
        p += formatEA(m, r, 1, code, pos, addr, p);
        return (int)(p - start);
    }

    /* MOVE to CCR: 0100 0100 11 mmmrrr */
    if ((opcode & 0xFFC0) == 0x44C0) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "MOVE    ");
        p += formatEA(m, r, 1, code, pos, addr, p);
        p += sprintf(p, ",CCR");
        return (int)(p - start);
    }

    /* MOVE to SR: 0100 0110 11 mmmrrr */
    if ((opcode & 0xFFC0) == 0x46C0) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "MOVE    ");
        p += formatEA(m, r, 1, code, pos, addr, p);
        p += sprintf(p, ",SR");
        return (int)(p - start);
    }

    /* NBCD */
    if ((opcode & 0xFFC0) == 0x4800) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "NBCD    ");
        p += formatEA(m, r, 0, code, pos, addr, p);
        return (int)(p - start);
    }

    /* TAS */
    if ((opcode & 0xFFC0) == 0x4AC0) {
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "TAS     ");
        p += formatEA(m, r, 0, code, pos, addr, p);
        return (int)(p - start);
    }

    /* TST */
    if ((opcode & 0xFF00) == 0x4A00) {
        int sz = (opcode >> 6) & 3, m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "TST%s    ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    /* CLR */
    if ((opcode & 0xFF00) == 0x4200) {
        int sz = (opcode >> 6) & 3, m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "CLR%s    ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    /* NEG */
    if ((opcode & 0xFF00) == 0x4400) {
        int sz = (opcode >> 6) & 3, m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "NEG%s    ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    /* NEGX */
    if ((opcode & 0xFF00) == 0x4000) {
        int sz = (opcode >> 6) & 3, m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "NEGX%s   ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    /* NOT */
    if ((opcode & 0xFF00) == 0x4600) {
        int sz = (opcode >> 6) & 3, m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "NOT%s    ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    p += sprintf(p, "DC.W    $%04X", opcode);
    return (int)(p - start);
}

/* Group 8: OR / DIV / SBCD */
static int disasmGroup8(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* SBCD */
    if ((opcode & 0x01F0) == 0x0100) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        if (opcode & 8) p += sprintf(p, "SBCD    -(A%d),-(A%d)", ry, rx);
        else            p += sprintf(p, "SBCD    D%d,D%d", ry, rx);
        return (int)(p - start);
    }
    /* DIVU */
    if ((opcode & 0x01C0) == 0x00C0) {
        int r = (opcode >> 9) & 7, m = (opcode >> 3) & 7, sr = opcode & 7;
        p += sprintf(p, "DIVU.W  ");
        p += formatEA(m, sr, 1, code, pos, addr, p);
        p += sprintf(p, ",D%d", r);
        return (int)(p - start);
    }
    /* DIVS */
    if ((opcode & 0x01C0) == 0x01C0) {
        int r = (opcode >> 9) & 7, m = (opcode >> 3) & 7, sr = opcode & 7;
        p += sprintf(p, "DIVS.W  ");
        p += formatEA(m, sr, 1, code, pos, addr, p);
        p += sprintf(p, ",D%d", r);
        return (int)(p - start);
    }
    /* OR */
    {
        int reg = (opcode >> 9) & 7;
        int dir = (opcode >> 8) & 1;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        if (dir) {
            p += sprintf(p, "OR%s    D%d,", sizeChar(sz), reg);
            p += formatEA(m, r, sz, code, pos, addr, p);
        } else {
            p += sprintf(p, "OR%s    ", sizeChar(sz));
            p += formatEA(m, r, sz, code, pos, addr, p);
            p += sprintf(p, ",D%d", reg);
        }
        return (int)(p - start);
    }
}

/* Group 9: SUB / SUBA / SUBX */
static int disasmGroup9(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* SUBX: 1001 xxx1 ss00 xyyy */
    if ((opcode & 0x0130) == 0x0100 && ((opcode >> 6) & 3) != 3) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        int sz = (opcode >> 6) & 3;
        if (opcode & 8)
            p += sprintf(p, "SUBX%s   -(A%d),-(A%d)", sizeChar(sz), ry, rx);
        else
            p += sprintf(p, "SUBX%s   D%d,D%d", sizeChar(sz), ry, rx);
        return (int)(p - start);
    }

    /* SUBA: 1001 rrr o11 mmmRRR */
    if (((opcode >> 6) & 3) == 3) {
        int dReg = (opcode >> 9) & 7;
        int sz = (opcode & 0x0100) ? 2 : 1;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "SUBA%s   ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        p += sprintf(p, ",A%d", dReg);
        return (int)(p - start);
    }

    /* SUB */
    {
        int reg = (opcode >> 9) & 7;
        int dir = (opcode >> 8) & 1;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        if (dir) {
            p += sprintf(p, "SUB%s    D%d,", sizeChar(sz), reg);
            p += formatEA(m, r, sz, code, pos, addr, p);
        } else {
            p += sprintf(p, "SUB%s    ", sizeChar(sz));
            p += formatEA(m, r, sz, code, pos, addr, p);
            p += sprintf(p, ",D%d", reg);
        }
        return (int)(p - start);
    }
}

/* Group 0xB: CMP / CMPA / EOR / CMPM */
static int disasmGroupB(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* CMPM: 1011 yyy1 ss001 xxx */
    if ((opcode & 0x0138) == 0x0108) {
        int ax = opcode & 7, ay = (opcode >> 9) & 7;
        int sz = (opcode >> 6) & 3;
        p += sprintf(p, "CMPM%s   (A%d)+,(A%d)+", sizeChar(sz), ax, ay);
        return (int)(p - start);
    }

    /* CMPA: 1011 rrr o11 mmmRRR */
    if (((opcode >> 6) & 3) == 3) {
        int dReg = (opcode >> 9) & 7;
        int sz = (opcode & 0x0100) ? 2 : 1;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "CMPA%s   ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        p += sprintf(p, ",A%d", dReg);
        return (int)(p - start);
    }

    /* EOR: 1011 rrr1 ss mmmRRR (direction=1, not CMPM) */
    if (opcode & 0x0100) {
        int reg = (opcode >> 9) & 7;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "EOR%s    D%d,", sizeChar(sz), reg);
        p += formatEA(m, r, sz, code, pos, addr, p);
        return (int)(p - start);
    }

    /* CMP: 1011 rrr0 ss mmmRRR */
    {
        int reg = (opcode >> 9) & 7;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "CMP%s    ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        p += sprintf(p, ",D%d", reg);
        return (int)(p - start);
    }
}

/* Group 0xC: AND / MUL / ABCD / EXG */
static int disasmGroupC(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* ABCD */
    if ((opcode & 0x01F0) == 0x0100) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        if (opcode & 8) p += sprintf(p, "ABCD    -(A%d),-(A%d)", ry, rx);
        else            p += sprintf(p, "ABCD    D%d,D%d", ry, rx);
        return (int)(p - start);
    }

    /* EXG */
    if ((opcode & 0x01F8) == 0x0140) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        p += sprintf(p, "EXG     D%d,D%d", rx, ry);
        return (int)(p - start);
    }
    if ((opcode & 0x01F8) == 0x0148) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        p += sprintf(p, "EXG     A%d,A%d", rx, ry);
        return (int)(p - start);
    }
    if ((opcode & 0x01F8) == 0x0188) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        p += sprintf(p, "EXG     D%d,A%d", rx, ry);
        return (int)(p - start);
    }

    /* MULU */
    if ((opcode & 0x01C0) == 0x00C0) {
        int r = (opcode >> 9) & 7, m = (opcode >> 3) & 7, sr = opcode & 7;
        p += sprintf(p, "MULU.W  ");
        p += formatEA(m, sr, 1, code, pos, addr, p);
        p += sprintf(p, ",D%d", r);
        return (int)(p - start);
    }

    /* MULS */
    if ((opcode & 0x01C0) == 0x01C0) {
        int r = (opcode >> 9) & 7, m = (opcode >> 3) & 7, sr = opcode & 7;
        p += sprintf(p, "MULS.W  ");
        p += formatEA(m, sr, 1, code, pos, addr, p);
        p += sprintf(p, ",D%d", r);
        return (int)(p - start);
    }

    /* AND */
    {
        int reg = (opcode >> 9) & 7;
        int dir = (opcode >> 8) & 1;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        if (dir) {
            p += sprintf(p, "AND%s    D%d,", sizeChar(sz), reg);
            p += formatEA(m, r, sz, code, pos, addr, p);
        } else {
            p += sprintf(p, "AND%s    ", sizeChar(sz));
            p += formatEA(m, r, sz, code, pos, addr, p);
            p += sprintf(p, ",D%d", reg);
        }
        return (int)(p - start);
    }
}

/* Group 0xD: ADD / ADDA / ADDX */
static int disasmGroupD(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;

    /* ADDX */
    if ((opcode & 0x0130) == 0x0100 && ((opcode >> 6) & 3) != 3) {
        int rx = (opcode >> 9) & 7, ry = opcode & 7;
        int sz = (opcode >> 6) & 3;
        if (opcode & 8)
            p += sprintf(p, "ADDX%s   -(A%d),-(A%d)", sizeChar(sz), ry, rx);
        else
            p += sprintf(p, "ADDX%s   D%d,D%d", sizeChar(sz), ry, rx);
        return (int)(p - start);
    }

    /* ADDA */
    if (((opcode >> 6) & 3) == 3) {
        int dReg = (opcode >> 9) & 7;
        int sz = (opcode & 0x0100) ? 2 : 1;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        p += sprintf(p, "ADDA%s   ", sizeChar(sz));
        p += formatEA(m, r, sz, code, pos, addr, p);
        p += sprintf(p, ",A%d", dReg);
        return (int)(p - start);
    }

    /* ADD */
    {
        int reg = (opcode >> 9) & 7;
        int dir = (opcode >> 8) & 1;
        int sz = (opcode >> 6) & 3;
        int m = (opcode >> 3) & 7, r = opcode & 7;
        if (dir) {
            p += sprintf(p, "ADD%s    D%d,", sizeChar(sz), reg);
            p += formatEA(m, r, sz, code, pos, addr, p);
        } else {
            p += sprintf(p, "ADD%s    ", sizeChar(sz));
            p += formatEA(m, r, sz, code, pos, addr, p);
            p += sprintf(p, ",D%d", reg);
        }
        return (int)(p - start);
    }
}

/* Group 0xE: Shift / Rotate */
static int disasmGroupE(u16 opcode, const u8 *code, u32 *pos, u32 addr, char *p) {
    char *start = p;
    int count = (opcode >> 9) & 7;
    int dr    = (opcode >> 8) & 1;
    int sz    = (opcode >> 6) & 3;
    int ir    = (opcode >> 5) & 1;
    int type  = (opcode >> 3) & 3;
    int reg   = opcode & 7;
    static const char *names[] = {"AS","LS","ROX","RO"};
    const char *dir = dr ? "L" : "R";

    if (sz == 3) {
        /* Memory shift */
        int m = (opcode >> 3) & 7, r = opcode & 7;
        type = (opcode >> 9) & 3;
        dir = ((opcode >> 8) & 1) ? "L" : "R";
        p += sprintf(p, "%s%s.W   ", names[type], dir);
        p += formatEA(m, r, 1, code, pos, addr, p);
    } else {
        if (ir)
            p += sprintf(p, "%s%s%s   D%d,D%d", names[type], dir, sizeChar(sz), count, reg);
        else
            p += sprintf(p, "%s%s%s   #%d,D%d", names[type], dir, sizeChar(sz),
                         count ? count : 8, reg);
    }
    return (int)(p - start);
}

/* ── main disassembler ───────────────────────────────── */

u32 disasmInstruction(const u8 *code, u32 address, char *outBuf, int outBufSize) {
    u16 opcode = readWord(code);
    u32 pos = 2;
    char tmp[256];
    tmp[0] = '\0';
    char *p = tmp;

    int group = (opcode >> 12) & 0xF;

    switch (group) {
    case 0x0:
        p += disasmGroup0(opcode, code, &pos, address, p);
        break;
    case 0x1: case 0x2: case 0x3: {
        /* MOVE / MOVEA */
        int dReg  = (opcode >> 9) & 7;
        int dMode = (opcode >> 6) & 7;
        int sMode = (opcode >> 3) & 7;
        int sReg  = opcode & 7;
        int sz = (group == 1) ? 0 : (group == 3) ? 1 : 2;
        if (dMode == 1)
            p += sprintf(p, "MOVEA%s  ", sizeChar(sz));
        else
            p += sprintf(p, "MOVE%s   ", sizeChar(sz));
        p += formatEA(sMode, sReg, sz, code, &pos, address, p);
        *p++ = ',';
        p += formatEA(dMode, dReg, sz, code, &pos, address, p);
        break;
    }
    case 0x4:
        p += disasmGroup4(opcode, code, &pos, address, p);
        break;
    case 0x5: {
        if ((opcode & 0x00F8) == 0x00C8) {
            /* DBcc */
            int cond = (opcode >> 8) & 0xF;
            int reg  = opcode & 7;
            s16 disp = (s16)readWord(code + pos); pos += 2;
            p += sprintf(p, "DB%s     D%d,$%06X", condCodes[cond], reg,
                         (u32)(address + 2 + disp));
        } else if ((opcode & 0x00C0) == 0x00C0) {
            /* Scc */
            int cond = (opcode >> 8) & 0xF;
            int m = (opcode >> 3) & 7, r = opcode & 7;
            p += sprintf(p, "S%s      ", condCodes[cond]);
            p += formatEA(m, r, 0, code, &pos, address, p);
        } else {
            /* ADDQ / SUBQ */
            int data = (opcode >> 9) & 7; if (data == 0) data = 8;
            int sz = (opcode >> 6) & 3;
            int m = (opcode >> 3) & 7, r = opcode & 7;
            p += sprintf(p, "%s%s  #%d,", (opcode & 0x0100) ? "SUBQ" : "ADDQ",
                         sizeChar(sz), data);
            p += formatEA(m, r, sz, code, &pos, address, p);
        }
        break;
    }
    case 0x6: {
        /* Bcc / BRA / BSR */
        int cond = (opcode >> 8) & 0xF;
        s32 disp = (s8)(opcode & 0xFF);
        if (disp == 0) { disp = (s16)readWord(code + pos); pos += 2; }
        u32 target = (u32)(address + 2 + disp);
        if (cond == 0)       p += sprintf(p, "BRA     $%06X", target);
        else if (cond == 1)  p += sprintf(p, "BSR     $%06X", target);
        else                 p += sprintf(p, "B%s     $%06X", condCodes[cond], target);
        break;
    }
    case 0x7: {
        /* MOVEQ */
        int reg = (opcode >> 9) & 7;
        s8 data = (s8)(opcode & 0xFF);
        p += sprintf(p, "MOVEQ   #%d,D%d", data, reg);
        break;
    }
    case 0x8:
        p += disasmGroup8(opcode, code, &pos, address, p);
        break;
    case 0x9:
        p += disasmGroup9(opcode, code, &pos, address, p);
        break;
    case 0xA:
        p += sprintf(p, "DC.W    $%04X  ; Line-A", opcode);
        break;
    case 0xB:
        p += disasmGroupB(opcode, code, &pos, address, p);
        break;
    case 0xC:
        p += disasmGroupC(opcode, code, &pos, address, p);
        break;
    case 0xD:
        p += disasmGroupD(opcode, code, &pos, address, p);
        break;
    case 0xE:
        p += disasmGroupE(opcode, code, &pos, address, p);
        break;
    case 0xF:
        p += sprintf(p, "DC.W    $%04X  ; Line-F", opcode);
        break;
    default:
        p += sprintf(p, "DC.W    $%04X", opcode);
        break;
    }

    *p = '\0';
    snprintf(outBuf, (size_t)outBufSize, "%s", tmp);
    return pos;
}

/* ── range disassembly ───────────────────────────────── */

bool disasmRange(const u8 *code, u32 baseAddress, u32 length, const char *outPath) {
    FILE *f = outPath ? fopen(outPath, "w") : stdout;
    if (!f) return false;

    u32 offset = 0;
    while (offset < length) {
        char line[256];
        u32 consumed = disasmInstruction(code + offset, baseAddress + offset, line, sizeof(line));
        fprintf(f, "%06X  ", baseAddress + offset);
        for (u32 i = 0; i < consumed && i < 10; i += 2)
            fprintf(f, "%04X ", readWord(code + offset + i));
        for (u32 i = consumed; i < 10; i += 2)
            fprintf(f, "     ");
        fprintf(f, " %s\n", line);
        offset += consumed;
    }

    if (outPath) fclose(f);
    return true;
}

void disasmFormatHex(const u8 *code, u32 length, char *outBuf, int outBufSize) {
    int p = 0;
    for (u32 i = 0; i < length && p < outBufSize - 3; i++) {
        p += snprintf(outBuf + p, (size_t)(outBufSize - p), "%02X", code[i]);
    }
}
