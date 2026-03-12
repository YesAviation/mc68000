/* test_assembler.c — Assembler subsystem tests */
#include "test_framework.h"
#include "common/types.h"
#include "assembler/assembler.h"
#include "assembler/lexer.h"
#include "assembler/parser.h"
#include "assembler/symbols.h"
#include "assembler/macros.h"

/* ── Lexer tests ────────────────────────────────────── */

TEST(lexerDecimal) {
    AsmLexer *lex = asmLexerCreate();
    asmLexerSetInput(lex, "123", "<test>");
    AsmToken tok = asmLexerNext(lex);
    ASSERT_EQ(tok.type, TOK_NUMBER);
    ASSERT_EQ(tok.numValue, 123);
    asmLexerDestroy(lex);
}

TEST(lexerHex) {
    AsmLexer *lex = asmLexerCreate();
    asmLexerSetInput(lex, "$FF", "<test>");
    AsmToken tok = asmLexerNext(lex);
    ASSERT_EQ(tok.type, TOK_NUMBER);
    ASSERT_EQ(tok.numValue, 0xFF);
    asmLexerDestroy(lex);
}

TEST(lexerBinary) {
    AsmLexer *lex = asmLexerCreate();
    asmLexerSetInput(lex, "%10101010", "<test>");
    AsmToken tok = asmLexerNext(lex);
    ASSERT_EQ(tok.type, TOK_NUMBER);
    ASSERT_EQ(tok.numValue, 0xAA);
    asmLexerDestroy(lex);
}

TEST(lexerIdentifier) {
    AsmLexer *lex = asmLexerCreate();
    asmLexerSetInput(lex, "myLabel", "<test>");
    AsmToken tok = asmLexerNext(lex);
    ASSERT_EQ(tok.type, TOK_IDENTIFIER);
    /* Compare first tok.length chars of tok.start */
    ASSERT_TRUE(tok.length == 7);
    ASSERT_TRUE(memcmp(tok.start, "myLabel", 7) == 0);
    asmLexerDestroy(lex);
}

TEST(lexerString) {
    AsmLexer *lex = asmLexerCreate();
    asmLexerSetInput(lex, "\"hello\"", "<test>");
    AsmToken tok = asmLexerNext(lex);
    ASSERT_EQ(tok.type, TOK_STRING);
    asmLexerDestroy(lex);
}

/* ── Symbol table tests ─────────────────────────────── */

TEST(symbolDefineAndLookup) {
    SymbolTable *st = symbolTableCreate();
    symbolTableDefineKind(st, "start", 0x1000, SYM_LABEL);
    Symbol *sym = symbolTableLookup(st, "start");
    ASSERT_NOT_NULL(sym);
    ASSERT_HEX_EQ(sym->value, 0x1000);
    ASSERT_EQ(sym->kind, SYM_LABEL);
    symbolTableDestroy(st);
}

TEST(symbolNotFound) {
    SymbolTable *st = symbolTableCreate();
    Symbol *sym = symbolTableLookup(st, "missing");
    ASSERT_NULL(sym);
    symbolTableDestroy(st);
}

/* ── Macro tests ────────────────────────────────────── */

TEST(macroDefineAndLookup) {
    MacroTable *mt = macroTableCreate();
    const char params[][MACRO_MAX_NAME] = { "reg" };
    macroTableDefine(mt, "push", " MOVE.L \\1,-(SP)", params, 1);
    Macro *m = macroTableLookup(mt, "push");
    ASSERT_NOT_NULL(m);
    ASSERT_EQ(m->paramCount, 1);
    macroTableDestroy(mt);
}

/* ── Assembler integration test ─────────────────────── */

TEST(assembleNop) {
    Assembler *as = asmCreate();
    bool ok = asmAssembleString(as, "    NOP\n    END\n", "<test>");
    ASSERT_TRUE(ok);
    u32 size;
    const u8 *out = asmGetOutput(as, &size);
    ASSERT_EQ(size, 2); /* NOP = $4E71 = 2 bytes */
    if (size >= 2) {
        ASSERT_HEX_EQ(out[0], 0x4E);
        ASSERT_HEX_EQ(out[1], 0x71);
    }
    asmDestroy(as);
}

/* ── Suite ───────────────────────────────────────────── */
TEST_SUITE(assembler) {
    RUN_TEST(lexerDecimal);
    RUN_TEST(lexerHex);
    RUN_TEST(lexerBinary);
    RUN_TEST(lexerIdentifier);
    RUN_TEST(lexerString);
    RUN_TEST(symbolDefineAndLookup);
    RUN_TEST(symbolNotFound);
    RUN_TEST(macroDefineAndLookup);
    RUN_TEST(assembleNop);
}

int main(void) {
    RUN_SUITE(assembler);
    TEST_REPORT();
    return TEST_EXIT_CODE();
}
