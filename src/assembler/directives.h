/* directives.h — Assembler directives */
#ifndef M68K_ASM_DIRECTIVES_H
#define M68K_ASM_DIRECTIVES_H

#include "common/types.h"
#include "common/buffer.h"
#include "assembler/parser.h"
#include "assembler/symbols.h"

/* Process a directive during assembly.
 * Returns the number of bytes emitted (or PC advance). */
u32 directiveProcess(const AsmLine *line, SymbolTable *symbols,
                     Buffer *output, u32 pc, int pass);

/* Calculate the size of a directive without emitting */
u32 directiveCalcSize(const AsmLine *line, SymbolTable *symbols, u32 pc, int pass);

#endif /* M68K_ASM_DIRECTIVES_H */
