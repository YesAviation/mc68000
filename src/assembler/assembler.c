/* assembler.c — MC68000 macro assembler implementation */
#include "assembler/assembler.h"
#include "assembler/lexer.h"
#include "assembler/parser.h"
#include "assembler/symbols.h"
#include "assembler/directives.h"
#include "assembler/macros.h"
#include "assembler/encoder.h"
#include "assembler/output.h"
#include "common/log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ASM_MAX_ERRORS 256

struct Assembler {
    /* Components */
    AsmLexer      *lexer;
    AsmParser     *parser;
    SymbolTable   *symbols;
    MacroTable    *macros;

    /* Output */
    Buffer         output;
    u32            origin;
    u32            pc;
    u32            entryPoint;

    /* Errors */
    char          *errors[ASM_MAX_ERRORS];
    int            errorCount;

    /* Options */
    bool           listing;
    int            pass;        /* 1 or 2 */
};

/* ── internal helpers ────────────────────────────────── */

static void asmError(Assembler *as, const char *fmt, ...) {
    if (as->errorCount >= ASM_MAX_ERRORS) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    as->errors[as->errorCount++] = strdup(buf);
}

/* ── lifecycle ───────────────────────────────────────── */

Assembler *asmCreate(void) {
    Assembler *as = calloc(1, sizeof(Assembler));
    as->lexer   = asmLexerCreate();
    as->parser  = asmParserCreate();
    as->symbols = symbolTableCreate();
    as->macros  = macroTableCreate();
    bufferInit(&as->output);
    return as;
}

void asmDestroy(Assembler *as) {
    if (!as) return;
    asmLexerDestroy(as->lexer);
    asmParserDestroy(as->parser);
    symbolTableDestroy(as->symbols);
    macroTableDestroy(as->macros);
    bufferFree(&as->output);
    for (int i = 0; i < as->errorCount; i++) free(as->errors[i]);
    free(as);
}

/* ── configuration ───────────────────────────────────── */

void asmSetOrigin(Assembler *as, u32 origin) { as->origin = origin; as->pc = origin; }
void asmSetListing(Assembler *as, bool enable) { as->listing = enable; }

/* ── assembly ────────────────────────────────────────── */

bool asmAssembleString(Assembler *as, const char *source, const char *filename) {
    as->errorCount = 0;
    bufferClear(&as->output);
    as->pc = as->origin;

    /* Pass 1 — collect symbols */
    as->pass = 1;
    asmLexerSetInput(as->lexer, source, filename);
    while (asmLexerHasMore(as->lexer)) {
        AsmLine line;
        if (!asmParserParseLine(as->parser, as->lexer, &line)) {
            /* parseLine returns false for EOF or blank remainder — not an error */
            break;
        }
        /* Handle ORG directive — update PC to new origin */
        if (line.isDirective && strcmp(line.mnemonic, "ORG") == 0) {
            if (line.operandCount > 0) {
                as->pc = (u32)line.operands[0].immediate;
            }
            continue;
        }
        /* Handle EQU: define label = operand value (not PC) */
        if (line.label[0]) {
            if (line.isDirective && strcmp(line.mnemonic, "EQU") == 0) {
                s64 val = line.operands[0].immediate;
                if (line.operands[0].symbol[0])
                    symbolTableResolve(as->symbols, line.operands[0].symbol, &val);
                symbolTableDefineKind(as->symbols, line.label, val, SYM_EQU);
                continue;
            }
            symbolTableDefine(as->symbols, line.label, as->pc);
        }
        /* Resolve symbol references in operands */
        for (int i = 0; i < line.operandCount; i++) {
            if (line.operands[i].symbol[0]) {
                s64 val = 0;
                if (symbolTableResolve(as->symbols, line.operands[i].symbol, &val)) {
                    line.operands[i].immediate = val;
                    if (line.operands[i].type == OPER_PC_DISP)
                        line.operands[i].displacement = (s32)(val - (s64)(as->pc + 2));
                    else if (line.operands[i].type == OPER_ADDR_DISP)
                        line.operands[i].displacement = (s32)val;
                }
            }
        }
        /* Calculate instruction/directive size to advance PC */
        u32 size = encoderCalcSize(&line, as->symbols, as->pc, as->pass);
        as->pc += size;
    }

    /* Pass 2 — emit binary */
    as->pass = 2;
    as->pc = as->origin;
    asmLexerSetInput(as->lexer, source, filename);
    while (asmLexerHasMore(as->lexer)) {
        AsmLine line;
        if (!asmParserParseLine(as->parser, as->lexer, &line)) break;
        /* Handle ORG directive — update PC to new origin */
        if (line.isDirective && strcmp(line.mnemonic, "ORG") == 0) {
            if (line.operandCount > 0) {
                u32 newPC = (u32)line.operands[0].immediate;
                /* Pad output buffer to reach new address if forward */
                if (newPC > as->pc) {
                    u32 gap = newPC - as->pc;
                    for (u32 i = 0; i < gap; i++) bufferWriteU8(&as->output, 0xFF);
                }
                as->pc = newPC;
            }
            continue;
        }
        /* Handle EQU: define label = operand value (not PC) */
        if (line.label[0]) {
            if (line.isDirective && strcmp(line.mnemonic, "EQU") == 0) {
                s64 val = line.operands[0].immediate;
                if (line.operands[0].symbol[0])
                    symbolTableResolve(as->symbols, line.operands[0].symbol, &val);
                symbolTableDefineKind(as->symbols, line.label, val, SYM_EQU);
                continue;
            }
            symbolTableDefine(as->symbols, line.label, as->pc);
        }
        /* Resolve symbol references in operands */
        for (int i = 0; i < line.operandCount; i++) {
            if (line.operands[i].symbol[0]) {
                s64 val = 0;
                if (symbolTableResolve(as->symbols, line.operands[i].symbol, &val)) {
                    line.operands[i].immediate = val;
                    if (line.operands[i].type == OPER_PC_DISP)
                        line.operands[i].displacement = (s32)(val - (s64)(as->pc + 2));
                    else if (line.operands[i].type == OPER_ADDR_DISP)
                        line.operands[i].displacement = (s32)val;
                }
            }
        }
        u32 emitted = encoderEncode(&line, as->symbols, &as->output, as->pc);
        as->pc += emitted;
    }

    return as->errorCount == 0;
}

bool asmAssembleFile(Assembler *as, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        asmError(as, "Cannot open file: %s", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *src = malloc((size_t)size + 1);
    fread(src, 1, (size_t)size, f);
    src[size] = '\0';
    fclose(f);

    bool ok = asmAssembleString(as, src, path);
    free(src);
    return ok;
}

/* ── output ──────────────────────────────────────────── */

bool asmWriteBinary(Assembler *as, const char *path) {
    return outputWriteBinary(&as->output, path);
}

bool asmWriteSRecord(Assembler *as, const char *path) {
    return outputWriteSRecord(&as->output, as->origin, path);
}

const u8 *asmGetOutput(Assembler *as, u32 *outSize) {
    if (outSize) *outSize = as->output.size;
    return as->output.data;
}

u32 asmGetEntryPoint(Assembler *as) { return as->entryPoint; }
int asmGetErrorCount(Assembler *as) { return as->errorCount; }
const char *asmGetError(Assembler *as, int index) {
    if (index < 0 || index >= as->errorCount) return NULL;
    return as->errors[index];
}
