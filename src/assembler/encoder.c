/* encoder.c — Instruction encoder (assembly → machine code)
 *
 * Encodes MC68000 instructions from parsed AsmLine structs into
 * binary opcode words + extension words.
 *
 * Every MC68000 mnemonic is handled.  The encoder emits big-endian
 * 16-bit words via bufferWriteU16BE.
 */
#include "assembler/encoder.h"
#include "assembler/directives.h"
#include <string.h>
#include <ctype.h>

/* ── EA mode/reg encoding helper ─────────────────────── */

static bool encodeEA(const AsmOperand *op, u8 *modeOut, u8 *regOut,
                     u16 *extWords, int *extCount) {
    *extCount = 0;
    switch (op->type) {
        case OPER_DATA_REG:      *modeOut = 0; *regOut = (u8)op->reg; return true;
        case OPER_ADDR_REG:      *modeOut = 1; *regOut = (u8)op->reg; return true;
        case OPER_ADDR_IND:      *modeOut = 2; *regOut = (u8)op->reg; return true;
        case OPER_ADDR_IND_POST: *modeOut = 3; *regOut = (u8)op->reg; return true;
        case OPER_ADDR_IND_PRE:  *modeOut = 4; *regOut = (u8)op->reg; return true;
        case OPER_ADDR_DISP:
            *modeOut = 5; *regOut = (u8)op->reg;
            extWords[0] = (u16)op->displacement; *extCount = 1;
            return true;
        case OPER_ADDR_INDEX:
            *modeOut = 6; *regOut = (u8)op->reg;
            extWords[0] = (u16)(((op->indexIsAddr ? 1 : 0) << 15) |
                                (op->indexReg << 12) |
                                ((op->indexIsLong ? 1 : 0) << 11) |
                                (op->displacement & 0xFF));
            *extCount = 1;
            return true;
        case OPER_ABS_SHORT:
            *modeOut = 7; *regOut = 0;
            extWords[0] = (u16)op->immediate; *extCount = 1;
            return true;
        case OPER_ABS_LONG:
            *modeOut = 7; *regOut = 1;
            extWords[0] = (u16)(op->immediate >> 16);
            extWords[1] = (u16)(op->immediate);
            *extCount = 2;
            return true;
        case OPER_PC_DISP:
            *modeOut = 7; *regOut = 2;
            extWords[0] = (u16)op->displacement; *extCount = 1;
            return true;
        case OPER_PC_INDEX:
            *modeOut = 7; *regOut = 3;
            extWords[0] = (u16)(((op->indexIsAddr ? 1 : 0) << 15) |
                                (op->indexReg << 12) |
                                ((op->indexIsLong ? 1 : 0) << 11) |
                                (op->displacement & 0xFF));
            *extCount = 1;
            return true;
        case OPER_IMMEDIATE:
            *modeOut = 7; *regOut = 4;
            return true;
        case OPER_EXPRESSION:
            /* Treat unresolved expression as absolute long */
            *modeOut = 7; *regOut = 1;
            extWords[0] = (u16)(op->immediate >> 16);
            extWords[1] = (u16)(op->immediate);
            *extCount = 2;
            return true;
        default: return false;
    }
}

/* Emit EA extension words (not including immediate data) */
static u32 emitEAExtWords(Buffer *out, const u16 *ext, int extCount) {
    u32 bytes = 0;
    for (int i = 0; i < extCount; i++) {
        if (out) bufferWriteU16BE(out, ext[i]);
        bytes += 2;
    }
    return bytes;
}

/* Emit immediate data according to size */
static u32 emitImmediate(Buffer *out, s64 value, u8 size) {
    if (size == 4) { /* long */
        if (out) {
            bufferWriteU16BE(out, (u16)(value >> 16));
            bufferWriteU16BE(out, (u16)value);
        }
        return 4;
    } else { /* byte or word */
        if (out) bufferWriteU16BE(out, (u16)value);
        return 2;
    }
}

/* ── size field helpers ─────────────────────────────── */

/* Standard size encoding: 00=byte, 01=word, 10=long (bits 7-6) */
static u16 sizeField76(u8 sz) {
    switch (sz) {
        case 1: return 0;   /* byte */
        case 4: return 2;   /* long */
        default: return 1;  /* word */
    }
}

/* ── Condition code name → 4-bit code ────────────────── */

static int condCode(const char *name) {
    static const struct { const char *n; int c; } tbl[] = {
        {"T",0},{"F",1},{"HI",2},{"LS",3},{"CC",4},{"CS",5},
        {"NE",6},{"EQ",7},{"VC",8},{"VS",9},{"PL",10},{"MI",11},
        {"GE",12},{"LT",13},{"GT",14},{"LE",15},
        {"HS",4},{"LO",5},
        {NULL,0}
    };
    for (int i = 0; tbl[i].n; i++) {
        if (strcmp(name, tbl[i].n) == 0) return tbl[i].c;
    }
    return -1;
}

/* ═══════════════════════════════════════════════════════
 *  Per-mnemonic encoders
 * ═══════════════════════════════════════════════════════ */

/* ── MOVE / MOVEA ────────────────────────────────────── */
static u32 encodeMove(const AsmLine *line, SymbolTable *sym, Buffer *out, u32 pc) {
    (void)sym; (void)pc;
    if (line->operandCount < 2) return 0;

    u8 srcMode, srcReg, dstMode, dstReg;
    u16 srcExt[4], dstExt[4];
    int srcExtN = 0, dstExtN = 0;

    encodeEA(&line->operands[0], &srcMode, &srcReg, srcExt, &srcExtN);
    encodeEA(&line->operands[1], &dstMode, &dstReg, dstExt, &dstExtN);

    u16 sz;
    switch (line->size) {
        case 1:  sz = 0x1000; break;
        case 4:  sz = 0x2000; break;
        default: sz = 0x3000; break;
    }

    u16 opcode = sz | ((u16)dstReg << 9) | ((u16)dstMode << 6) |
                 ((u16)srcMode << 3) | srcReg;

    if (out) bufferWriteU16BE(out, opcode);
    u32 total = 2;

    total += emitEAExtWords(out, srcExt, srcExtN);
    if (line->operands[0].type == OPER_IMMEDIATE)
        total += emitImmediate(out, line->operands[0].immediate, line->size);
    total += emitEAExtWords(out, dstExt, dstExtN);
    return total;
}

/* ── MOVEQ ───────────────────────────────────────────── */
static u32 encodeMoveq(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    int reg = line->operands[1].reg;
    u8 data = (u8)(line->operands[0].immediate & 0xFF);
    u16 opcode = 0x7000 | ((u16)reg << 9) | data;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── LEA ─────────────────────────────────────────────── */
static u32 encodeLea(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    int dReg = line->operands[1].reg;
    u16 opcode = 0x41C0 | ((u16)dReg << 9) | ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── PEA ─────────────────────────────────────────────── */
static u32 encodePea(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    u16 opcode = 0x4840 | ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── Generic ALU: ADD/SUB/AND/OR/CMP ─────────────────── */
static u32 encodeAluReg(const AsmLine *line, Buffer *out, u16 lineCode) {
    if (line->operandCount < 2) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    u16 sz = sizeField76(line->size);

    if (line->operands[1].type == OPER_DATA_REG) {
        /* <ea>,Dn  → direction = 0 */
        encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
        int dReg = line->operands[1].reg;
        u16 opcode = lineCode | ((u16)dReg << 9) | (sz << 6) |
                     ((u16)sMode << 3) | sReg;
        if (out) bufferWriteU16BE(out, opcode);
        u32 total = 2 + emitEAExtWords(out, ext, extN);
        if (line->operands[0].type == OPER_IMMEDIATE)
            total += emitImmediate(out, line->operands[0].immediate, line->size);
        return total;
    } else {
        /* Dn,<ea>  → direction = 1 */
        int sRegD = line->operands[0].reg;
        encodeEA(&line->operands[1], &sMode, &sReg, ext, &extN);
        u16 opcode = lineCode | ((u16)sRegD << 9) | (1 << 8) | (sz << 6) |
                     ((u16)sMode << 3) | sReg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2 + emitEAExtWords(out, ext, extN);
    }
}

/* ── EOR (always Dn,<ea>) ───────────────────────────── */
static u32 encodeEor(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    u16 sz = sizeField76(line->size);
    int sRegD = line->operands[0].reg;
    encodeEA(&line->operands[1], &dMode, &dReg, ext, &extN);
    u16 opcode = 0xB000 | ((u16)sRegD << 9) | (1 << 8) | (sz << 6) |
                 ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ADDA/SUBA/CMPA: opmode 011=word, 111=long */
static u32 encodeAluAddr(const AsmLine *line, Buffer *out, u16 lineCode) {
    if (line->operandCount < 2) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    int dReg = line->operands[1].reg;
    u16 opmode = (line->size == 4) ? 7 : 3;
    u16 opcode = lineCode | ((u16)dReg << 9) | (opmode << 6) |
                 ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    u32 total = 2 + emitEAExtWords(out, ext, extN);
    if (line->operands[0].type == OPER_IMMEDIATE)
        total += emitImmediate(out, line->operands[0].immediate, line->size);
    return total;
}

/* ADDI/SUBI/CMPI/ANDI/ORI/EORI: 0000 XXXX ss mmmrrr */
static u32 encodeAluImm(const AsmLine *line, Buffer *out, u16 baseOpcode) {
    if (line->operandCount < 2) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[1], &dMode, &dReg, ext, &extN);
    u16 sz = sizeField76(line->size);
    u16 opcode = baseOpcode | (sz << 6) | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    u32 total = 2;
    total += emitImmediate(out, line->operands[0].immediate, line->size);
    total += emitEAExtWords(out, ext, extN);
    return total;
}

/* ADDQ/SUBQ */
static u32 encodeQuick(const AsmLine *line, Buffer *out, bool isSub) {
    if (line->operandCount < 2) return 0;
    int data = (int)(line->operands[0].immediate & 7);
    if (line->operands[0].immediate == 8) data = 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[1], &dMode, &dReg, ext, &extN);
    u16 sz = sizeField76(line->size);
    u16 opcode = 0x5000 | ((u16)data << 9) | (isSub ? 0x0100 : 0) |
                 (sz << 6) | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ADDX/SUBX */
static u32 encodeXOp(const AsmLine *line, Buffer *out, u16 lineCode) {
    if (line->operandCount < 2) return 0;
    u16 sz = sizeField76(line->size);
    int rx, ry;
    u16 rm = 0;
    if (line->operands[0].type == OPER_ADDR_IND_PRE) {
        rm = 1;
        ry = line->operands[0].reg;
        rx = line->operands[1].reg;
    } else {
        ry = line->operands[0].reg;
        rx = line->operands[1].reg;
    }
    u16 opcode = lineCode | ((u16)rx << 9) | (1 << 8) | (sz << 6) |
                 (rm << 3) | (u16)ry;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── Bcc / BRA / BSR ────────────────────────────────── */
static u32 encodeBranch(const AsmLine *line, Buffer *out, u32 pc, int cond) {
    if (line->operandCount < 1) return 0;
    s32 target = (s32)line->operands[0].immediate;
    s32 disp = target - (s32)(pc + 2);

    /* Explicit .S suffix → always 2 bytes (short branch) */
    if (line->size == 1) {
        u16 opcode = 0x6000 | ((u16)cond << 8) | ((u16)(u8)disp);
        if (out) bufferWriteU16BE(out, opcode);
        return 2;
    }

    /* Explicit .W suffix or default → always 4 bytes (word branch) */
    u16 opcode = 0x6000 | ((u16)cond << 8);
    if (out) {
        bufferWriteU16BE(out, opcode);
        bufferWriteU16BE(out, (u16)disp);
    }
    return 4;
}

/* ── DBcc ────────────────────────────────────────────── */
static u32 encodeDBcc(const AsmLine *line, Buffer *out, u32 pc, int cond) {
    if (line->operandCount < 2) return 0;
    int reg = line->operands[0].reg;
    s32 target = (s32)line->operands[1].immediate;
    s16 disp = (s16)(target - (s32)(pc + 2));
    u16 opcode = 0x50C8 | ((u16)cond << 8) | (u16)reg;
    if (out) {
        bufferWriteU16BE(out, opcode);
        bufferWriteU16BE(out, (u16)disp);
    }
    return 4;
}

/* ── Scc ─────────────────────────────────────────────── */
static u32 encodeScc(const AsmLine *line, Buffer *out, int cond) {
    if (line->operandCount < 1) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &dMode, &dReg, ext, &extN);
    u16 opcode = 0x50C0 | ((u16)cond << 8) | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── JMP / JSR ───────────────────────────────────────── */
static u32 encodeJmpJsr(const AsmLine *line, Buffer *out, bool isJsr) {
    if (line->operandCount < 1) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    u16 base = isJsr ? 0x4E80 : 0x4EC0;
    u16 opcode = base | ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── CLR / NEG / NEGX / NOT / TST ────────────────────── */
static u32 encodeUnaryEA(const AsmLine *line, Buffer *out, u16 baseOpcode) {
    if (line->operandCount < 1) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &dMode, &dReg, ext, &extN);
    u16 sz = sizeField76(line->size);
    u16 opcode = baseOpcode | (sz << 6) | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── EXT ─────────────────────────────────────────────── */
static u32 encodeExt(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    int reg = line->operands[0].reg;
    u16 opmode = (line->size == 4) ? 3 : 2;
    u16 opcode = 0x4800 | (opmode << 6) | (u16)reg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── SWAP ────────────────────────────────────────────── */
static u32 encodeSwap(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u16 opcode = 0x4840 | (u16)line->operands[0].reg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── EXG ─────────────────────────────────────────────── */
static u32 encodeExg(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    bool src_is_a = (line->operands[0].type == OPER_ADDR_REG);
    bool dst_is_a = (line->operands[1].type == OPER_ADDR_REG);
    int rx = line->operands[0].reg;
    int ry = line->operands[1].reg;
    u16 opmode;
    if (!src_is_a && !dst_is_a)      opmode = 0x40;
    else if (src_is_a && dst_is_a)   opmode = 0x48;
    else                             opmode = 0x88;
    u16 opcode = 0xC100 | ((u16)rx << 9) | opmode | (u16)ry;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── LINK ────────────────────────────────────────────── */
static u32 encodeLink(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    int reg = line->operands[0].reg;
    s16 disp = (s16)line->operands[1].immediate;
    u16 opcode = 0x4E50 | (u16)reg;
    if (out) {
        bufferWriteU16BE(out, opcode);
        bufferWriteU16BE(out, (u16)disp);
    }
    return 4;
}

/* ── UNLK ────────────────────────────────────────────── */
static u32 encodeUnlk(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u16 opcode = 0x4E58 | (u16)line->operands[0].reg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── TRAP ────────────────────────────────────────────── */
static u32 encodeTrap(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u16 opcode = 0x4E40 | (u16)(line->operands[0].immediate & 0xF);
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── MULU / MULS / DIVU / DIVS ──────────────────────── */
static u32 encodeMulDiv(const AsmLine *line, Buffer *out, u16 baseOpcode) {
    if (line->operandCount < 2) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    int dReg = line->operands[1].reg;
    u16 opcode = baseOpcode | ((u16)dReg << 9) | ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    u32 total = 2 + emitEAExtWords(out, ext, extN);
    if (line->operands[0].type == OPER_IMMEDIATE)
        total += emitImmediate(out, line->operands[0].immediate, 2);
    return total;
}

/* ── Shift / Rotate ──────────────────────────────────── */
static u32 encodeShift(const AsmLine *line, Buffer *out,
                       int type, int direction) {
    if (line->operandCount < 1) return 0;

    if (line->operandCount == 1) {
        /* Memory shift: size is always word */
        u8 dMode, dReg;
        u16 ext[4]; int extN = 0;
        encodeEA(&line->operands[0], &dMode, &dReg, ext, &extN);
        u16 opcode = 0xE000 | ((u16)type << 9) | ((u16)direction << 8) |
                     (3 << 6) | ((u16)dMode << 3) | dReg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2 + emitEAExtWords(out, ext, extN);
    } else {
        u16 sz = sizeField76(line->size);
        int dstReg = line->operands[1].reg;
        u16 ir;
        int count;
        if (line->operands[0].type == OPER_DATA_REG) {
            ir = 1;
            count = line->operands[0].reg;
        } else {
            ir = 0;
            count = (int)(line->operands[0].immediate & 7);
            if (line->operands[0].immediate == 8) count = 0;
        }
        u16 opcode = 0xE000 | ((u16)count << 9) | ((u16)direction << 8) |
                     (sz << 6) | (ir << 5) | ((u16)type << 3) | (u16)dstReg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2;
    }
}

/* ── BTST / BCHG / BCLR / BSET ──────────────────────── */
static u32 encodeBitOp(const AsmLine *line, Buffer *out, int bitOp) {
    if (line->operandCount < 2) return 0;

    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[1], &dMode, &dReg, ext, &extN);

    if (line->operands[0].type == OPER_DATA_REG) {
        int sReg = line->operands[0].reg;
        u16 opcode = 0x0100 | ((u16)sReg << 9) | ((u16)bitOp << 6) |
                     ((u16)dMode << 3) | dReg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2 + emitEAExtWords(out, ext, extN);
    } else {
        u16 opcode = 0x0800 | ((u16)bitOp << 6) | ((u16)dMode << 3) | dReg;
        if (out) {
            bufferWriteU16BE(out, opcode);
            bufferWriteU16BE(out, (u16)(line->operands[0].immediate & 0xFF));
        }
        return 4 + emitEAExtWords(out, ext, extN);
    }
}

/* ── ABCD / SBCD ─────────────────────────────────────── */
static u32 encodeBcd(const AsmLine *line, Buffer *out, bool isSbcd) {
    if (line->operandCount < 2) return 0;
    int ry = line->operands[0].reg;
    int rx = line->operands[1].reg;
    u16 rm = (line->operands[0].type == OPER_ADDR_IND_PRE) ? 8 : 0;
    u16 base = isSbcd ? 0x8100 : 0xC100;
    u16 opcode = base | ((u16)rx << 9) | rm | (u16)ry;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ── NBCD ────────────────────────────────────────────── */
static u32 encodeNbcd(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &dMode, &dReg, ext, &extN);
    u16 opcode = 0x4800 | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── CHK ─────────────────────────────────────────────── */
static u32 encodeChk(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    u8 sMode, sReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
    int dReg = line->operands[1].reg;
    u16 opcode = 0x4180 | ((u16)dReg << 9) | ((u16)sMode << 3) | sReg;
    if (out) bufferWriteU16BE(out, opcode);
    u32 total = 2 + emitEAExtWords(out, ext, extN);
    if (line->operands[0].type == OPER_IMMEDIATE)
        total += emitImmediate(out, line->operands[0].immediate, 2);
    return total;
}

/* ── TAS ─────────────────────────────────────────────── */
static u32 encodeTas(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 1) return 0;
    u8 dMode, dReg;
    u16 ext[4]; int extN = 0;
    encodeEA(&line->operands[0], &dMode, &dReg, ext, &extN);
    u16 opcode = 0x4AC0 | ((u16)dMode << 3) | dReg;
    if (out) bufferWriteU16BE(out, opcode);
    return 2 + emitEAExtWords(out, ext, extN);
}

/* ── MOVEM ───────────────────────────────────────────── */
static u16 regOperandToMask(const AsmOperand *op) {
    if (op->type == OPER_REG_LIST) return op->regListMask;
    if (op->type == OPER_DATA_REG) return (u16)(1 << op->reg);
    if (op->type == OPER_ADDR_REG) return (u16)(1 << (op->reg + 8));
    return 0;
}

static bool isRegOrRegList(const AsmOperand *op) {
    return op->type == OPER_REG_LIST ||
           op->type == OPER_DATA_REG ||
           op->type == OPER_ADDR_REG;
}

static u32 encodeMovem(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;

    u16 mask;
    u8 eaMode, eaReg;
    u16 ext[4]; int extN = 0;
    u16 dir;
    u16 sz = (line->size == 4) ? 1 : 0;

    if (isRegOrRegList(&line->operands[0])) {
        dir = 0;
        mask = regOperandToMask(&line->operands[0]);
        encodeEA(&line->operands[1], &eaMode, &eaReg, ext, &extN);
        if (eaMode == 4) {
            u16 rev = 0;
            for (int i = 0; i < 16; i++) {
                if (mask & (1 << i)) rev |= (1 << (15 - i));
            }
            mask = rev;
        }
    } else {
        dir = 1;
        mask = regOperandToMask(&line->operands[1]);
        encodeEA(&line->operands[0], &eaMode, &eaReg, ext, &extN);
    }

    u16 opcode = 0x4880 | ((u16)dir << 10) | ((u16)sz << 6) |
                 ((u16)eaMode << 3) | eaReg;
    if (out) {
        bufferWriteU16BE(out, opcode);
        bufferWriteU16BE(out, mask);
    }
    return 4 + emitEAExtWords(out, ext, extN);
}

/* ── MOVEP ───────────────────────────────────────────── */
static u32 encodeMovep(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    u16 opmode;
    int dReg, aReg;
    s16 disp;

    if (line->operands[0].type == OPER_DATA_REG) {
        dReg = line->operands[0].reg;
        aReg = line->operands[1].reg;
        disp = (s16)line->operands[1].displacement;
        opmode = (line->size == 4) ? 7 : 6;
    } else {
        aReg = line->operands[0].reg;
        disp = (s16)line->operands[0].displacement;
        dReg = line->operands[1].reg;
        opmode = (line->size == 4) ? 5 : 4;
    }

    u16 opcode = ((u16)dReg << 9) | ((u16)opmode << 6) | 0x0008 | (u16)aReg;
    if (out) {
        bufferWriteU16BE(out, opcode);
        bufferWriteU16BE(out, (u16)disp);
    }
    return 4;
}

/* ── MOVE to/from SR/CCR/USP ─────────────────────────── */
static u32 encodeMoveSR(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;

    if (line->operands[1].type == OPER_SR) {
        u8 sMode, sReg;
        u16 ext[4]; int extN = 0;
        encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
        u16 opcode = 0x46C0 | ((u16)sMode << 3) | sReg;
        if (out) bufferWriteU16BE(out, opcode);
        u32 total = 2 + emitEAExtWords(out, ext, extN);
        if (line->operands[0].type == OPER_IMMEDIATE)
            total += emitImmediate(out, line->operands[0].immediate, 2);
        return total;
    } else if (line->operands[0].type == OPER_SR) {
        u8 dMode, dReg;
        u16 ext[4]; int extN = 0;
        encodeEA(&line->operands[1], &dMode, &dReg, ext, &extN);
        u16 opcode = 0x40C0 | ((u16)dMode << 3) | dReg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2 + emitEAExtWords(out, ext, extN);
    } else if (line->operands[1].type == OPER_CCR) {
        u8 sMode, sReg;
        u16 ext[4]; int extN = 0;
        encodeEA(&line->operands[0], &sMode, &sReg, ext, &extN);
        u16 opcode = 0x44C0 | ((u16)sMode << 3) | sReg;
        if (out) bufferWriteU16BE(out, opcode);
        u32 total = 2 + emitEAExtWords(out, ext, extN);
        if (line->operands[0].type == OPER_IMMEDIATE)
            total += emitImmediate(out, line->operands[0].immediate, 2);
        return total;
    } else if (line->operands[1].type == OPER_USP) {
        u16 opcode = 0x4E60 | (u16)line->operands[0].reg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2;
    } else if (line->operands[0].type == OPER_USP) {
        u16 opcode = 0x4E68 | (u16)line->operands[1].reg;
        if (out) bufferWriteU16BE(out, opcode);
        return 2;
    }
    return 0;
}

/* ── ANDI/ORI/EORI to CCR/SR ────────────────────────── */
static u32 encodeImmCCRSR(const AsmLine *line, Buffer *out, u16 baseImm) {
    if (line->operandCount < 2) return 0;
    if (line->operands[1].type == OPER_CCR) {
        u16 opcode = baseImm | 0x003C;
        if (out) {
            bufferWriteU16BE(out, opcode);
            bufferWriteU16BE(out, (u16)(line->operands[0].immediate & 0xFF));
        }
        return 4;
    } else if (line->operands[1].type == OPER_SR) {
        u16 opcode = baseImm | 0x007C;
        if (out) {
            bufferWriteU16BE(out, opcode);
            bufferWriteU16BE(out, (u16)line->operands[0].immediate);
        }
        return 4;
    }
    return encodeAluImm(line, out, baseImm);
}

/* ── CMPM ────────────────────────────────────────────── */
static u32 encodeCmpm(const AsmLine *line, Buffer *out) {
    if (line->operandCount < 2) return 0;
    u16 sz = sizeField76(line->size);
    int ax = line->operands[0].reg;
    int ay = line->operands[1].reg;
    u16 opcode = 0xB108 | ((u16)ay << 9) | (sz << 6) | (u16)ax;
    if (out) bufferWriteU16BE(out, opcode);
    return 2;
}

/* ═══════════════════════════════════════════════════════
 *  Main dispatch
 * ═══════════════════════════════════════════════════════ */

static bool mneq(const char *mnemonic, const char *expected) {
    return strcmp(mnemonic, expected) == 0;
}

u32 encoderEncode(const AsmLine *line, SymbolTable *symbols,
                  Buffer *output, u32 pc) {
    if (line->isDirective)
        return directiveProcess(line, symbols, output, pc, 2);

    const char *m = line->mnemonic;

    /* Empty mnemonic (label-only line) — emit nothing */
    if (m[0] == '\0') return 0;

    /* ── MOVE family ── */
    if (mneq(m, "MOVE") || mneq(m, "MOVEA")) {
        if (line->operandCount == 2 &&
            (line->operands[0].type == OPER_SR || line->operands[1].type == OPER_SR ||
             line->operands[0].type == OPER_CCR || line->operands[1].type == OPER_CCR ||
             line->operands[0].type == OPER_USP || line->operands[1].type == OPER_USP))
            return encodeMoveSR(line, output);
        return encodeMove(line, symbols, output, pc);
    }
    if (mneq(m, "MOVEQ"))   return encodeMoveq(line, output);
    if (mneq(m, "MOVEM"))   return encodeMovem(line, output);
    if (mneq(m, "MOVEP"))   return encodeMovep(line, output);

    /* ── Address computation ── */
    if (mneq(m, "LEA"))     return encodeLea(line, output);
    if (mneq(m, "PEA"))     return encodePea(line, output);

    /* ── ADD family ── */
    if (mneq(m, "ADD"))     return encodeAluReg(line, output, 0xD000);
    if (mneq(m, "ADDA"))    return encodeAluAddr(line, output, 0xD000);
    if (mneq(m, "ADDI"))    return encodeAluImm(line, output, 0x0600);
    if (mneq(m, "ADDQ"))    return encodeQuick(line, output, false);
    if (mneq(m, "ADDX"))    return encodeXOp(line, output, 0xD000);

    /* ── SUB family ── */
    if (mneq(m, "SUB"))     return encodeAluReg(line, output, 0x9000);
    if (mneq(m, "SUBA"))    return encodeAluAddr(line, output, 0x9000);
    if (mneq(m, "SUBI"))    return encodeAluImm(line, output, 0x0400);
    if (mneq(m, "SUBQ"))    return encodeQuick(line, output, true);
    if (mneq(m, "SUBX"))    return encodeXOp(line, output, 0x9000);

    /* ── CMP family ── */
    if (mneq(m, "CMP"))     return encodeAluReg(line, output, 0xB000);
    if (mneq(m, "CMPA"))    return encodeAluAddr(line, output, 0xB000);
    if (mneq(m, "CMPI"))    return encodeAluImm(line, output, 0x0C00);
    if (mneq(m, "CMPM"))    return encodeCmpm(line, output);

    /* ── AND family ── */
    if (mneq(m, "AND"))     return encodeAluReg(line, output, 0xC000);
    if (mneq(m, "ANDI"))    return encodeImmCCRSR(line, output, 0x0200);

    /* ── OR family ── */
    if (mneq(m, "OR"))      return encodeAluReg(line, output, 0x8000);
    if (mneq(m, "ORI"))     return encodeImmCCRSR(line, output, 0x0000);

    /* ── EOR family ── */
    if (mneq(m, "EOR"))     return encodeEor(line, output);
    if (mneq(m, "EORI"))    return encodeImmCCRSR(line, output, 0x0A00);

    /* ── NOT / NEG / NEGX / CLR / TST ── */
    if (mneq(m, "NOT"))     return encodeUnaryEA(line, output, 0x4600);
    if (mneq(m, "NEG"))     return encodeUnaryEA(line, output, 0x4400);
    if (mneq(m, "NEGX"))    return encodeUnaryEA(line, output, 0x4000);
    if (mneq(m, "CLR"))     return encodeUnaryEA(line, output, 0x4200);
    if (mneq(m, "TST"))     return encodeUnaryEA(line, output, 0x4A00);

    /* ── EXT / SWAP / EXG ── */
    if (mneq(m, "EXT"))     return encodeExt(line, output);
    if (mneq(m, "SWAP"))    return encodeSwap(line, output);
    if (mneq(m, "EXG"))     return encodeExg(line, output);

    /* ── MUL / DIV ── */
    if (mneq(m, "MULU"))    return encodeMulDiv(line, output, 0xC0C0);
    if (mneq(m, "MULS"))    return encodeMulDiv(line, output, 0xC1C0);
    if (mneq(m, "DIVU"))    return encodeMulDiv(line, output, 0x80C0);
    if (mneq(m, "DIVS"))    return encodeMulDiv(line, output, 0x81C0);

    /* ── Shifts / Rotates ── */
    if (mneq(m, "ASL"))     return encodeShift(line, output, 0, 1);
    if (mneq(m, "ASR"))     return encodeShift(line, output, 0, 0);
    if (mneq(m, "LSL"))     return encodeShift(line, output, 1, 1);
    if (mneq(m, "LSR"))     return encodeShift(line, output, 1, 0);
    if (mneq(m, "ROXL"))    return encodeShift(line, output, 2, 1);
    if (mneq(m, "ROXR"))    return encodeShift(line, output, 2, 0);
    if (mneq(m, "ROL"))     return encodeShift(line, output, 3, 1);
    if (mneq(m, "ROR"))     return encodeShift(line, output, 3, 0);

    /* ── Bit operations ── */
    if (mneq(m, "BTST"))    return encodeBitOp(line, output, 0);
    if (mneq(m, "BCHG"))    return encodeBitOp(line, output, 1);
    if (mneq(m, "BCLR"))    return encodeBitOp(line, output, 2);
    if (mneq(m, "BSET"))    return encodeBitOp(line, output, 3);

    /* ── BCD ── */
    if (mneq(m, "ABCD"))    return encodeBcd(line, output, false);
    if (mneq(m, "SBCD"))    return encodeBcd(line, output, true);
    if (mneq(m, "NBCD"))    return encodeNbcd(line, output);

    /* ── Branch ── */
    if (mneq(m, "BRA"))     return encodeBranch(line, output, pc, 0);
    if (mneq(m, "BSR"))     return encodeBranch(line, output, pc, 1);
    if (m[0] == 'B' && strlen(m) >= 3) {
        int cc = condCode(m + 1);
        if (cc >= 2) return encodeBranch(line, output, pc, cc);
    }

    /* ── DBcc ── */
    if (m[0] == 'D' && m[1] == 'B') {
        int cc = condCode(m + 2);
        if (cc >= 0) return encodeDBcc(line, output, pc, cc);
    }
    if (mneq(m, "DBRA"))    return encodeDBcc(line, output, pc, 1);

    /* ── Scc ── */
    if (m[0] == 'S' && strlen(m) >= 2 && strcmp(m, "SWAP") != 0 &&
        strcmp(m, "STOP") != 0 && strcmp(m, "SBCD") != 0 &&
        strcmp(m, "SUB") != 0 && strcmp(m, "SUBA") != 0 &&
        strcmp(m, "SUBI") != 0 && strcmp(m, "SUBQ") != 0 &&
        strcmp(m, "SUBX") != 0) {
        int cc = condCode(m + 1);
        if (cc >= 0) return encodeScc(line, output, cc);
    }

    /* ── JMP / JSR ── */
    if (mneq(m, "JMP"))     return encodeJmpJsr(line, output, false);
    if (mneq(m, "JSR"))     return encodeJmpJsr(line, output, true);

    /* ── Linkage ── */
    if (mneq(m, "LINK"))    return encodeLink(line, output);
    if (mneq(m, "UNLK"))    return encodeUnlk(line, output);

    /* ── Misc control ── */
    if (mneq(m, "TRAP"))    return encodeTrap(line, output);
    if (mneq(m, "CHK"))     return encodeChk(line, output);
    if (mneq(m, "TAS"))     return encodeTas(line, output);

    /* ── Simple one-word instructions ── */
    if (mneq(m, "NOP"))     { if (output) bufferWriteU16BE(output, 0x4E71); return 2; }
    if (mneq(m, "RTS"))     { if (output) bufferWriteU16BE(output, 0x4E75); return 2; }
    if (mneq(m, "RTE"))     { if (output) bufferWriteU16BE(output, 0x4E73); return 2; }
    if (mneq(m, "RTR"))     { if (output) bufferWriteU16BE(output, 0x4E77); return 2; }
    if (mneq(m, "TRAPV"))   { if (output) bufferWriteU16BE(output, 0x4E76); return 2; }
    if (mneq(m, "RESET"))   { if (output) bufferWriteU16BE(output, 0x4E70); return 2; }
    if (mneq(m, "ILLEGAL")) { if (output) bufferWriteU16BE(output, 0x4AFC); return 2; }

    /* STOP #imm */
    if (mneq(m, "STOP")) {
        if (output) {
            bufferWriteU16BE(output, 0x4E72);
            bufferWriteU16BE(output, (u16)line->operands[0].immediate);
        }
        return 4;
    }

    /* Unknown mnemonic — emit NOP as fallback */
    if (output) bufferWriteU16BE(output, 0x4E71);
    return 2;
}

u32 encoderCalcSize(const AsmLine *line, SymbolTable *symbols, u32 pc, int pass) {
    if (line->isDirective)
        return directiveCalcSize(line, symbols, pc, pass);
    return encoderEncode(line, symbols, NULL, pc);
}
