/* lexer.c — Assembly language lexer */
#include "assembler/lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct AsmLexer {
    const char *source;
    const char *pos;
    const char *filename;
    int         line;
    int         col;
    AsmToken    peeked;
    bool        hasPeeked;
};

AsmLexer *asmLexerCreate(void) { return calloc(1, sizeof(AsmLexer)); }
void asmLexerDestroy(AsmLexer *lex) { free(lex); }

void asmLexerSetInput(AsmLexer *lex, const char *source, const char *filename) {
    lex->source   = source;
    lex->pos      = source;
    lex->filename = filename;
    lex->line     = 1;
    lex->col      = 1;
    lex->hasPeeked = false;
}

bool asmLexerHasMore(AsmLexer *lex) {
    if (lex->hasPeeked) return lex->peeked.type != TOK_EOF;
    const char *p = lex->pos;
    while (*p == ' ' || *p == '\t') p++;
    return *p != '\0';
}

int asmLexerGetLine(AsmLexer *lex) { return lex->line; }
const char *asmLexerGetFilename(AsmLexer *lex) { return lex->filename; }

static void advance(AsmLexer *lex) {
    if (*lex->pos == '\n') { lex->line++; lex->col = 1; }
    else { lex->col++; }
    lex->pos++;
}

static AsmToken makeToken(AsmLexer *lex, AsmTokenType type, const char *start, int len) {
    AsmToken t;
    t.type = type; t.start = start; t.length = len;
    t.numValue = 0; t.line = lex->line; t.col = lex->col - len;
    return t;
}

static s64 parseNumber(const char *start, int len) {
    /* $hex, 0xHex, %binary, @octal, decimal */
    char buf[64];
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    memcpy(buf, start, (size_t)len); buf[len] = '\0';

    if (buf[0] == '$')          return (s64)strtoull(buf + 1, NULL, 16);
    if (buf[0] == '%')          return (s64)strtoull(buf + 1, NULL, 2);
    if (buf[0] == '@')          return (s64)strtoull(buf + 1, NULL, 8);
    if (len > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
        return (s64)strtoull(buf + 2, NULL, 16);
    return (s64)strtoull(buf, NULL, 10);
}

AsmToken asmLexerNext(AsmLexer *lex) {
    if (lex->hasPeeked) { lex->hasPeeked = false; return lex->peeked; }

    /* skip whitespace (not newlines) */
    while (*lex->pos == ' ' || *lex->pos == '\t') advance(lex);

    const char *start = lex->pos;
    char c = *lex->pos;

    if (c == '\0') return makeToken(lex, TOK_EOF, start, 0);
    if (c == '\n' || c == '\r') {
        advance(lex);
        if (c == '\r' && *lex->pos == '\n') advance(lex);
        return makeToken(lex, TOK_NEWLINE, start, (int)(lex->pos - start));
    }
    if (c == ';' || c == '*') {
        /* line comment — consume to end of line */
        if (c == '*' && start != lex->source && *(start - 1) != '\n') {
            /* * not at start of line => it's the multiplication operator */
            advance(lex);
            return makeToken(lex, TOK_STAR, start, 1);
        }
        while (*lex->pos && *lex->pos != '\n') advance(lex);
        return makeToken(lex, TOK_COMMENT, start, (int)(lex->pos - start));
    }

    /* single-character tokens */
    switch (c) {
        case '.': advance(lex); return makeToken(lex, TOK_DOT, start, 1);
        case ':': advance(lex); return makeToken(lex, TOK_COLON, start, 1);
        case ',': advance(lex); return makeToken(lex, TOK_COMMA, start, 1);
        case '#': advance(lex); return makeToken(lex, TOK_HASH, start, 1);
        case '(': advance(lex); return makeToken(lex, TOK_LPAREN, start, 1);
        case ')': advance(lex); return makeToken(lex, TOK_RPAREN, start, 1);
        case '+': advance(lex); return makeToken(lex, TOK_PLUS, start, 1);
        case '-': advance(lex); return makeToken(lex, TOK_MINUS, start, 1);
        case '/': advance(lex); return makeToken(lex, TOK_SLASH, start, 1);
        case '&': advance(lex); return makeToken(lex, TOK_AMPERSAND, start, 1);
        case '|': advance(lex); return makeToken(lex, TOK_PIPE, start, 1);
        case '^': advance(lex); return makeToken(lex, TOK_CARET, start, 1);
        case '~': advance(lex); return makeToken(lex, TOK_TILDE, start, 1);
        case '=': advance(lex); return makeToken(lex, TOK_EQUALS, start, 1);
        case '<': advance(lex); if (*lex->pos == '<') { advance(lex); return makeToken(lex, TOK_LSHIFT, start, 2); }
                  return makeToken(lex, TOK_ERROR, start, 1);
        case '>': advance(lex); if (*lex->pos == '>') { advance(lex); return makeToken(lex, TOK_RSHIFT, start, 2); }
                  return makeToken(lex, TOK_ERROR, start, 1);
        default: break;
    }

    /* String literal */
    if (c == '"' || c == '\'') {
        char quote = c;
        advance(lex);
        while (*lex->pos && *lex->pos != quote && *lex->pos != '\n') advance(lex);
        if (*lex->pos == quote) advance(lex);
        return makeToken(lex, TOK_STRING, start + 1, (int)(lex->pos - start - 2));
    }

    /* Number: $hex, %binary, @octal, 0x, digit... */
    if (c == '$' || c == '%' || c == '@' || isdigit((unsigned char)c)) {
        advance(lex);
        while (isalnum((unsigned char)*lex->pos) || *lex->pos == '_') advance(lex);
        int len = (int)(lex->pos - start);
        AsmToken t = makeToken(lex, TOK_NUMBER, start, len);
        t.numValue = parseNumber(start, len);
        return t;
    }

    /* Identifier (label, mnemonic, register) */
    if (isalpha((unsigned char)c) || c == '_') {
        advance(lex);
        while (isalnum((unsigned char)*lex->pos) || *lex->pos == '_') advance(lex);
        return makeToken(lex, TOK_IDENTIFIER, start, (int)(lex->pos - start));
    }

    /* Unknown */
    advance(lex);
    return makeToken(lex, TOK_ERROR, start, 1);
}

AsmToken asmLexerPeek(AsmLexer *lex) {
    if (!lex->hasPeeked) {
        lex->peeked = asmLexerNext(lex);
        lex->hasPeeked = true;
    }
    return lex->peeked;
}
