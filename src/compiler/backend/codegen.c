/* codegen.c — MC68000 code generator
 *
 * Translates TAC IR to MC68000 assembly text (Motorola syntax).
 *
 * ABI:
 *   - Stack grows downward, A7 = SP
 *   - Parameters pushed right-to-left
 *   - Return value in D0 (word/long) or D0:D1 (8-byte)
 *   - Caller saves D0-D1, A0-A1
 *   - Callee saves D2-D7, A2-A6
 *   - Frame pointer: A6
 *
 * Register allocation strategy:
 *   - IR temporaries are mapped to data registers D0-D7
 *   - Spilled temporaries go to stack slots relative to A6
 *   - Address computations use A0-A5
 */
#include "compiler/backend/codegen.h"
#include "compiler/backend/registers.h"
#include "compiler/frontend/ast.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_TEMPS 256

struct CodeGen {
    Buffer       asmOutput;
    RegAllocator ra;
    /* Temp → register mapping */
    int          tempReg[MAX_TEMPS];   /* -1 = spilled */
    int          tempSpill[MAX_TEMPS]; /* stack offset for spills */
    int          spillOffset;          /* grows negative from FP */
    /* String literal pool */
    struct {
        char label[32];
        char value[AST_MAX_NAME];
    } strings[128];
    int stringCount;
};

/* ── Emit helpers ────────────────────────────────────── */

static void emit(Buffer *buf, const char *fmt, ...) {
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    size_t len = strlen(line);
    for (size_t i = 0; i < len; i++)
        bufferWriteU8(buf, (u8)line[i]);
}

/* Get the register name for a temporary, spilling if necessary */
static const char *tempToReg(CodeGen *cg, Buffer *out, int tempId) {
    static char spill[32];
    if (tempId < 0) return "D0"; /* fallback for immediates */
    if (tempId >= MAX_TEMPS) return "D0";

    if (cg->tempReg[tempId] >= 0)
        return regName((M68kReg)cg->tempReg[tempId]);

    /* Not allocated — try to allocate now */
    M68kReg r = regAllocData(&cg->ra);
    if (r != REG_NONE) {
        cg->tempReg[tempId] = (int)r;
        return regName(r);
    }

    /* Spilled — return stack reference */
    if (cg->tempSpill[tempId] == 0) {
        cg->spillOffset -= 4;
        cg->tempSpill[tempId] = cg->spillOffset;
    }
    snprintf(spill, sizeof(spill), "%d(A6)", cg->tempSpill[tempId]);
    return spill;
}

/* Load an operand into a register, returning register name */
static const char *loadOperand(CodeGen *cg, Buffer *out, IrOperand *op) {
    if (op->id == -1) {
        /* Immediate */
        const char *r = tempToReg(cg, out, cg->ra.spillCount + 200);
        emit(out, "    MOVE.L  #%lld,%s\n", (long long)op->imm, r);
        return r;
    }
    if (op->id == -2) {
        /* Named (variable/label) */
        const char *r = tempToReg(cg, out, cg->ra.spillCount + 201);
        emit(out, "    MOVE.L  %s,%s\n", op->name, r);
        return r;
    }
    return tempToReg(cg, out, op->id);
}

/* Free the register for a temporary */
static void freeTemp(CodeGen *cg, int tempId) {
    if (tempId < 0 || tempId >= MAX_TEMPS) return;
    if (cg->tempReg[tempId] >= 0) {
        regFree(&cg->ra, (M68kReg)cg->tempReg[tempId]);
        cg->tempReg[tempId] = -1;
    }
}

static const char *sizeStr(int bytes) {
    if (bytes <= 1) return ".B";
    if (bytes <= 2) return ".W";
    return ".L";
}

/* ── Code generation per IR instruction ──────────────── */

static void genInstr(CodeGen *cg, Buffer *out, IrInstr *ins) {
    switch (ins->op) {
        case IR_NOP:
            break;

        case IR_CONST: {
            const char *dst = tempToReg(cg, out, ins->result.id);
            emit(out, "    MOVE.L  #%lld,%s\n", (long long)ins->arg1.imm, dst);
            break;
        }

        case IR_MOVE: {
            const char *src = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(src, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", src, dst);
            break;
        }

        case IR_ADD: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                if (ins->arg2.imm >= 1 && ins->arg2.imm <= 8)
                    emit(out, "    ADDQ.L  #%lld,%s\n", (long long)ins->arg2.imm, dst);
                else
                    emit(out, "    ADD.L   #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    ADD.L   %s,%s\n", b, dst);
            }
            break;
        }

        case IR_SUB: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                if (ins->arg2.imm >= 1 && ins->arg2.imm <= 8)
                    emit(out, "    SUBQ.L  #%lld,%s\n", (long long)ins->arg2.imm, dst);
                else
                    emit(out, "    SUB.L   #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    SUB.L   %s,%s\n", b, dst);
            }
            break;
        }

        case IR_MUL: {
            /* MC68000 MULS/MULU: 16-bit × 16-bit → 32-bit result */
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                emit(out, "    MULS.W  #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    MULS.W  %s,%s\n", b, dst);
            }
            break;
        }

        case IR_DIV: {
            /* MC68000 DIVS: 32 / 16 → 16 quot : 16 rem */
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                int t = cg->ra.spillCount + 202;
                const char *tmp = tempToReg(cg, out, t);
                emit(out, "    MOVE.W  #%lld,%s\n", (long long)ins->arg2.imm, tmp);
                emit(out, "    DIVS.W  %s,%s\n", tmp, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    DIVS.W  %s,%s\n", b, dst);
            }
            /* Quotient is in low word, extend to long */
            emit(out, "    EXT.L   %s\n", dst);
            break;
        }

        case IR_MOD: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                int t = cg->ra.spillCount + 202;
                const char *tmp = tempToReg(cg, out, t);
                emit(out, "    MOVE.W  #%lld,%s\n", (long long)ins->arg2.imm, tmp);
                emit(out, "    DIVS.W  %s,%s\n", tmp, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    DIVS.W  %s,%s\n", b, dst);
            }
            /* Remainder is in high word — swap */
            emit(out, "    SWAP    %s\n", dst);
            emit(out, "    EXT.L   %s\n", dst);
            break;
        }

        case IR_NEG: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            emit(out, "    NEG.L   %s\n", dst);
            break;
        }

        case IR_AND: case IR_OR: case IR_XOR: {
            const char *opname = ins->op == IR_AND ? "AND" :
                                 ins->op == IR_OR  ? "OR"  : "EOR";
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                if (ins->op == IR_XOR) {
                    /* EORI uses different encoding */
                    emit(out, "    EORI.L  #%lld,%s\n", (long long)ins->arg2.imm, dst);
                } else {
                    emit(out, "    %sI.L  #%lld,%s\n", opname, (long long)ins->arg2.imm, dst);
                }
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                if (ins->op == IR_XOR) {
                    /* EOR Dn,<ea> requires source to be data register */
                    emit(out, "    EOR.L   %s,%s\n", b, dst);
                } else {
                    emit(out, "    %s.L   %s,%s\n", opname, b, dst);
                }
            }
            break;
        }

        case IR_NOT: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            emit(out, "    NOT.L   %s\n", dst);
            break;
        }

        case IR_SHL: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1 && ins->arg2.imm >= 1 && ins->arg2.imm <= 8) {
                emit(out, "    LSL.L   #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *cnt = loadOperand(cg, out, &ins->arg2);
                emit(out, "    LSL.L   %s,%s\n", cnt, dst);
            }
            break;
        }

        case IR_SHR: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(a, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1 && ins->arg2.imm >= 1 && ins->arg2.imm <= 8) {
                emit(out, "    LSR.L   #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *cnt = loadOperand(cg, out, &ins->arg2);
                emit(out, "    LSR.L   %s,%s\n", cnt, dst);
            }
            break;
        }

        /* Comparisons: set result to 0 or 1 */
        case IR_EQ: case IR_NE: case IR_LT: case IR_LE:
        case IR_GT: case IR_GE: {
            const char *a = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            emit(out, "    MOVE.L  %s,%s\n", a, dst);
            if (ins->arg2.id == -1) {
                emit(out, "    CMP.L   #%lld,%s\n", (long long)ins->arg2.imm, dst);
            } else {
                const char *b = loadOperand(cg, out, &ins->arg2);
                emit(out, "    CMP.L   %s,%s\n", b, dst);
            }
            /* Use Scc to set byte, then extend */
            const char *cc_name = "EQ";
            switch (ins->op) {
                case IR_EQ: cc_name = "EQ"; break;
                case IR_NE: cc_name = "NE"; break;
                case IR_LT: cc_name = "LT"; break;
                case IR_LE: cc_name = "LE"; break;
                case IR_GT: cc_name = "GT"; break;
                case IR_GE: cc_name = "GE"; break;
                default: break;
            }
            emit(out, "    S%s     %s\n", cc_name, dst);
            emit(out, "    AND.L   #1,%s\n", dst);
            break;
        }

        case IR_LOAD: {
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (ins->arg1.id == -2) {
                /* Named variable */
                emit(out, "    MOVE.L  %s,%s\n", ins->arg1.name, dst);
            } else {
                const char *addr = loadOperand(cg, out, &ins->arg1);
                emit(out, "    MOVE.L  (%s),%s\n", addr, dst);
            }
            break;
        }

        case IR_STORE: {
            const char *val = loadOperand(cg, out, &ins->arg2);
            if (ins->arg1.id == -2) {
                /* Named variable */
                emit(out, "    MOVE.L  %s,%s\n", val, ins->arg1.name);
            } else {
                const char *addr = loadOperand(cg, out, &ins->arg1);
                emit(out, "    MOVE.L  %s,(%s)\n", val, addr);
            }
            break;
        }

        case IR_ADDR: {
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (ins->arg1.id == -2) {
                emit(out, "    LEA     %s,A0\n", ins->arg1.name);
                emit(out, "    MOVE.L  A0,%s\n", dst);
            } else {
                const char *src = loadOperand(cg, out, &ins->arg1);
                emit(out, "    MOVE.L  %s,%s\n", src, dst);
            }
            break;
        }

        case IR_DEREF: {
            const char *src = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            emit(out, "    MOVEA.L %s,A0\n", src);
            emit(out, "    MOVE.L  (A0),%s\n", dst);
            break;
        }

        case IR_CAST: {
            const char *src = loadOperand(cg, out, &ins->arg1);
            const char *dst = tempToReg(cg, out, ins->result.id);
            if (strcmp(src, dst) != 0)
                emit(out, "    MOVE.L  %s,%s\n", src, dst);
            /* Type narrowing/widening handled by MOVE size */
            break;
        }

        case IR_JUMP: {
            if (ins->arg1.id == -2)
                emit(out, "    BRA     .%s\n", ins->arg1.name);
            else
                emit(out, "    BRA     .L%lld\n", (long long)ins->arg1.imm);
            break;
        }

        case IR_BRANCH: {
            const char *cond = loadOperand(cg, out, &ins->arg1);
            emit(out, "    TST.L   %s\n", cond);
            if (ins->arg2.id == -2)
                emit(out, "    BNE     .%s\n", ins->arg2.name);
            else
                emit(out, "    BNE     .L%lld\n", (long long)ins->arg2.imm);
            break;
        }

        case IR_BRANCHZ: {
            const char *cond = loadOperand(cg, out, &ins->arg1);
            emit(out, "    TST.L   %s\n", cond);
            if (ins->arg2.id == -2)
                emit(out, "    BEQ     .%s\n", ins->arg2.name);
            else
                emit(out, "    BEQ     .L%lld\n", (long long)ins->arg2.imm);
            break;
        }

        case IR_PARAM: {
            /* Push parameter onto stack */
            if (ins->arg1.id == -1) {
                emit(out, "    MOVE.L  #%lld,-(SP)\n", (long long)ins->arg1.imm);
            } else {
                const char *val = loadOperand(cg, out, &ins->arg1);
                emit(out, "    MOVE.L  %s,-(SP)\n", val);
            }
            break;
        }

        case IR_CALL: {
            /* Call function */
            if (ins->arg1.id == -2) {
                emit(out, "    JSR     %s\n", ins->arg1.name);
            } else {
                const char *fn = loadOperand(cg, out, &ins->arg1);
                emit(out, "    JSR     (%s)\n", fn);
            }
            /* Clean up parameters */
            int nparams = (int)ins->arg2.imm;
            if (nparams > 0) {
                int bytes = nparams * 4;
                if (bytes <= 8)
                    emit(out, "    ADDQ.L  #%d,SP\n", bytes);
                else
                    emit(out, "    ADD.L   #%d,SP\n", bytes);
            }
            /* Result in D0 */
            if (ins->result.id >= 0) {
                const char *dst = tempToReg(cg, out, ins->result.id);
                if (strcmp(dst, "D0") != 0)
                    emit(out, "    MOVE.L  D0,%s\n", dst);
            }
            break;
        }

        case IR_RETURN: {
            /* Move return value to D0 */
            if (ins->arg1.id == -1) {
                if (ins->arg1.imm != 0)
                    emit(out, "    MOVE.L  #%lld,D0\n", (long long)ins->arg1.imm);
                else
                    emit(out, "    CLR.L   D0\n");
            } else if (ins->arg1.id >= 0) {
                const char *src = loadOperand(cg, out, &ins->arg1);
                if (strcmp(src, "D0") != 0)
                    emit(out, "    MOVE.L  %s,D0\n", src);
            }
            emit(out, "    BRA     .exit\n");
            break;
        }

        case IR_LABEL:
            /* Labels are handled at block level */
            break;

        default:
            emit(out, "    ; unhandled IR op %d\n", ins->op);
            break;
    }
}

/* ── Function code generation ────────────────────────── */

static void genFunction(CodeGen *cg, Buffer *out, IrFunction *fn) {
    /* Reset register allocator */
    regAllocInit(&cg->ra);
    memset(cg->tempReg, -1, sizeof(cg->tempReg));
    memset(cg->tempSpill, 0, sizeof(cg->tempSpill));
    cg->spillOffset = 0;

    /* Section header */
    emit(out, "\n    SECTION .text\n");
    emit(out, "    EVEN\n");

    /* Global label */
    emit(out, "%s:\n", fn->name);

    /* Prologue */
    emit(out, "    LINK    A6,#0\n");
    emit(out, "    MOVEM.L D2-D7/A2-A5,-(SP)\n");
    emit(out, "\n");

    /* Emit code for each basic block */
    for (int b = 0; b < fn->blockCount; b++) {
        IrBlock *blk = &fn->blocks[b];
        emit(out, ".%s:\n", blk->label);

        for (int i = 0; i < blk->instrCount; i++) {
            genInstr(cg, out, &blk->instrs[i]);
        }
    }

    /* Epilogue */
    emit(out, "\n.exit:\n");
    emit(out, "    MOVEM.L (SP)+,D2-D7/A2-A5\n");
    emit(out, "    UNLK    A6\n");
    emit(out, "    RTS\n");
}

/* ── Public API ──────────────────────────────────────── */

CodeGen *codeGenCreate(void) {
    CodeGen *cg = calloc(1, sizeof(CodeGen));
    bufferInit(&cg->asmOutput);
    regAllocInit(&cg->ra);
    memset(cg->tempReg, -1, sizeof(cg->tempReg));
    return cg;
}

void codeGenDestroy(CodeGen *cg) {
    if (!cg) return;
    bufferFree(&cg->asmOutput);
    free(cg);
}

bool codeGenEmit(CodeGen *cg, IrModule *module, Buffer *output) {
    Buffer *out = output ? output : &cg->asmOutput;
    bufferClear(out);

    emit(out, "; Generated by m68k_cc\n");
    emit(out, "; MC68000 assembly (Motorola syntax)\n");

    for (int f = 0; f < module->functionCount; f++) {
        genFunction(cg, out, &module->functions[f]);
    }

    /* Emit string literals in data section */
    if (cg->stringCount > 0) {
        emit(out, "\n    SECTION .data\n");
        for (int i = 0; i < cg->stringCount; i++) {
            emit(out, "%s:\n", cg->strings[i].label);
            emit(out, "    DC.B    '%s',0\n", cg->strings[i].value);
            emit(out, "    EVEN\n");
        }
    }

    emit(out, "\n    END\n");
    return true;
}

char *codeGenGetAsm(CodeGen *cg, IrModule *module) {
    bufferClear(&cg->asmOutput);
    codeGenEmit(cg, module, &cg->asmOutput);
    /* Null-terminate */
    bufferWriteU8(&cg->asmOutput, 0);
    char *result = malloc(cg->asmOutput.size);
    if (result)
        memcpy(result, cg->asmOutput.data, cg->asmOutput.size);
    return result;
}
