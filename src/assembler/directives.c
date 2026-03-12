/* directives.c — Assembler directives */
#include "assembler/directives.h"
#include <string.h>

/* ── individual directive handlers ───────────────────── */

static u32 dirOrg(const AsmLine *line, SymbolTable *symbols, Buffer *out, u32 pc, int pass) {
    (void)symbols; (void)out; (void)pc; (void)pass;
    /* PC is set by the assembler top-level; we just return 0 bytes emitted */
    (void)line;
    return 0;
}

static u32 dirDC(const AsmLine *line, SymbolTable *symbols, Buffer *out, u32 pc, int pass) {
    (void)pc;
    u32 emitted = 0;
    u8 sz = line->size ? line->size : 2; /* default word */

    for (int i = 0; i < line->dataCount; i++) {
        s64 val = line->dataValues[i];
        if (pass == 2) {
            if (sz == 1)      bufferWriteU8(out, (u8)val);
            else if (sz == 2) bufferWriteU16BE(out, (u16)val);
            else              bufferWriteU32BE(out, (u32)val);
        }
        emitted += sz;
    }
    /* If no data values but operands exist, handle single value from operand */
    if (line->dataCount == 0 && line->operandCount > 0) {
        s64 val = line->operands[0].immediate;
        if (line->operands[0].symbol[0]) {
            symbolTableResolve(symbols, line->operands[0].symbol, &val);
        }
        if (pass == 2) {
            if (sz == 1)      bufferWriteU8(out, (u8)val);
            else if (sz == 2) bufferWriteU16BE(out, (u16)val);
            else              bufferWriteU32BE(out, (u32)val);
        }
        emitted += sz;
    }
    return emitted;
}

static u32 dirDS(const AsmLine *line, SymbolTable *symbols, Buffer *out, u32 pc, int pass) {
    (void)symbols; (void)pc;
    u8 sz = line->size ? line->size : 2;
    s64 count = 0;
    if (line->operandCount > 0) count = line->operands[0].immediate;
    u32 total = (u32)count * sz;
    if (pass == 2) {
        for (u32 i = 0; i < total; i++) bufferWriteU8(out, 0);
    }
    return total;
}

static u32 dirEven(const AsmLine *line, SymbolTable *symbols, Buffer *out, u32 pc, int pass) {
    (void)line; (void)symbols;
    if (pc & 1) {
        if (pass == 2) bufferWriteU8(out, 0);
        return 1;
    }
    return 0;
}

static u32 dirAlign(const AsmLine *line, SymbolTable *symbols, Buffer *out, u32 pc, int pass) {
    (void)symbols;
    u32 alignment = 2;
    if (line->operandCount > 0) alignment = (u32)line->operands[0].immediate;
    if (alignment == 0) alignment = 1;
    u32 pad = (alignment - (pc % alignment)) % alignment;
    if (pass == 2) {
        for (u32 i = 0; i < pad; i++) bufferWriteU8(out, 0);
    }
    return pad;
}

/* ── dispatch ────────────────────────────────────────── */

u32 directiveProcess(const AsmLine *line, SymbolTable *symbols,
                     Buffer *output, u32 pc, int pass) {
    if (strcmp(line->mnemonic, "ORG") == 0)     return dirOrg(line, symbols, output, pc, pass);
    if (strcmp(line->mnemonic, "DC") == 0)      return dirDC(line, symbols, output, pc, pass);
    if (strcmp(line->mnemonic, "DS") == 0)      return dirDS(line, symbols, output, pc, pass);
    if (strcmp(line->mnemonic, "EVEN") == 0)    return dirEven(line, symbols, output, pc, pass);
    if (strcmp(line->mnemonic, "ALIGN") == 0)   return dirAlign(line, symbols, output, pc, pass);
    if (strcmp(line->mnemonic, "END") == 0)     return 0;
    if (strcmp(line->mnemonic, "EQU") == 0) {
        /* Handled by assembler top-level (label = value) */
        return 0;
    }
    if (strcmp(line->mnemonic, "SET") == 0)     return 0;
    /* MACRO, ENDM, REPT, ENDR, IF, ELSE, ENDIF — handled by macro/conditional layer */
    return 0;
}

u32 directiveCalcSize(const AsmLine *line, SymbolTable *symbols, u32 pc, int pass) {
    /* Same as process but without emitting */
    return directiveProcess(line, symbols, NULL, pc, pass);
}
