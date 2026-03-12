/* encoder.h — Instruction encoder (assembly → machine code) */
#ifndef M68K_ASM_ENCODER_H
#define M68K_ASM_ENCODER_H

#include "common/types.h"
#include "common/buffer.h"
#include "assembler/parser.h"
#include "assembler/symbols.h"

/* Encode a parsed instruction line into machine code.
 * Returns the number of bytes emitted. */
u32 encoderEncode(const AsmLine *line, SymbolTable *symbols,
                  Buffer *output, u32 pc);

/* Calculate instruction size without emitting (for pass 1). */
u32 encoderCalcSize(const AsmLine *line, SymbolTable *symbols, u32 pc, int pass);

#endif /* M68K_ASM_ENCODER_H */
