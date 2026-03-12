/* lexer.h — Assembly language lexer */
#ifndef M68K_ASM_LEXER_H
#define M68K_ASM_LEXER_H

#include "common/types.h"

typedef enum {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_IDENTIFIER,       /* label, mnemonic, register name */
    TOK_NUMBER,           /* decimal, hex ($xx or 0xNN), octal (@nn), binary (%bb) */
    TOK_STRING,           /* "..." or '...' */
    TOK_DOT,              /* .size suffix */
    TOK_COLON,            /* label: */
    TOK_COMMA,
    TOK_HASH,             /* # immediate */
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,             /* * (also "current PC") */
    TOK_SLASH,
    TOK_AMPERSAND,
    TOK_PIPE,
    TOK_CARET,
    TOK_TILDE,
    TOK_LSHIFT,           /* << */
    TOK_RSHIFT,           /* >> */
    TOK_EQUALS,           /* = (for EQU / SET shorthand) */
    TOK_COMMENT,
    TOK_ERROR
} AsmTokenType;

typedef struct {
    AsmTokenType type;
    const char  *start;     /* pointer into source */
    int          length;
    s64          numValue;  /* valid when type == TOK_NUMBER */
    int          line;
    int          col;
} AsmToken;

typedef struct AsmLexer AsmLexer;

AsmLexer *asmLexerCreate(void);
void      asmLexerDestroy(AsmLexer *lex);
void      asmLexerSetInput(AsmLexer *lex, const char *source, const char *filename);
bool      asmLexerHasMore(AsmLexer *lex);
AsmToken  asmLexerNext(AsmLexer *lex);
AsmToken  asmLexerPeek(AsmLexer *lex);
int       asmLexerGetLine(AsmLexer *lex);
const char *asmLexerGetFilename(AsmLexer *lex);

#endif /* M68K_ASM_LEXER_H */
