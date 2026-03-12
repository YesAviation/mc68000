/* cc_lexer.h — C language lexer */
#ifndef M68K_CC_LEXER_H
#define M68K_CC_LEXER_H

#include "common/types.h"

typedef enum {
    /* Special */
    CC_TOK_EOF = 0, CC_TOK_ERROR,
    /* Literals */
    CC_TOK_INT_LIT, CC_TOK_CHAR_LIT, CC_TOK_STRING_LIT, CC_TOK_FLOAT_LIT,
    /* Identifier */
    CC_TOK_IDENT,
    /* Keywords (C89/C99 subset) */
    CC_TOK_AUTO, CC_TOK_BREAK, CC_TOK_CASE, CC_TOK_CHAR, CC_TOK_CONST,
    CC_TOK_CONTINUE, CC_TOK_DEFAULT, CC_TOK_DO, CC_TOK_DOUBLE, CC_TOK_ELSE,
    CC_TOK_ENUM, CC_TOK_EXTERN, CC_TOK_FLOAT, CC_TOK_FOR, CC_TOK_GOTO,
    CC_TOK_IF, CC_TOK_INT, CC_TOK_LONG, CC_TOK_REGISTER, CC_TOK_RETURN,
    CC_TOK_SHORT, CC_TOK_SIGNED, CC_TOK_SIZEOF, CC_TOK_STATIC,
    CC_TOK_STRUCT, CC_TOK_SWITCH, CC_TOK_TYPEDEF, CC_TOK_UNION,
    CC_TOK_UNSIGNED, CC_TOK_VOID, CC_TOK_VOLATILE, CC_TOK_WHILE,
    /* Punctuation / Operators */
    CC_TOK_LPAREN, CC_TOK_RPAREN, CC_TOK_LBRACE, CC_TOK_RBRACE,
    CC_TOK_LBRACKET, CC_TOK_RBRACKET,
    CC_TOK_SEMICOLON, CC_TOK_COMMA, CC_TOK_DOT, CC_TOK_ARROW,
    CC_TOK_PLUS, CC_TOK_MINUS, CC_TOK_STAR, CC_TOK_SLASH, CC_TOK_PERCENT,
    CC_TOK_AMP, CC_TOK_PIPE, CC_TOK_CARET, CC_TOK_TILDE,
    CC_TOK_BANG, CC_TOK_ASSIGN,
    CC_TOK_EQ, CC_TOK_NE, CC_TOK_LT, CC_TOK_GT, CC_TOK_LE, CC_TOK_GE,
    CC_TOK_AND, CC_TOK_OR,
    CC_TOK_LSHIFT, CC_TOK_RSHIFT,
    CC_TOK_INC, CC_TOK_DEC,
    CC_TOK_PLUS_ASSIGN, CC_TOK_MINUS_ASSIGN, CC_TOK_STAR_ASSIGN,
    CC_TOK_SLASH_ASSIGN, CC_TOK_PERCENT_ASSIGN,
    CC_TOK_AMP_ASSIGN, CC_TOK_PIPE_ASSIGN, CC_TOK_CARET_ASSIGN,
    CC_TOK_LSHIFT_ASSIGN, CC_TOK_RSHIFT_ASSIGN,
    CC_TOK_QUESTION, CC_TOK_COLON, CC_TOK_ELLIPSIS, CC_TOK_HASH
} CcTokenType;

typedef struct {
    CcTokenType  type;
    const char  *start;
    int          length;
    s64          intValue;
    double       floatValue;
    int          line;
    int          col;
} CcToken;

typedef struct CcLexer CcLexer;

CcLexer *ccLexerCreate(void);
void     ccLexerDestroy(CcLexer *lex);
void     ccLexerSetInput(CcLexer *lex, const char *source, const char *filename);
CcToken  ccLexerNext(CcLexer *lex);
CcToken  ccLexerPeek(CcLexer *lex);
int      ccLexerGetLine(CcLexer *lex);

#endif /* M68K_CC_LEXER_H */
