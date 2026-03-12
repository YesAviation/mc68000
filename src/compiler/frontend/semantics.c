/* semantics.c — Semantic analysis: type checking & symbol resolution
 *
 * Walks the AST produced by the parser and:
 *   1. Builds a scope chain / symbol table
 *   2. Resolves identifiers to their declarations
 *   3. Type-checks expressions and annotates nodes with resolvedType
 *   4. Validates control flow (break/continue context)
 *   5. Applies implicit type promotions
 */
#include "compiler/frontend/semantics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERRORS  256
#define MAX_SYMBOLS 512
#define MAX_SCOPES   64

/* ── Symbol table ────────────────────────────────────── */

typedef struct {
    char    name[AST_MAX_NAME];
    CcType *type;
    bool    isFunction;
    int     scopeLevel;
} Symbol;

typedef struct {
    Symbol syms[MAX_SYMBOLS];
    int    count;
    int    scopeLevel;
} SymTable;

static void symInit(SymTable *st)  { st->count = 0; st->scopeLevel = 0; }
static void symPush(SymTable *st)  { st->scopeLevel++; }
static void symPop(SymTable *st) {
    /* Remove symbols at current scope level */
    while (st->count > 0 && st->syms[st->count - 1].scopeLevel == st->scopeLevel)
        st->count--;
    st->scopeLevel--;
}
static void symAdd(SymTable *st, const char *name, CcType *type, bool isFunc) {
    if (st->count >= MAX_SYMBOLS) return;
    Symbol *s = &st->syms[st->count++];
    strncpy(s->name, name, AST_MAX_NAME - 1);
    s->type = type;
    s->isFunction = isFunc;
    s->scopeLevel = st->scopeLevel;
}
static Symbol *symLookup(SymTable *st, const char *name) {
    /* Search from most recent (innermost scope) to oldest */
    for (int i = st->count - 1; i >= 0; i--) {
        if (strcmp(st->syms[i].name, name) == 0)
            return &st->syms[i];
    }
    return NULL;
}

/* ── Sema context ────────────────────────────────────── */

struct CcSema {
    char    *errors[MAX_ERRORS];
    int      errorCount;
    SymTable syms;
    int      loopDepth;   /* for break/continue validation */
    int      switchDepth; /* for case/default validation */
    CcType  *currentReturnType;
};

static void semaError(CcSema *s, int line, const char *fmt, ...) {
    if (s->errorCount >= MAX_ERRORS) return;
    char buf[512];
    int off = snprintf(buf, sizeof(buf), "line %d: ", line);
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf + off, sizeof(buf) - (size_t)off, fmt, ap);
    va_end(ap);
    s->errors[s->errorCount++] = strdup(buf);
}

/* ── Type resolution from AST_TYPE_SPEC ──────────────── */

static CcType *resolveType(CcSema *s, AstNode *node) {
    if (!node) return ccTypeInt();

    if (node->type == AST_POINTER) {
        CcType *base = resolveType(s, node->childCount > 0 ? node->children[0] : NULL);
        return ccTypePointerTo(base);
    }
    if (node->type == AST_ARRAY) {
        CcType *base = resolveType(s, node->childCount > 0 ? node->children[0] : NULL);
        return ccTypeArrayOf(base, (u32)node->intValue);
    }

    if (node->type != AST_TYPE_SPEC) return ccTypeInt();

    /* Check struct/union first */
    if (strncmp(node->name, "struct", 6) == 0 || strncmp(node->name, "union", 5) == 0) {
        bool isStruct = (node->name[0] == 's');
        CcType *t = ccTypeCreate(isStruct ? CC_TYPE_STRUCT : CC_TYPE_UNION);
        if (node->childCount > 0 && node->children[0]) {
            AstNode *def = node->children[0];
            strncpy(t->name, def->name, CC_TYPE_MAX_NAME - 1);
            /* Resolve members */
            u32 offset = 0;
            for (int i = 0; i < def->childCount && t->memberCount < CC_TYPE_MAX_MEMBERS; i++) {
                AstNode *mem = def->children[i];
                if (!mem) continue;
                CcType *mt = resolveType(s, mem->childCount > 0 ? mem->children[0] : NULL);
                strncpy(t->members[t->memberCount].name, mem->name, CC_TYPE_MAX_NAME - 1);
                t->members[t->memberCount].type = mt;
                t->members[t->memberCount].offset = offset;
                t->memberCount++;
                if (isStruct) {
                    u32 msz = ccTypeGetSize(mt);
                    if (mem->intValue > 0) msz *= (u32)mem->intValue; /* array */
                    offset += msz;
                    /* Align to member alignment */
                    u32 al = ccTypeGetAlignment(mt);
                    if (al > 1 && (offset & (al - 1)))
                        offset = (offset + al - 1) & ~(al - 1);
                } else {
                    /* Union: size = max member size */
                    u32 msz = ccTypeGetSize(mt);
                    if (msz > offset) offset = msz;
                }
            }
            t->size = offset;
            t->alignment = 2; /* MC68000 word-aligned */
        }
        return t;
    }

    if (strncmp(node->name, "enum", 4) == 0) {
        return ccTypeCreate(CC_TYPE_ENUM); /* enum = int-sized */
    }

    /* Primitive types */
    if (strcmp(node->name, "void") == 0)   return ccTypeVoid();
    if (strcmp(node->name, "char") == 0) {
        CcType *t = ccTypeChar();
        if (node->intValue & 0x01) { /* unsigned flag */
            CcType *ut = ccTypeCreate(CC_TYPE_CHAR);
            ut->isUnsigned = true;
            return ut;
        }
        return t;
    }
    if (strcmp(node->name, "short") == 0) {
        CcType *t = ccTypeShort();
        if (node->intValue & 0x01) {
            CcType *ut = ccTypeCreate(CC_TYPE_SHORT);
            ut->isUnsigned = true;
            return ut;
        }
        return t;
    }
    if (strcmp(node->name, "long") == 0) {
        CcType *t = ccTypeLong();
        if (node->intValue & 0x01) {
            CcType *ut = ccTypeCreate(CC_TYPE_LONG);
            ut->isUnsigned = true;
            return ut;
        }
        return t;
    }
    if (strcmp(node->name, "float") == 0)  return ccTypeCreate(CC_TYPE_FLOAT);
    if (strcmp(node->name, "double") == 0) return ccTypeCreate(CC_TYPE_DOUBLE);

    /* Default: int */
    CcType *t = ccTypeInt();
    if (node->intValue & 0x01) {
        CcType *ut = ccTypeCreate(CC_TYPE_INT);
        ut->isUnsigned = true;
        return ut;
    }

    /* Could be a typedef name — look it up */
    Symbol *sym = symLookup(&s->syms, node->name);
    if (sym) return sym->type;

    return t;
}

/* ── Forward declarations ────────────────────────────── */

static CcType *analyzeExpr(CcSema *s, AstNode *node);
static void    analyzeStmt(CcSema *s, AstNode *node);

/* ── Expression analysis ─────────────────────────────── */

static CcType *analyzeExpr(CcSema *s, AstNode *node) {
    if (!node) return ccTypeVoid();

    switch (node->type) {
        case AST_INT_LITERAL:
            node->resolvedType = ccTypeInt();
            if (node->intValue > 32767 || node->intValue < -32768)
                node->resolvedType = ccTypeLong();
            return node->resolvedType;

        case AST_CHAR_LITERAL:
            node->resolvedType = ccTypeChar();
            return node->resolvedType;

        case AST_STRING_LITERAL:
            node->resolvedType = ccTypePointerTo(ccTypeChar());
            return node->resolvedType;

        case AST_FLOAT_LITERAL:
            node->resolvedType = ccTypeCreate(CC_TYPE_DOUBLE);
            return node->resolvedType;

        case AST_IDENT: {
            Symbol *sym = symLookup(&s->syms, node->name);
            if (!sym) {
                semaError(s, node->line, "undeclared identifier '%s'", node->name);
                node->resolvedType = ccTypeInt();
            } else {
                node->resolvedType = sym->type;
            }
            return node->resolvedType;
        }

        case AST_BINARY_OP: {
            CcType *lt = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            CcType *rt = analyzeExpr(s, node->childCount > 1 ? node->children[1] : NULL);
            /* Pointer arithmetic */
            if (ccTypeIsPointer(lt) && ccTypeIsIntegral(rt)) {
                node->resolvedType = lt;
            } else if (ccTypeIsIntegral(lt) && ccTypeIsPointer(rt)) {
                node->resolvedType = rt;
            }
            /* Comparison operators return int */
            else if (strcmp(node->name, "==") == 0 || strcmp(node->name, "!=") == 0 ||
                     strcmp(node->name, "<") == 0  || strcmp(node->name, ">") == 0 ||
                     strcmp(node->name, "<=") == 0 || strcmp(node->name, ">=") == 0 ||
                     strcmp(node->name, "&&") == 0 || strcmp(node->name, "||") == 0) {
                node->resolvedType = ccTypeInt();
            }
            /* Integer promotion: result is the wider type */
            else if (lt && rt) {
                node->resolvedType = (ccTypeGetSize(lt) >= ccTypeGetSize(rt)) ? lt : rt;
            } else {
                node->resolvedType = lt ? lt : ccTypeInt();
            }
            return node->resolvedType;
        }

        case AST_UNARY_OP: {
            CcType *operand = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            if (strcmp(node->name, "&") == 0) {
                node->resolvedType = ccTypePointerTo(operand);
            } else if (strcmp(node->name, "*") == 0) {
                if (operand && ccTypeIsPointer(operand) && operand->base) {
                    node->resolvedType = operand->base;
                } else {
                    semaError(s, node->line, "dereference of non-pointer");
                    node->resolvedType = ccTypeInt();
                }
            } else if (strcmp(node->name, "!") == 0) {
                node->resolvedType = ccTypeInt();
            } else {
                node->resolvedType = operand;
            }
            return node->resolvedType;
        }

        case AST_ASSIGN: {
            CcType *lt = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            analyzeExpr(s, node->childCount > 1 ? node->children[1] : NULL);
            node->resolvedType = lt;
            return node->resolvedType;
        }

        case AST_CALL: {
            CcType *ft = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            /* Analyze arguments */
            for (int i = 1; i < node->childCount; i++)
                analyzeExpr(s, node->children[i]);
            if (ft && ft->kind == CC_TYPE_FUNCTION && ft->returnType) {
                node->resolvedType = ft->returnType;
            } else {
                /* Implicit function declaration returns int */
                node->resolvedType = ccTypeInt();
            }
            return node->resolvedType;
        }

        case AST_INDEX: {
            CcType *arr = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            analyzeExpr(s, node->childCount > 1 ? node->children[1] : NULL);
            if (arr && (arr->kind == CC_TYPE_POINTER || arr->kind == CC_TYPE_ARRAY) && arr->base)
                node->resolvedType = arr->base;
            else
                node->resolvedType = ccTypeInt();
            return node->resolvedType;
        }

        case AST_MEMBER:
        case AST_PTR_MEMBER: {
            CcType *st = analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            CcType *target = st;
            if (node->type == AST_PTR_MEMBER && st && st->kind == CC_TYPE_POINTER)
                target = st->base;
            if (target && (target->kind == CC_TYPE_STRUCT || target->kind == CC_TYPE_UNION)) {
                for (int i = 0; i < target->memberCount; i++) {
                    if (strcmp(target->members[i].name, node->name) == 0) {
                        node->resolvedType = target->members[i].type;
                        return node->resolvedType;
                    }
                }
                semaError(s, node->line, "no member '%s' in struct/union", node->name);
            }
            node->resolvedType = ccTypeInt();
            return node->resolvedType;
        }

        case AST_CAST: {
            CcType *targetType = resolveType(s, node->childCount > 0 ? node->children[0] : NULL);
            analyzeExpr(s, node->childCount > 1 ? node->children[1] : NULL);
            node->resolvedType = targetType;
            return node->resolvedType;
        }

        case AST_SIZEOF:
            node->resolvedType = ccTypeInt(); /* sizeof returns size_t, but int on MC68000 */
            if (node->childCount > 0 && node->children[0]) {
                AstNode *child = node->children[0];
                if (child->type == AST_TYPE_SPEC || child->type == AST_POINTER) {
                    CcType *t = resolveType(s, child);
                    node->intValue = (s64)ccTypeGetSize(t);
                } else {
                    CcType *t = analyzeExpr(s, child);
                    node->intValue = (s64)ccTypeGetSize(t);
                }
            }
            return node->resolvedType;

        case AST_TERNARY: {
            analyzeExpr(s, node->childCount > 0 ? node->children[0] : NULL);
            CcType *t = analyzeExpr(s, node->childCount > 1 ? node->children[1] : NULL);
            analyzeExpr(s, node->childCount > 2 ? node->children[2] : NULL);
            node->resolvedType = t;
            return node->resolvedType;
        }

        case AST_COMMA_EXPR: {
            CcType *last = ccTypeVoid();
            for (int i = 0; i < node->childCount; i++)
                last = analyzeExpr(s, node->children[i]);
            node->resolvedType = last;
            return node->resolvedType;
        }

        default:
            node->resolvedType = ccTypeInt();
            return node->resolvedType;
    }
}

/* ── Statement analysis ──────────────────────────────── */

static void analyzeStmt(CcSema *s, AstNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_COMPOUND_STMT:
            symPush(&s->syms);
            for (int i = 0; i < node->childCount; i++)
                analyzeStmt(s, node->children[i]);
            symPop(&s->syms);
            break;

        case AST_VAR_DECL:
        case AST_TYPEDEF_DECL: {
            CcType *t = resolveType(s, node->childCount > 0 ? node->children[0] : NULL);
            /* Handle array */
            if (node->intValue > 0) {
                t = ccTypeArrayOf(t, (u32)node->intValue);
            }
            node->resolvedType = t;
            if (node->name[0])
                symAdd(&s->syms, node->name, t, false);
            /* Analyze initializer if present */
            if (node->childCount > 1 && node->children[1])
                analyzeExpr(s, node->children[1]);
            break;
        }

        case AST_EXPR_STMT:
            if (node->childCount > 0)
                analyzeExpr(s, node->children[0]);
            break;

        case AST_RETURN_STMT:
            if (node->childCount > 0) {
                CcType *rt = analyzeExpr(s, node->children[0]);
                if (s->currentReturnType && !ccTypeIsCompatible(s->currentReturnType, rt))
                    semaError(s, node->line, "incompatible return type");
            }
            break;

        case AST_IF_STMT:
            if (node->childCount > 0) analyzeExpr(s, node->children[0]);
            if (node->childCount > 1) analyzeStmt(s, node->children[1]);
            if (node->childCount > 2) analyzeStmt(s, node->children[2]);
            break;

        case AST_WHILE_STMT:
            if (node->childCount > 0) analyzeExpr(s, node->children[0]);
            s->loopDepth++;
            if (node->childCount > 1) analyzeStmt(s, node->children[1]);
            s->loopDepth--;
            break;

        case AST_DO_WHILE_STMT:
            s->loopDepth++;
            if (node->childCount > 0) analyzeStmt(s, node->children[0]);
            s->loopDepth--;
            if (node->childCount > 1) analyzeExpr(s, node->children[1]);
            break;

        case AST_FOR_STMT:
            symPush(&s->syms);
            if (node->childCount > 0) analyzeStmt(s, node->children[0]);
            if (node->childCount > 1 && node->children[1]) analyzeExpr(s, node->children[1]);
            if (node->childCount > 2 && node->children[2]) analyzeExpr(s, node->children[2]);
            s->loopDepth++;
            if (node->childCount > 3) analyzeStmt(s, node->children[3]);
            s->loopDepth--;
            symPop(&s->syms);
            break;

        case AST_SWITCH_STMT:
            if (node->childCount > 0) analyzeExpr(s, node->children[0]);
            s->switchDepth++;
            if (node->childCount > 1) analyzeStmt(s, node->children[1]);
            s->switchDepth--;
            break;

        case AST_CASE_STMT:
            if (s->switchDepth <= 0)
                semaError(s, node->line, "'case' outside switch");
            if (node->childCount > 0) analyzeExpr(s, node->children[0]);
            if (node->childCount > 1) analyzeStmt(s, node->children[1]);
            break;

        case AST_DEFAULT_STMT:
            if (s->switchDepth <= 0)
                semaError(s, node->line, "'default' outside switch");
            if (node->childCount > 0) analyzeStmt(s, node->children[0]);
            break;

        case AST_BREAK_STMT:
            if (s->loopDepth <= 0 && s->switchDepth <= 0)
                semaError(s, node->line, "'break' outside loop/switch");
            break;

        case AST_CONTINUE_STMT:
            if (s->loopDepth <= 0)
                semaError(s, node->line, "'continue' outside loop");
            break;

        case AST_LABEL_STMT:
            if (node->childCount > 0) analyzeStmt(s, node->children[0]);
            break;

        case AST_GOTO_STMT:
            /* Label resolution could be done here; skip for now */
            break;

        case AST_FUNCTION_DEF: {
            /* Resolve return type */
            CcType *retType = resolveType(s, node->childCount > 0 ? node->children[0] : NULL);

            /* Build function type */
            CcType *fnType = ccTypeCreate(CC_TYPE_FUNCTION);
            fnType->returnType = retType;
            strncpy(fnType->name, node->name, CC_TYPE_MAX_NAME - 1);

            /* Register function in current scope */
            symAdd(&s->syms, node->name, fnType, true);

            /* Push scope for parameters and body */
            symPush(&s->syms);
            CcType *prevRet = s->currentReturnType;
            s->currentReturnType = retType;

            /* Process parameters */
            AstNode *body = NULL;
            for (int i = 1; i < node->childCount; i++) {
                AstNode *child = node->children[i];
                if (!child) continue;
                if (child->type == AST_PARAM) {
                    CcType *pt = resolveType(s, child->childCount > 0 ? child->children[0] : NULL);
                    child->resolvedType = pt;
                    if (child->name[0])
                        symAdd(&s->syms, child->name, pt, false);
                    if (fnType->paramCount < CC_TYPE_MAX_PARAMS)
                        fnType->params[fnType->paramCount++] = pt;
                } else if (child->type == AST_COMPOUND_STMT) {
                    body = child;
                }
            }
            fnType->isVariadic = (node->intValue != 0);

            /* Analyze body */
            if (body) {
                /* Don't push another scope — compound will do it */
                for (int i = 0; i < body->childCount; i++)
                    analyzeStmt(s, body->children[i]);
            }

            s->currentReturnType = prevRet;
            symPop(&s->syms);
            break;
        }

        default:
            /* Try to recurse into children */
            for (int i = 0; i < node->childCount; i++)
                analyzeStmt(s, node->children[i]);
            break;
    }
}

/* ── Public API ──────────────────────────────────────── */

CcSema *ccSemaCreate(void) { return calloc(1, sizeof(CcSema)); }

void ccSemaDestroy(CcSema *s) {
    if (!s) return;
    for (int i = 0; i < s->errorCount; i++) free(s->errors[i]);
    free(s);
}

int ccSemaGetErrorCount(CcSema *s) { return s->errorCount; }
const char *ccSemaGetError(CcSema *s, int i) {
    return (i >= 0 && i < s->errorCount) ? s->errors[i] : NULL;
}

bool ccSemaAnalyze(CcSema *s, AstNode *root) {
    s->errorCount = 0;
    s->loopDepth = 0;
    s->switchDepth = 0;
    s->currentReturnType = NULL;
    symInit(&s->syms);

    if (!root) return false;

    /* Process each top-level declaration */
    for (int i = 0; i < root->childCount; i++)
        analyzeStmt(s, root->children[i]);

    return s->errorCount == 0;
}
