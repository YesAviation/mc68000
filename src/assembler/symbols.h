/* symbols.h — Assembler symbol table */
#ifndef M68K_ASM_SYMBOLS_H
#define M68K_ASM_SYMBOLS_H

#include "common/types.h"

#define SYMBOL_MAX_NAME 64

typedef enum {
    SYM_LABEL,      /* address label          */
    SYM_EQU,        /* immutable constant     */
    SYM_SET,        /* reassignable constant  */
    SYM_MACRO       /* macro name             */
} SymbolKind;

typedef struct {
    char       name[SYMBOL_MAX_NAME];
    SymbolKind kind;
    s64        value;
    bool       defined;
    int        definedLine;
    const char *definedFile;
} Symbol;

typedef struct SymbolTable SymbolTable;

SymbolTable *symbolTableCreate(void);
void         symbolTableDestroy(SymbolTable *st);
void         symbolTableClear(SymbolTable *st);

/* Define or redefine (SET allows redefinition) */
bool symbolTableDefine(SymbolTable *st, const char *name, s64 value);
bool symbolTableDefineKind(SymbolTable *st, const char *name, s64 value, SymbolKind kind);

/* Lookup */
Symbol *symbolTableLookup(SymbolTable *st, const char *name);
bool    symbolTableResolve(SymbolTable *st, const char *name, s64 *outValue);

/* Iteration */
int     symbolTableCount(SymbolTable *st);
Symbol *symbolTableGetByIndex(SymbolTable *st, int index);

#endif /* M68K_ASM_SYMBOLS_H */
