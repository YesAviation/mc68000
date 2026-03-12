/* semantics.h — Semantic analysis (type checking, symbol resolution) */
#ifndef M68K_CC_SEMANTICS_H
#define M68K_CC_SEMANTICS_H

#include "compiler/frontend/ast.h"
#include "compiler/frontend/cc_types.h"

typedef struct CcSema CcSema;

CcSema *ccSemaCreate(void);
void    ccSemaDestroy(CcSema *s);

/* Run semantic analysis on AST; annotates nodes with type info */
bool ccSemaAnalyze(CcSema *s, AstNode *root);

int         ccSemaGetErrorCount(CcSema *s);
const char *ccSemaGetError(CcSema *s, int index);

#endif /* M68K_CC_SEMANTICS_H */
