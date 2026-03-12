/* cc_parser.h — C language parser (recursive descent) */
#ifndef M68K_CC_PARSER_H
#define M68K_CC_PARSER_H

#include "common/types.h"
#include "compiler/frontend/cc_lexer.h"
#include "compiler/frontend/ast.h"

typedef struct CcParser CcParser;

CcParser  *ccParserCreate(void);
void       ccParserDestroy(CcParser *p);

/* Parse a complete translation unit; returns AST root */
AstNode *ccParserParse(CcParser *p, CcLexer *lex);

int         ccParserGetErrorCount(CcParser *p);
const char *ccParserGetError(CcParser *p, int index);

#endif /* M68K_CC_PARSER_H */
