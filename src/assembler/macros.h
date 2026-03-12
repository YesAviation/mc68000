/* macros.h — Assembler macro table */
#ifndef M68K_ASM_MACROS_H
#define M68K_ASM_MACROS_H

#include "common/types.h"

#define MACRO_MAX_NAME   64
#define MACRO_MAX_PARAMS 16
#define MACRO_MAX_BODY   4096

typedef struct {
    char name[MACRO_MAX_NAME];
    char params[MACRO_MAX_PARAMS][MACRO_MAX_NAME];
    int  paramCount;
    char body[MACRO_MAX_BODY];
} Macro;

typedef struct MacroTable MacroTable;

MacroTable *macroTableCreate(void);
void        macroTableDestroy(MacroTable *mt);
void        macroTableClear(MacroTable *mt);

bool   macroTableDefine(MacroTable *mt, const char *name, const char *body,
                        const char params[][MACRO_MAX_NAME], int paramCount);
Macro *macroTableLookup(MacroTable *mt, const char *name);

/* Expand a macro call with arguments, returns allocated string (caller frees) */
char *macroExpand(Macro *m, const char *args[], int argCount);

#endif /* M68K_ASM_MACROS_H */
