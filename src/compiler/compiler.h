/* compiler.h — C compiler targeting MC68000 assembly */
#ifndef M68K_COMPILER_H
#define M68K_COMPILER_H

#include "common/types.h"

/*
 * Pipeline:  C source → Lexer → Parser → AST → Semantic analysis
 *                    → IR → Optimiser → 68k Code Generator → Assembly text
 *
 * The output is Motorola-syntax assembly suitable for our assembler.
 */

typedef struct Compiler Compiler;

Compiler *compilerCreate(void);
void      compilerDestroy(Compiler *cc);

/* Compile a single C source file; returns true on success */
bool compilerCompileFile(Compiler *cc, const char *inputPath, const char *outputPath);
bool compilerCompileString(Compiler *cc, const char *source, const char *filename,
                           char **outAsm, u32 *outLen);

/* Error reporting */
int         compilerGetErrorCount(Compiler *cc);
const char *compilerGetError(Compiler *cc, int index);

/* Options */
void compilerSetOptLevel(Compiler *cc, int level);  /* 0-2 */

#endif /* M68K_COMPILER_H */
