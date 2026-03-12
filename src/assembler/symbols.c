/* symbols.c — Assembler symbol table (linear search — fine for ≤ 10K symbols) */
#include "assembler/symbols.h"
#include <stdlib.h>
#include <string.h>

#define SYMBOLS_INIT_CAP 512

struct SymbolTable {
    Symbol *entries;
    int     count;
    int     capacity;
};

SymbolTable *symbolTableCreate(void) {
    SymbolTable *st = calloc(1, sizeof(SymbolTable));
    st->capacity = SYMBOLS_INIT_CAP;
    st->entries  = calloc((size_t)st->capacity, sizeof(Symbol));
    return st;
}

void symbolTableDestroy(SymbolTable *st) {
    if (!st) return;
    free(st->entries);
    free(st);
}

void symbolTableClear(SymbolTable *st) { st->count = 0; }

Symbol *symbolTableLookup(SymbolTable *st, const char *name) {
    for (int i = 0; i < st->count; i++) {
        if (strcasecmp(st->entries[i].name, name) == 0)
            return &st->entries[i];
    }
    return NULL;
}

bool symbolTableDefine(SymbolTable *st, const char *name, s64 value) {
    return symbolTableDefineKind(st, name, value, SYM_LABEL);
}

bool symbolTableDefineKind(SymbolTable *st, const char *name, s64 value, SymbolKind kind) {
    Symbol *existing = symbolTableLookup(st, name);
    if (existing) {
        if (existing->kind == SYM_EQU && existing->defined) return false; /* can't redefine EQU */
        existing->value = value;
        existing->defined = true;
        existing->kind = kind;
        return true;
    }
    if (st->count >= st->capacity) {
        st->capacity *= 2;
        st->entries = realloc(st->entries, (size_t)st->capacity * sizeof(Symbol));
    }
    Symbol *s = &st->entries[st->count++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, SYMBOL_MAX_NAME - 1);
    s->kind = kind;
    s->value = value;
    s->defined = true;
    return true;
}

bool symbolTableResolve(SymbolTable *st, const char *name, s64 *outValue) {
    Symbol *s = symbolTableLookup(st, name);
    if (s && s->defined) { *outValue = s->value; return true; }
    return false;
}

int symbolTableCount(SymbolTable *st) { return st->count; }
Symbol *symbolTableGetByIndex(SymbolTable *st, int index) {
    if (index < 0 || index >= st->count) return NULL;
    return &st->entries[index];
}
