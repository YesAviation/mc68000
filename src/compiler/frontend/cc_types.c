/* cc_types.c — C type system */
#include "compiler/frontend/cc_types.h"
#include <stdlib.h>
#include <string.h>

/* ── primitive singletons ────────────────────────────── */
static CcType sVoid  = { .kind = CC_TYPE_VOID,  .size = 0, .alignment = 1 };
static CcType sChar  = { .kind = CC_TYPE_CHAR,  .size = 1, .alignment = 1 };
static CcType sShort = { .kind = CC_TYPE_SHORT, .size = 2, .alignment = 2 };
static CcType sInt   = { .kind = CC_TYPE_INT,   .size = 2, .alignment = 2 };
static CcType sLong  = { .kind = CC_TYPE_LONG,  .size = 4, .alignment = 2 };

CcType *ccTypeVoid(void)  { return &sVoid; }
CcType *ccTypeChar(void)  { return &sChar; }
CcType *ccTypeShort(void) { return &sShort; }
CcType *ccTypeInt(void)   { return &sInt; }
CcType *ccTypeLong(void)  { return &sLong; }

CcType *ccTypeCreate(CcTypeKind kind) {
    CcType *t = calloc(1, sizeof(CcType));
    t->kind = kind;
    switch (kind) {
        case CC_TYPE_VOID:   t->size = 0; t->alignment = 1; break;
        case CC_TYPE_CHAR:   t->size = 1; t->alignment = 1; break;
        case CC_TYPE_SHORT:  t->size = 2; t->alignment = 2; break;
        case CC_TYPE_INT:    t->size = 2; t->alignment = 2; break;
        case CC_TYPE_LONG:   t->size = 4; t->alignment = 2; break;
        case CC_TYPE_FLOAT:  t->size = 4; t->alignment = 2; break;
        case CC_TYPE_DOUBLE: t->size = 8; t->alignment = 2; break;
        case CC_TYPE_POINTER:t->size = 4; t->alignment = 2; break;
        case CC_TYPE_ENUM:   t->size = 2; t->alignment = 2; break;
        default: break;
    }
    return t;
}

CcType *ccTypePointerTo(CcType *base) {
    CcType *t = ccTypeCreate(CC_TYPE_POINTER);
    t->base = base;
    return t;
}

CcType *ccTypeArrayOf(CcType *base, u32 size) {
    CcType *t = ccTypeCreate(CC_TYPE_ARRAY);
    t->base = base;
    t->arraySize = size;
    t->size = base->size * size;
    t->alignment = base->alignment;
    return t;
}

void ccTypeDestroy(CcType *t) {
    if (!t) return;
    /* Don't free singletons */
    if (t == &sVoid || t == &sChar || t == &sShort || t == &sInt || t == &sLong) return;
    free(t);
}

u32 ccTypeGetSize(CcType *t)      { return t ? t->size : 0; }
u32 ccTypeGetAlignment(CcType *t) { return t ? t->alignment : 1; }

bool ccTypeIsIntegral(CcType *t) {
    return t && (t->kind == CC_TYPE_CHAR || t->kind == CC_TYPE_SHORT ||
                 t->kind == CC_TYPE_INT  || t->kind == CC_TYPE_LONG ||
                 t->kind == CC_TYPE_ENUM);
}

bool ccTypeIsArithmetic(CcType *t) {
    return ccTypeIsIntegral(t) ||
           (t && (t->kind == CC_TYPE_FLOAT || t->kind == CC_TYPE_DOUBLE));
}

bool ccTypeIsPointer(CcType *t) {
    return t && (t->kind == CC_TYPE_POINTER || t->kind == CC_TYPE_ARRAY);
}

bool ccTypeIsCompatible(CcType *a, CcType *b) {
    if (!a || !b) return false;
    if (a->kind == b->kind) return true;
    /* implicit conversions between integral types */
    if (ccTypeIsIntegral(a) && ccTypeIsIntegral(b)) return true;
    return false;
}
