/* cc_lexer.c — C language lexer (stub) */
#include "compiler/frontend/cc_lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct CcLexer {
    const char *source;
    const char *pos;
    const char *filename;
    int         line;
    int         col;
    CcToken     peeked;
    bool        hasPeeked;
};

CcLexer *ccLexerCreate(void)  { return calloc(1, sizeof(CcLexer)); }
void ccLexerDestroy(CcLexer *lex) { free(lex); }

void ccLexerSetInput(CcLexer *lex, const char *source, const char *filename) {
    lex->source = source; lex->pos = source;
    lex->filename = filename; lex->line = 1; lex->col = 1;
    lex->hasPeeked = false;
}

int ccLexerGetLine(CcLexer *lex) { return lex->line; }

static void ccAdvance(CcLexer *lex) {
    if (*lex->pos == '\n') { lex->line++; lex->col = 1; } else { lex->col++; }
    lex->pos++;
}

static CcToken ccMake(CcLexer *lex, CcTokenType t, const char *s, int len) {
    CcToken tok = {0};
    tok.type = t; tok.start = s; tok.length = len;
    tok.line = lex->line; tok.col = lex->col;
    return tok;
}

CcToken ccLexerNext(CcLexer *lex) {
    if (lex->hasPeeked) { lex->hasPeeked = false; return lex->peeked; }

    /* skip whitespace */
    while (*lex->pos && (*lex->pos == ' ' || *lex->pos == '\t' ||
                         *lex->pos == '\n' || *lex->pos == '\r')) ccAdvance(lex);

    const char *start = lex->pos;
    if (!*lex->pos) return ccMake(lex, CC_TOK_EOF, start, 0);

    /* skip // and block comments */
    if (lex->pos[0] == '/' && lex->pos[1] == '/') {
        while (*lex->pos && *lex->pos != '\n') ccAdvance(lex);
        return ccLexerNext(lex);
    }
    if (lex->pos[0] == '/' && lex->pos[1] == '*') {
        ccAdvance(lex); ccAdvance(lex);
        while (*lex->pos && !(lex->pos[0] == '*' && lex->pos[1] == '/')) ccAdvance(lex);
        if (*lex->pos) { ccAdvance(lex); ccAdvance(lex); }
        return ccLexerNext(lex);
    }

    char c = *lex->pos;

    /* Number */
    if (isdigit((unsigned char)c)) {
        while (isalnum((unsigned char)*lex->pos) || *lex->pos == 'x' || *lex->pos == 'X') ccAdvance(lex);
        CcToken t = ccMake(lex, CC_TOK_INT_LIT, start, (int)(lex->pos - start));
        char buf[32]; int n = t.length < 31 ? t.length : 31;
        memcpy(buf, t.start, (size_t)n); buf[n] = 0;
        t.intValue = (s64)strtoll(buf, NULL, 0);
        return t;
    }

    /* Identifier / keyword */
    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)*lex->pos) || *lex->pos == '_') ccAdvance(lex);
        CcToken t = ccMake(lex, CC_TOK_IDENT, start, (int)(lex->pos - start));
        /* keyword matching (simplified) */
        struct { const char *kw; CcTokenType tt; } kws[] = {
            {"auto",CC_TOK_AUTO},{"break",CC_TOK_BREAK},{"case",CC_TOK_CASE},
            {"char",CC_TOK_CHAR},{"const",CC_TOK_CONST},{"continue",CC_TOK_CONTINUE},
            {"default",CC_TOK_DEFAULT},{"do",CC_TOK_DO},{"double",CC_TOK_DOUBLE},
            {"else",CC_TOK_ELSE},{"enum",CC_TOK_ENUM},{"extern",CC_TOK_EXTERN},
            {"float",CC_TOK_FLOAT},{"for",CC_TOK_FOR},{"goto",CC_TOK_GOTO},
            {"if",CC_TOK_IF},{"int",CC_TOK_INT},{"long",CC_TOK_LONG},
            {"register",CC_TOK_REGISTER},{"return",CC_TOK_RETURN},
            {"short",CC_TOK_SHORT},{"signed",CC_TOK_SIGNED},{"sizeof",CC_TOK_SIZEOF},
            {"static",CC_TOK_STATIC},{"struct",CC_TOK_STRUCT},{"switch",CC_TOK_SWITCH},
            {"typedef",CC_TOK_TYPEDEF},{"union",CC_TOK_UNION},
            {"unsigned",CC_TOK_UNSIGNED},{"void",CC_TOK_VOID},
            {"volatile",CC_TOK_VOLATILE},{"while",CC_TOK_WHILE},{NULL,CC_TOK_EOF}
        };
        for (int i = 0; kws[i].kw; i++) {
            if (t.length == (int)strlen(kws[i].kw) && memcmp(t.start, kws[i].kw, (size_t)t.length) == 0) {
                t.type = kws[i].tt; break;
            }
        }
        return t;
    }

    /* String literal */
    if (c == '"') {
        ccAdvance(lex);
        while (*lex->pos && *lex->pos != '"') {
            if (*lex->pos == '\\') ccAdvance(lex);
            ccAdvance(lex);
        }
        if (*lex->pos == '"') ccAdvance(lex);
        return ccMake(lex, CC_TOK_STRING_LIT, start + 1, (int)(lex->pos - start - 2));
    }

    /* Char literal */
    if (c == '\'') {
        ccAdvance(lex);
        if (*lex->pos == '\\') ccAdvance(lex);
        ccAdvance(lex);
        if (*lex->pos == '\'') ccAdvance(lex);
        return ccMake(lex, CC_TOK_CHAR_LIT, start, (int)(lex->pos - start));
    }

    /* Multi-char operators (simplified) */
    ccAdvance(lex);
    switch (c) {
        case '(': return ccMake(lex, CC_TOK_LPAREN, start, 1);
        case ')': return ccMake(lex, CC_TOK_RPAREN, start, 1);
        case '{': return ccMake(lex, CC_TOK_LBRACE, start, 1);
        case '}': return ccMake(lex, CC_TOK_RBRACE, start, 1);
        case '[': return ccMake(lex, CC_TOK_LBRACKET, start, 1);
        case ']': return ccMake(lex, CC_TOK_RBRACKET, start, 1);
        case ';': return ccMake(lex, CC_TOK_SEMICOLON, start, 1);
        case ',': return ccMake(lex, CC_TOK_COMMA, start, 1);
        case '~': return ccMake(lex, CC_TOK_TILDE, start, 1);
        case '?': return ccMake(lex, CC_TOK_QUESTION, start, 1);
        case ':': return ccMake(lex, CC_TOK_COLON, start, 1);
        case '#': return ccMake(lex, CC_TOK_HASH, start, 1);
        case '+': if (*lex->pos=='+') { ccAdvance(lex); return ccMake(lex,CC_TOK_INC,start,2); }
                  if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_PLUS_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_PLUS, start, 1);
        case '-': if (*lex->pos=='-') { ccAdvance(lex); return ccMake(lex,CC_TOK_DEC,start,2); }
                  if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_MINUS_ASSIGN,start,2); }
                  if (*lex->pos=='>') { ccAdvance(lex); return ccMake(lex,CC_TOK_ARROW,start,2); }
                  return ccMake(lex, CC_TOK_MINUS, start, 1);
        case '*': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_STAR_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_STAR, start, 1);
        case '/': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_SLASH_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_SLASH, start, 1);
        case '%': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_PERCENT_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_PERCENT, start, 1);
        case '=': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_EQ,start,2); }
                  return ccMake(lex, CC_TOK_ASSIGN, start, 1);
        case '!': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_NE,start,2); }
                  return ccMake(lex, CC_TOK_BANG, start, 1);
        case '<': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_LE,start,2); }
                  if (*lex->pos=='<') { ccAdvance(lex); return ccMake(lex,CC_TOK_LSHIFT,start,2); }
                  return ccMake(lex, CC_TOK_LT, start, 1);
        case '>': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_GE,start,2); }
                  if (*lex->pos=='>') { ccAdvance(lex); return ccMake(lex,CC_TOK_RSHIFT,start,2); }
                  return ccMake(lex, CC_TOK_GT, start, 1);
        case '&': if (*lex->pos=='&') { ccAdvance(lex); return ccMake(lex,CC_TOK_AND,start,2); }
                  if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_AMP_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_AMP, start, 1);
        case '|': if (*lex->pos=='|') { ccAdvance(lex); return ccMake(lex,CC_TOK_OR,start,2); }
                  if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_PIPE_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_PIPE, start, 1);
        case '^': if (*lex->pos=='=') { ccAdvance(lex); return ccMake(lex,CC_TOK_CARET_ASSIGN,start,2); }
                  return ccMake(lex, CC_TOK_CARET, start, 1);
        case '.': if (*lex->pos=='.' && lex->pos[1]=='.') { ccAdvance(lex); ccAdvance(lex); return ccMake(lex,CC_TOK_ELLIPSIS,start,3); }
                  return ccMake(lex, CC_TOK_DOT, start, 1);
        default: return ccMake(lex, CC_TOK_ERROR, start, 1);
    }
}

CcToken ccLexerPeek(CcLexer *lex) {
    if (!lex->hasPeeked) { lex->peeked = ccLexerNext(lex); lex->hasPeeked = true; }
    return lex->peeked;
}
