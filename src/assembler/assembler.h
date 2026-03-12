/* assembler.h — MC68000 macro assembler public API */
#ifndef M68K_ASSEMBLER_H
#define M68K_ASSEMBLER_H

#include "common/types.h"
#include "common/buffer.h"

/*
 * Two-pass assembler:
 *   Pass 1 — collect symbols, resolve forward references, expand macros
 *   Pass 2 — encode instructions, emit binary
 *
 * Supported syntax (Motorola-style):
 *   label:  MNEMONIC.SIZE  operands   ; comment
 *
 * Directives:
 *   ORG, DC.B/W/L, DS.B/W/L, EQU, SET, MACRO/ENDM,
 *   REPT/ENDR, IF/ELSE/ENDIF, INCLUDE, INCBIN,
 *   SECTION, ALIGN, END, EVEN, ODD
 */

typedef struct Assembler Assembler;

/* Create / destroy */
Assembler *asmCreate(void);
void       asmDestroy(Assembler *as);

/* Configuration */
void asmSetOrigin(Assembler *as, u32 origin);
void asmSetListing(Assembler *as, bool enable);

/* Assembly */
bool asmAssembleFile(Assembler *as, const char *path);
bool asmAssembleString(Assembler *as, const char *source, const char *filename);

/* Output */
bool asmWriteBinary(Assembler *as, const char *path);
bool asmWriteSRecord(Assembler *as, const char *path);   /* Motorola S-record */

/* Access results */
const u8 *asmGetOutput(Assembler *as, u32 *outSize);
u32       asmGetEntryPoint(Assembler *as);
int       asmGetErrorCount(Assembler *as);
const char *asmGetError(Assembler *as, int index);

#endif /* M68K_ASSEMBLER_H */
