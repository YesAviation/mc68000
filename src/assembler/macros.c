/* macros.c — Assembler macro table */
#include "assembler/macros.h"
#include <stdlib.h>
#include <string.h>

#define MACROS_INIT_CAP 64

struct MacroTable {
    Macro *entries;
    int    count;
    int    capacity;
};

MacroTable *macroTableCreate(void) {
    MacroTable *mt = calloc(1, sizeof(MacroTable));
    mt->capacity = MACROS_INIT_CAP;
    mt->entries  = calloc((size_t)mt->capacity, sizeof(Macro));
    return mt;
}

void macroTableDestroy(MacroTable *mt) {
    if (!mt) return;
    free(mt->entries);
    free(mt);
}

void macroTableClear(MacroTable *mt) { mt->count = 0; }

Macro *macroTableLookup(MacroTable *mt, const char *name) {
    for (int i = 0; i < mt->count; i++) {
        if (strcasecmp(mt->entries[i].name, name) == 0)
            return &mt->entries[i];
    }
    return NULL;
}

bool macroTableDefine(MacroTable *mt, const char *name, const char *body,
                      const char params[][MACRO_MAX_NAME], int paramCount) {
    if (macroTableLookup(mt, name)) return false; /* no redefinition */
    if (mt->count >= mt->capacity) {
        mt->capacity *= 2;
        mt->entries = realloc(mt->entries, (size_t)mt->capacity * sizeof(Macro));
    }
    Macro *m = &mt->entries[mt->count++];
    memset(m, 0, sizeof(*m));
    strncpy(m->name, name, MACRO_MAX_NAME - 1);
    if (body) strncpy(m->body, body, MACRO_MAX_BODY - 1);
    m->paramCount = paramCount;
    for (int i = 0; i < paramCount && i < MACRO_MAX_PARAMS; i++) {
        strncpy(m->params[i], params[i], MACRO_MAX_NAME - 1);
    }
    return true;
}

char *macroExpand(Macro *m, const char *args[], int argCount) {
    /* Simple text substitution: replace \1..\9 or named params with arguments */
    size_t bodyLen = strlen(m->body);
    size_t outCap  = bodyLen * 2 + 256;
    char  *out     = malloc(outCap);
    size_t outPos  = 0;

    for (size_t i = 0; i < bodyLen; i++) {
        if (m->body[i] == '\\' && i + 1 < bodyLen) {
            char next = m->body[i + 1];
            if (next >= '1' && next <= '9') {
                int idx = next - '1';
                if (idx < argCount && args[idx]) {
                    size_t argLen = strlen(args[idx]);
                    while (outPos + argLen >= outCap) { outCap *= 2; out = realloc(out, outCap); }
                    memcpy(out + outPos, args[idx], argLen);
                    outPos += argLen;
                }
                i++; /* skip next char */
                continue;
            }
        }
        if (outPos + 1 >= outCap) { outCap *= 2; out = realloc(out, outCap); }
        out[outPos++] = m->body[i];
    }
    out[outPos] = '\0';
    return out;
}
