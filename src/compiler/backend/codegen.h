/* codegen.h — MC68000 code generator */
#ifndef M68K_CC_CODEGEN_H
#define M68K_CC_CODEGEN_H

#include "common/types.h"
#include "common/buffer.h"
#include "compiler/middle/ir.h"

typedef struct CodeGen CodeGen;

CodeGen *codeGenCreate(void);
void     codeGenDestroy(CodeGen *cg);

/* Generate assembly text from IR module */
bool codeGenEmit(CodeGen *cg, IrModule *module, Buffer *output);

/* Get assembly as string (null-terminated) */
char *codeGenGetAsm(CodeGen *cg, IrModule *module);

#endif /* M68K_CC_CODEGEN_H */
