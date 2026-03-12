/* compiler.c — C compiler top-level driver */
#include "compiler/compiler.h"
#include "compiler/frontend/cc_lexer.h"
#include "compiler/frontend/cc_parser.h"
#include "compiler/frontend/ast.h"
#include "compiler/frontend/semantics.h"
#include "compiler/middle/ir.h"
#include "compiler/middle/optimizer.h"
#include "compiler/backend/codegen.h"
#include "common/buffer.h"
#include "common/log.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERRORS 256

struct Compiler {
    CcLexer   *lexer;
    CcParser  *parser;
    CcSema    *sema;
    CodeGen   *codegen;
    int        optLevel;
    char      *errors[MAX_ERRORS];
    int        errorCount;
};

Compiler *compilerCreate(void) {
    Compiler *cc = calloc(1, sizeof(Compiler));
    cc->lexer   = ccLexerCreate();
    cc->parser  = ccParserCreate();
    cc->sema    = ccSemaCreate();
    cc->codegen = codeGenCreate();
    return cc;
}

void compilerDestroy(Compiler *cc) {
    if (!cc) return;
    ccLexerDestroy(cc->lexer);
    ccParserDestroy(cc->parser);
    ccSemaDestroy(cc->sema);
    codeGenDestroy(cc->codegen);
    for (int i = 0; i < cc->errorCount; i++) free(cc->errors[i]);
    free(cc);
}

void compilerSetOptLevel(Compiler *cc, int level) { cc->optLevel = level; }
int  compilerGetErrorCount(Compiler *cc) { return cc->errorCount; }
const char *compilerGetError(Compiler *cc, int i) {
    return (i >= 0 && i < cc->errorCount) ? cc->errors[i] : NULL;
}

static void ccError(Compiler *cc, const char *fmt, ...) {
    if (cc->errorCount >= MAX_ERRORS) return;
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    cc->errors[cc->errorCount++] = strdup(buf);
}

/* ── AST → IR lowering ───────────────────────────────── */

typedef struct {
    IrModule   *module;
    IrFunction *curFn;
    IrBlock    *curBlk;
    int         labelCount;
    /* Local variable stack offsets: name → stack offset from FP */
    struct { char name[128]; int offset; } locals[256];
    int         localCount;
    int         stackOffset; /* grows negative from FP */
} IrCtx;

static int newTemp(IrCtx *ctx) { return ctx->curFn->tempCount++; }

static char *newLabel(IrCtx *ctx, const char *prefix) {
    char *buf = malloc(64);
    snprintf(buf, 64, "%s_%d", prefix, ctx->labelCount++);
    return buf;
}

static void emitInstr(IrCtx *ctx, IrOpcode op, IrOperand res, IrOperand a1, IrOperand a2, int line) {
    IrInstr ins = { .op = op, .result = res, .arg1 = a1, .arg2 = a2, .line = line };
    irBlockAddInstr(ctx->curBlk, ins);
}

static int lookupLocal(IrCtx *ctx, const char *name) {
    for (int i = 0; i < ctx->localCount; i++)
        if (strcmp(ctx->locals[i].name, name) == 0)
            return ctx->locals[i].offset;
    return 0; /* not found — could be global */
}

static void addLocal(IrCtx *ctx, const char *name, int size) {
    if (ctx->localCount >= 256) return;
    /* Align to word boundary for MC68000 */
    if (size < 2) size = 2;
    ctx->stackOffset -= size;
    strncpy(ctx->locals[ctx->localCount].name, name, 127);
    ctx->locals[ctx->localCount].offset = ctx->stackOffset;
    ctx->localCount++;
}

/* Forward declaration */
static IrOperand lowerExpr(IrCtx *ctx, AstNode *node);

static int typeSize(AstNode *node) {
    if (!node || !node->resolvedType) return 2;
    return (int)ccTypeGetSize(node->resolvedType);
}

static IrOperand lowerExpr(IrCtx *ctx, AstNode *node) {
    if (!node) return irImm(0);

    switch (node->type) {
        case AST_INT_LITERAL:
        case AST_CHAR_LITERAL:
            return irImm(node->intValue);

        case AST_STRING_LITERAL: {
            /* Strings stored as named constants */
            int t = newTemp(ctx);
            emitInstr(ctx, IR_CONST, irTemp(t), irNamed(node->name), irImm(0), node->line);
            return irTemp(t);
        }

        case AST_IDENT: {
            int off = lookupLocal(ctx, node->name);
            if (off != 0) {
                /* Local variable: load from stack */
                int t = newTemp(ctx);
                emitInstr(ctx, IR_LOAD, irTemp(t), irNamed(node->name), irImm(0), node->line);
                return irTemp(t);
            }
            /* Global or function name */
            int t = newTemp(ctx);
            emitInstr(ctx, IR_LOAD, irTemp(t), irNamed(node->name), irImm(0), node->line);
            return irTemp(t);
        }

        case AST_BINARY_OP: {
            IrOperand l = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            IrOperand r = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            int t = newTemp(ctx);
            IrOpcode op = IR_NOP;
            const char *nm = node->name;
            if      (strcmp(nm, "+") == 0)  op = IR_ADD;
            else if (strcmp(nm, "-") == 0)  op = IR_SUB;
            else if (strcmp(nm, "*") == 0)  op = IR_MUL;
            else if (strcmp(nm, "/") == 0)  op = IR_DIV;
            else if (strcmp(nm, "%") == 0)  op = IR_MOD;
            else if (strcmp(nm, "&") == 0)  op = IR_AND;
            else if (strcmp(nm, "|") == 0)  op = IR_OR;
            else if (strcmp(nm, "^") == 0)  op = IR_XOR;
            else if (strcmp(nm, "<<") == 0) op = IR_SHL;
            else if (strcmp(nm, ">>") == 0) op = IR_SHR;
            else if (strcmp(nm, "==") == 0) op = IR_EQ;
            else if (strcmp(nm, "!=") == 0) op = IR_NE;
            else if (strcmp(nm, "<") == 0)  op = IR_LT;
            else if (strcmp(nm, "<=") == 0) op = IR_LE;
            else if (strcmp(nm, ">") == 0)  op = IR_GT;
            else if (strcmp(nm, ">=") == 0) op = IR_GE;
            else if (strcmp(nm, "&&") == 0) {
                /* Short-circuit: if !l goto false; t = r; */
                char *lblF = newLabel(ctx, "and_f");
                char *lblE = newLabel(ctx, "and_e");
                emitInstr(ctx, IR_BRANCHZ, irImm(0), l, irNamed(lblF), node->line);
                emitInstr(ctx, IR_BRANCHZ, irImm(0), r, irNamed(lblF), node->line);
                emitInstr(ctx, IR_CONST, irTemp(t), irImm(1), irImm(0), node->line);
                emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblE), irImm(0), node->line);
                IrBlock *blkF = irFunctionAddBlock(ctx->curFn, lblF);
                ctx->curBlk = blkF;
                emitInstr(ctx, IR_CONST, irTemp(t), irImm(0), irImm(0), node->line);
                IrBlock *blkE = irFunctionAddBlock(ctx->curFn, lblE);
                ctx->curBlk = blkE;
                return irTemp(t);
            }
            else if (strcmp(nm, "||") == 0) {
                char *lblT = newLabel(ctx, "or_t");
                char *lblE = newLabel(ctx, "or_e");
                emitInstr(ctx, IR_BRANCH, irImm(0), l, irNamed(lblT), node->line);
                emitInstr(ctx, IR_BRANCH, irImm(0), r, irNamed(lblT), node->line);
                emitInstr(ctx, IR_CONST, irTemp(t), irImm(0), irImm(0), node->line);
                emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblE), irImm(0), node->line);
                IrBlock *blkT = irFunctionAddBlock(ctx->curFn, lblT);
                ctx->curBlk = blkT;
                emitInstr(ctx, IR_CONST, irTemp(t), irImm(1), irImm(0), node->line);
                IrBlock *blkE = irFunctionAddBlock(ctx->curFn, lblE);
                ctx->curBlk = blkE;
                return irTemp(t);
            }
            emitInstr(ctx, op, irTemp(t), l, r, node->line);
            return irTemp(t);
        }

        case AST_UNARY_OP: {
            IrOperand operand = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            int t = newTemp(ctx);
            const char *nm = node->name;
            if (strcmp(nm, "-") == 0) {
                emitInstr(ctx, IR_NEG, irTemp(t), operand, irImm(0), node->line);
            } else if (strcmp(nm, "~") == 0) {
                emitInstr(ctx, IR_NOT, irTemp(t), operand, irImm(0), node->line);
            } else if (strcmp(nm, "!") == 0) {
                emitInstr(ctx, IR_EQ, irTemp(t), operand, irImm(0), node->line);
            } else if (strcmp(nm, "&") == 0) {
                emitInstr(ctx, IR_ADDR, irTemp(t), operand, irImm(0), node->line);
            } else if (strcmp(nm, "*") == 0) {
                emitInstr(ctx, IR_DEREF, irTemp(t), operand, irImm(0), node->line);
            } else if (strcmp(nm, "pre++") == 0 || strcmp(nm, "post++") == 0) {
                emitInstr(ctx, IR_ADD, irTemp(t), operand, irImm(1), node->line);
                /* Store back */
                if (node->childCount > 0 && node->children[0]->type == AST_IDENT) {
                    emitInstr(ctx, IR_STORE, irImm(0), irNamed(node->children[0]->name), irTemp(t), node->line);
                }
                if (strcmp(nm, "post++") == 0) return operand; /* return old value */
            } else if (strcmp(nm, "pre--") == 0 || strcmp(nm, "post--") == 0) {
                emitInstr(ctx, IR_SUB, irTemp(t), operand, irImm(1), node->line);
                if (node->childCount > 0 && node->children[0]->type == AST_IDENT) {
                    emitInstr(ctx, IR_STORE, irImm(0), irNamed(node->children[0]->name), irTemp(t), node->line);
                }
                if (strcmp(nm, "post--") == 0) return operand;
            } else {
                emitInstr(ctx, IR_MOVE, irTemp(t), operand, irImm(0), node->line);
            }
            return irTemp(t);
        }

        case AST_ASSIGN: {
            IrOperand rhs = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            AstNode *lhsNode = node->childCount > 0 ? node->children[0] : NULL;
            if (lhsNode && lhsNode->type == AST_IDENT) {
                const char *nm = node->name;
                if (strcmp(nm, "=") == 0) {
                    emitInstr(ctx, IR_STORE, irImm(0), irNamed(lhsNode->name), rhs, node->line);
                } else {
                    /* Compound assignment: +=, -=, etc. */
                    IrOperand cur = lowerExpr(ctx, lhsNode);
                    int t = newTemp(ctx);
                    IrOpcode op = IR_NOP;
                    if      (strcmp(nm, "+=") == 0) op = IR_ADD;
                    else if (strcmp(nm, "-=") == 0) op = IR_SUB;
                    else if (strcmp(nm, "*=") == 0) op = IR_MUL;
                    else if (strcmp(nm, "/=") == 0) op = IR_DIV;
                    else if (strcmp(nm, "%=") == 0) op = IR_MOD;
                    else if (strcmp(nm, "&=") == 0) op = IR_AND;
                    else if (strcmp(nm, "|=") == 0) op = IR_OR;
                    else if (strcmp(nm, "^=") == 0) op = IR_XOR;
                    else if (strcmp(nm, "<<=") == 0) op = IR_SHL;
                    else if (strcmp(nm, ">>=") == 0) op = IR_SHR;
                    emitInstr(ctx, op, irTemp(t), cur, rhs, node->line);
                    emitInstr(ctx, IR_STORE, irImm(0), irNamed(lhsNode->name), irTemp(t), node->line);
                    return irTemp(t);
                }
            } else if (lhsNode && lhsNode->type == AST_UNARY_OP && strcmp(lhsNode->name, "*") == 0) {
                /* Pointer store: *ptr = val */
                IrOperand addr = lowerExpr(ctx, lhsNode->childCount > 0 ? lhsNode->children[0] : NULL);
                emitInstr(ctx, IR_STORE, irImm(0), addr, rhs, node->line);
            } else if (lhsNode && lhsNode->type == AST_INDEX) {
                IrOperand base = lowerExpr(ctx, lhsNode->childCount > 0 ? lhsNode->children[0] : NULL);
                IrOperand idx = lowerExpr(ctx, lhsNode->childCount > 1 ? lhsNode->children[1] : NULL);
                int addr = newTemp(ctx);
                int sz = typeSize(lhsNode);
                int sTemp = newTemp(ctx);
                emitInstr(ctx, IR_MUL, irTemp(sTemp), idx, irImm(sz), node->line);
                emitInstr(ctx, IR_ADD, irTemp(addr), base, irTemp(sTemp), node->line);
                emitInstr(ctx, IR_STORE, irImm(0), irTemp(addr), rhs, node->line);
            }
            return rhs;
        }

        case AST_CALL: {
            /* Push arguments right-to-left */
            for (int i = node->childCount - 1; i >= 1; i--) {
                IrOperand arg = lowerExpr(ctx, node->children[i]);
                emitInstr(ctx, IR_PARAM, irImm(0), arg, irImm(0), node->line);
            }
            /* Call function */
            IrOperand func = irImm(0);
            if (node->childCount > 0 && node->children[0]->type == AST_IDENT)
                func = irNamed(node->children[0]->name);
            else
                func = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            int t = newTemp(ctx);
            emitInstr(ctx, IR_CALL, irTemp(t), func, irImm(node->childCount - 1), node->line);
            return irTemp(t);
        }

        case AST_INDEX: {
            IrOperand base = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            IrOperand idx = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            int addr = newTemp(ctx);
            int sz = typeSize(node);
            int sTemp = newTemp(ctx);
            emitInstr(ctx, IR_MUL, irTemp(sTemp), idx, irImm(sz), node->line);
            emitInstr(ctx, IR_ADD, irTemp(addr), base, irTemp(sTemp), node->line);
            int t = newTemp(ctx);
            emitInstr(ctx, IR_DEREF, irTemp(t), irTemp(addr), irImm(0), node->line);
            return irTemp(t);
        }

        case AST_CAST: {
            IrOperand val = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            int t = newTemp(ctx);
            emitInstr(ctx, IR_CAST, irTemp(t), val, irImm(0), node->line);
            return irTemp(t);
        }

        case AST_SIZEOF:
            return irImm(node->intValue > 0 ? node->intValue : 2);

        case AST_TERNARY: {
            IrOperand cond = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            char *lblElse = newLabel(ctx, "ter_e");
            char *lblEnd  = newLabel(ctx, "ter_d");
            int t = newTemp(ctx);
            emitInstr(ctx, IR_BRANCHZ, irImm(0), cond, irNamed(lblElse), node->line);
            IrOperand thenV = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            emitInstr(ctx, IR_MOVE, irTemp(t), thenV, irImm(0), node->line);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblEnd), irImm(0), node->line);
            IrBlock *blkE = irFunctionAddBlock(ctx->curFn, lblElse);
            ctx->curBlk = blkE;
            IrOperand elseV = lowerExpr(ctx, node->childCount > 2 ? node->children[2] : NULL);
            emitInstr(ctx, IR_MOVE, irTemp(t), elseV, irImm(0), node->line);
            IrBlock *blkD = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = blkD;
            return irTemp(t);
        }

        case AST_COMMA_EXPR: {
            IrOperand last = irImm(0);
            for (int i = 0; i < node->childCount; i++)
                last = lowerExpr(ctx, node->children[i]);
            return last;
        }

        case AST_MEMBER:
        case AST_PTR_MEMBER: {
            IrOperand base = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            /* Find member offset from type info */
            int memberOff = 0;
            if (node->childCount > 0 && node->children[0]->resolvedType) {
                CcType *st = node->children[0]->resolvedType;
                if (node->type == AST_PTR_MEMBER && st->kind == CC_TYPE_POINTER)
                    st = st->base;
                if (st) {
                    for (int i = 0; i < st->memberCount; i++) {
                        if (strcmp(st->members[i].name, node->name) == 0) {
                            memberOff = (int)st->members[i].offset;
                            break;
                        }
                    }
                }
            }
            int addr = newTemp(ctx);
            emitInstr(ctx, IR_ADD, irTemp(addr), base, irImm(memberOff), node->line);
            int t = newTemp(ctx);
            emitInstr(ctx, IR_DEREF, irTemp(t), irTemp(addr), irImm(0), node->line);
            return irTemp(t);
        }

        default:
            return irImm(0);
    }
}

/* Statement lowering context also needs break/continue label stack */
#define MAX_LOOP_DEPTH 32

typedef struct {
    IrCtx   *ir;
    char     breakLabels[MAX_LOOP_DEPTH][64];
    char     contLabels[MAX_LOOP_DEPTH][64];
    int      loopDepth;
} StmtCtx;

static void lowerStmt(StmtCtx *sc, AstNode *node);

static void lowerStmt(StmtCtx *sc, AstNode *node) {
    IrCtx *ctx = sc->ir;
    if (!node) return;

    switch (node->type) {
        case AST_COMPOUND_STMT:
            for (int i = 0; i < node->childCount; i++)
                lowerStmt(sc, node->children[i]);
            break;

        case AST_VAR_DECL:
        case AST_TYPEDEF_DECL: {
            if (node->name[0]) {
                int sz = typeSize(node);
                if (node->intValue > 0) sz *= (int)node->intValue; /* array */
                addLocal(ctx, node->name, sz);
            }
            /* Initializer */
            if (node->childCount > 1 && node->children[1]) {
                IrOperand val = lowerExpr(ctx, node->children[1]);
                emitInstr(ctx, IR_STORE, irImm(0), irNamed(node->name), val, node->line);
            }
            break;
        }

        case AST_EXPR_STMT:
            if (node->childCount > 0)
                lowerExpr(ctx, node->children[0]);
            break;

        case AST_RETURN_STMT: {
            if (node->childCount > 0) {
                IrOperand val = lowerExpr(ctx, node->children[0]);
                emitInstr(ctx, IR_RETURN, irImm(0), val, irImm(0), node->line);
            } else {
                emitInstr(ctx, IR_RETURN, irImm(0), irImm(0), irImm(0), node->line);
            }
            break;
        }

        case AST_IF_STMT: {
            IrOperand cond = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            char *lblElse = newLabel(ctx, "if_e");
            char *lblEnd  = newLabel(ctx, "if_d");
            bool hasElse = (node->childCount > 2);
            emitInstr(ctx, IR_BRANCHZ, irImm(0), cond, irNamed(hasElse ? lblElse : lblEnd), node->line);
            if (node->childCount > 1)
                lowerStmt(sc, node->children[1]);
            if (hasElse) {
                emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblEnd), irImm(0), node->line);
                IrBlock *blkE = irFunctionAddBlock(ctx->curFn, lblElse);
                ctx->curBlk = blkE;
                lowerStmt(sc, node->children[2]);
            }
            IrBlock *blkD = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = blkD;
            break;
        }

        case AST_WHILE_STMT: {
            char *lblCond = newLabel(ctx, "wh_c");
            char *lblEnd  = newLabel(ctx, "wh_e");
            /* Push loop context */
            int ld = sc->loopDepth++;
            if (ld < MAX_LOOP_DEPTH) {
                strncpy(sc->breakLabels[ld], lblEnd, 63);
                strncpy(sc->contLabels[ld], lblCond, 63);
            }
            IrBlock *condBlk = irFunctionAddBlock(ctx->curFn, lblCond);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblCond), irImm(0), node->line);
            ctx->curBlk = condBlk;
            IrOperand cond = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            emitInstr(ctx, IR_BRANCHZ, irImm(0), cond, irNamed(lblEnd), node->line);
            if (node->childCount > 1) lowerStmt(sc, node->children[1]);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblCond), irImm(0), node->line);
            IrBlock *endBlk = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = endBlk;
            sc->loopDepth--;
            break;
        }

        case AST_DO_WHILE_STMT: {
            char *lblBody = newLabel(ctx, "do_b");
            char *lblCond = newLabel(ctx, "do_c");
            char *lblEnd  = newLabel(ctx, "do_e");
            int ld = sc->loopDepth++;
            if (ld < MAX_LOOP_DEPTH) {
                strncpy(sc->breakLabels[ld], lblEnd, 63);
                strncpy(sc->contLabels[ld], lblCond, 63);
            }
            IrBlock *bodyBlk = irFunctionAddBlock(ctx->curFn, lblBody);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblBody), irImm(0), node->line);
            ctx->curBlk = bodyBlk;
            if (node->childCount > 0) lowerStmt(sc, node->children[0]);
            IrBlock *condBlk = irFunctionAddBlock(ctx->curFn, lblCond);
            ctx->curBlk = condBlk;
            IrOperand cond = lowerExpr(ctx, node->childCount > 1 ? node->children[1] : NULL);
            emitInstr(ctx, IR_BRANCH, irImm(0), cond, irNamed(lblBody), node->line);
            IrBlock *endBlk = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = endBlk;
            sc->loopDepth--;
            break;
        }

        case AST_FOR_STMT: {
            /* child[0]=init, child[1]=cond, child[2]=incr, child[3]=body */
            char *lblCond = newLabel(ctx, "for_c");
            char *lblIncr = newLabel(ctx, "for_i");
            char *lblEnd  = newLabel(ctx, "for_e");
            int ld = sc->loopDepth++;
            if (ld < MAX_LOOP_DEPTH) {
                strncpy(sc->breakLabels[ld], lblEnd, 63);
                strncpy(sc->contLabels[ld], lblIncr, 63);
            }
            /* Init */
            if (node->childCount > 0 && node->children[0])
                lowerStmt(sc, node->children[0]);
            /* Condition */
            IrBlock *condBlk = irFunctionAddBlock(ctx->curFn, lblCond);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblCond), irImm(0), node->line);
            ctx->curBlk = condBlk;
            if (node->childCount > 1 && node->children[1]) {
                IrOperand cond = lowerExpr(ctx, node->children[1]);
                emitInstr(ctx, IR_BRANCHZ, irImm(0), cond, irNamed(lblEnd), node->line);
            }
            /* Body */
            if (node->childCount > 3) lowerStmt(sc, node->children[3]);
            /* Increment */
            IrBlock *incrBlk = irFunctionAddBlock(ctx->curFn, lblIncr);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblIncr), irImm(0), node->line);
            ctx->curBlk = incrBlk;
            if (node->childCount > 2 && node->children[2])
                lowerExpr(ctx, node->children[2]);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lblCond), irImm(0), node->line);
            IrBlock *endBlk = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = endBlk;
            sc->loopDepth--;
            break;
        }

        case AST_SWITCH_STMT: {
            /* Simplified: lower condition, then fall through body */
            IrOperand val = lowerExpr(ctx, node->childCount > 0 ? node->children[0] : NULL);
            char *lblEnd = newLabel(ctx, "sw_e");
            int ld = sc->loopDepth++;
            if (ld < MAX_LOOP_DEPTH) {
                strncpy(sc->breakLabels[ld], lblEnd, 63);
                strncpy(sc->contLabels[ld], lblEnd, 63);
            }
            (void)val;
            if (node->childCount > 1) lowerStmt(sc, node->children[1]);
            IrBlock *endBlk = irFunctionAddBlock(ctx->curFn, lblEnd);
            ctx->curBlk = endBlk;
            sc->loopDepth--;
            break;
        }

        case AST_CASE_STMT:
        case AST_DEFAULT_STMT: {
            /* Simplified: just emit label and continue */
            char *lbl = newLabel(ctx, node->type == AST_CASE_STMT ? "case" : "default");
            IrBlock *blk = irFunctionAddBlock(ctx->curFn, lbl);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(lbl), irImm(0), node->line);
            ctx->curBlk = blk;
            /* body */
            int bodyIdx = (node->type == AST_CASE_STMT) ? 1 : 0;
            if (node->childCount > bodyIdx)
                lowerStmt(sc, node->children[bodyIdx]);
            break;
        }

        case AST_BREAK_STMT: {
            if (sc->loopDepth > 0) {
                emitInstr(ctx, IR_JUMP, irImm(0), irNamed(sc->breakLabels[sc->loopDepth - 1]), irImm(0), node->line);
            }
            break;
        }

        case AST_CONTINUE_STMT: {
            if (sc->loopDepth > 0) {
                emitInstr(ctx, IR_JUMP, irImm(0), irNamed(sc->contLabels[sc->loopDepth - 1]), irImm(0), node->line);
            }
            break;
        }

        case AST_LABEL_STMT: {
            IrBlock *blk = irFunctionAddBlock(ctx->curFn, node->name);
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(node->name), irImm(0), node->line);
            ctx->curBlk = blk;
            if (node->childCount > 0)
                lowerStmt(sc, node->children[0]);
            break;
        }

        case AST_GOTO_STMT:
            emitInstr(ctx, IR_JUMP, irImm(0), irNamed(node->name), irImm(0), node->line);
            break;

        default:
            break;
    }
}

static void lowerFunction(IrCtx *ctx, AstNode *func) {
    if (!func || func->type != AST_FUNCTION_DEF) return;

    /* Find body (last child should be compound stmt) */
    AstNode *body = NULL;
    for (int i = func->childCount - 1; i >= 0; i--) {
        if (func->children[i] && func->children[i]->type == AST_COMPOUND_STMT) {
            body = func->children[i];
            break;
        }
    }
    if (!body) return; /* prototype only */

    /* Determine return type */
    CcType *retType = NULL;
    if (func->childCount > 0 && func->children[0]) {
        if (func->children[0]->resolvedType)
            retType = func->children[0]->resolvedType;
        else if (strcmp(func->children[0]->name, "void") == 0)
            retType = ccTypeVoid();
        else
            retType = ccTypeInt();
    } else {
        retType = ccTypeInt();
    }

    IrFunction *irFn = irModuleAddFunction(ctx->module, func->name, retType);
    ctx->curFn = irFn;
    ctx->localCount = 0;
    ctx->stackOffset = 0;

    IrBlock *entry = irFunctionAddBlock(irFn, "entry");
    ctx->curBlk = entry;

    /* Register parameters as locals */
    int paramOff = 8; /* past saved FP and return address */
    for (int i = 1; i < func->childCount; i++) {
        AstNode *child = func->children[i];
        if (!child || child->type != AST_PARAM) continue;
        if (child->name[0]) {
            if (ctx->localCount < 256) {
                strncpy(ctx->locals[ctx->localCount].name, child->name, 127);
                ctx->locals[ctx->localCount].offset = paramOff;
                ctx->localCount++;
            }
            int sz = 2; /* default parameter size */
            if (child->resolvedType)
                sz = (int)ccTypeGetSize(child->resolvedType);
            if (sz < 2) sz = 2; /* MC68000 pushes at least words */
            paramOff += sz < 4 ? 4 : sz; /* parameters always pushed as longwords on stack */
        }
    }

    /* Lower body */
    StmtCtx sc = { .ir = ctx, .loopDepth = 0 };
    lowerStmt(&sc, body);

    /* Ensure function ends with a return */
    if (ctx->curBlk->instrCount == 0 ||
        ctx->curBlk->instrs[ctx->curBlk->instrCount - 1].op != IR_RETURN) {
        emitInstr(ctx, IR_RETURN, irImm(0), irImm(0), irImm(0), 0);
    }
}

static IrModule *lowerAstToIr(AstNode *ast) {
    IrModule *module = irModuleCreate();
    IrCtx ctx = { .module = module, .labelCount = 0 };

    for (int i = 0; i < ast->childCount; i++) {
        AstNode *child = ast->children[i];
        if (!child) continue;

        if (child->type == AST_FUNCTION_DEF) {
            lowerFunction(&ctx, child);
        }
        /* Global variables would be handled here too */
    }

    return module;
}

/* ── Compile entry points ────────────────────────────── */

bool compilerCompileString(Compiler *cc, const char *source, const char *filename,
                           char **outAsm, u32 *outLen) {
    cc->errorCount = 0;

    /* 1. Lex + Parse → AST */
    ccLexerSetInput(cc->lexer, source, filename);
    AstNode *ast = ccParserParse(cc->parser, cc->lexer);
    if (!ast) { ccError(cc, "parse failed"); return false; }

    /* Collect parser errors */
    for (int i = 0; i < ccParserGetErrorCount(cc->parser); i++)
        ccError(cc, "%s", ccParserGetError(cc->parser, i));
    if (cc->errorCount > 0) { astDestroyNode(ast); return false; }

    /* 2. Semantic analysis */
    if (!ccSemaAnalyze(cc->sema, ast)) {
        for (int i = 0; i < ccSemaGetErrorCount(cc->sema); i++)
            ccError(cc, "%s", ccSemaGetError(cc->sema, i));
        astDestroyNode(ast); return false;
    }

    /* 3. AST → IR */
    IrModule *ir = lowerAstToIr(ast);

    /* 4. Optimise */
    optimizerRun(ir, cc->optLevel);

    /* 5. Code generation */
    char *asm_text = codeGenGetAsm(cc->codegen, ir);
    if (outAsm) *outAsm = asm_text; else free(asm_text);
    if (outLen) *outLen = asm_text ? (u32)strlen(asm_text) : 0;

    irModuleDestroy(ir);
    astDestroyNode(ast);
    return cc->errorCount == 0;
}

bool compilerCompileFile(Compiler *cc, const char *inputPath, const char *outputPath) {
    FILE *f = fopen(inputPath, "rb");
    if (!f) { ccError(cc, "cannot open %s", inputPath); return false; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = malloc((size_t)sz + 1);
    fread(src, 1, (size_t)sz, f); src[sz] = '\0'; fclose(f);

    char *asmText = NULL;
    u32 asmLen = 0;
    bool ok = compilerCompileString(cc, src, inputPath, &asmText, &asmLen);
    free(src);

    if (ok && outputPath && asmText) {
        FILE *out = fopen(outputPath, "w");
        if (out) { fwrite(asmText, 1, asmLen, out); fclose(out); }
        else { ccError(cc, "cannot write %s", outputPath); ok = false; }
    }
    free(asmText);
    return ok;
}
