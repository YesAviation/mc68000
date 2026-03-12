/* optimizer.c — IR optimisation passes
 *
 * Implements three classic optimisation passes on the TAC IR:
 *   1. Constant folding: evaluate compile-time constant expressions
 *   2. Copy propagation: replace uses of copies with their sources
 *   3. Dead code elimination: remove instructions whose results are unused
 */
#include "compiler/middle/optimizer.h"
#include <string.h>
#include <stdlib.h>

/* ── Helpers ─────────────────────────────────────────── */

static bool isConst(IrOperand *o) { return o->id == -1; }

static bool hasSideEffect(IrOpcode op) {
    switch (op) {
        case IR_STORE: case IR_CALL: case IR_RETURN:
        case IR_JUMP: case IR_BRANCH: case IR_BRANCHZ:
        case IR_PARAM: case IR_LABEL:
            return true;
        default:
            return false;
    }
}

/* ── Constant Folding ────────────────────────────────── */

void optimizerConstantFold(IrModule *m) {
    if (!m) return;

    for (int f = 0; f < m->functionCount; f++) {
        IrFunction *fn = &m->functions[f];
        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            for (int i = 0; i < blk->instrCount; i++) {
                IrInstr *ins = &blk->instrs[i];

                /* Fold binary ops with two constant operands */
                if (isConst(&ins->arg1) && isConst(&ins->arg2)) {
                    s64 a = ins->arg1.imm;
                    s64 b2 = ins->arg2.imm;
                    s64 result = 0;
                    bool folded = true;

                    switch (ins->op) {
                        case IR_ADD: result = a + b2; break;
                        case IR_SUB: result = a - b2; break;
                        case IR_MUL: result = a * b2; break;
                        case IR_DIV: if (b2 != 0) result = a / b2; else folded = false; break;
                        case IR_MOD: if (b2 != 0) result = a % b2; else folded = false; break;
                        case IR_AND: result = a & b2; break;
                        case IR_OR:  result = a | b2; break;
                        case IR_XOR: result = a ^ b2; break;
                        case IR_SHL: result = a << b2; break;
                        case IR_SHR: result = (s64)((u64)a >> b2); break;
                        case IR_EQ:  result = (a == b2) ? 1 : 0; break;
                        case IR_NE:  result = (a != b2) ? 1 : 0; break;
                        case IR_LT:  result = (a < b2)  ? 1 : 0; break;
                        case IR_LE:  result = (a <= b2) ? 1 : 0; break;
                        case IR_GT:  result = (a > b2)  ? 1 : 0; break;
                        case IR_GE:  result = (a >= b2) ? 1 : 0; break;
                        default: folded = false; break;
                    }

                    if (folded) {
                        ins->op = IR_CONST;
                        ins->arg1 = irImm(result);
                        ins->arg2 = irImm(0);
                    }
                }

                /* Fold unary ops with constant operand */
                if (isConst(&ins->arg1) && ins->arg2.id == -1 && ins->arg2.imm == 0) {
                    s64 a = ins->arg1.imm;
                    bool folded = true;
                    s64 result = 0;

                    switch (ins->op) {
                        case IR_NEG: result = -a; break;
                        case IR_NOT: result = ~a; break;
                        default: folded = false; break;
                    }

                    if (folded) {
                        ins->op = IR_CONST;
                        ins->arg1 = irImm(result);
                    }
                }

                /* Algebraic simplifications */
                switch (ins->op) {
                    case IR_ADD:
                        /* x + 0 = x */
                        if (isConst(&ins->arg2) && ins->arg2.imm == 0) {
                            ins->op = IR_MOVE;
                            ins->arg2 = irImm(0);
                        }
                        /* 0 + x = x */
                        else if (isConst(&ins->arg1) && ins->arg1.imm == 0) {
                            ins->op = IR_MOVE;
                            ins->arg1 = ins->arg2;
                            ins->arg2 = irImm(0);
                        }
                        break;
                    case IR_SUB:
                        /* x - 0 = x */
                        if (isConst(&ins->arg2) && ins->arg2.imm == 0) {
                            ins->op = IR_MOVE;
                            ins->arg2 = irImm(0);
                        }
                        break;
                    case IR_MUL:
                        /* x * 1 = x */
                        if (isConst(&ins->arg2) && ins->arg2.imm == 1) {
                            ins->op = IR_MOVE;
                            ins->arg2 = irImm(0);
                        }
                        /* x * 0 = 0 */
                        else if (isConst(&ins->arg2) && ins->arg2.imm == 0) {
                            ins->op = IR_CONST;
                            ins->arg1 = irImm(0);
                            ins->arg2 = irImm(0);
                        }
                        /* x * 2 = x << 1 (strength reduction) */
                        else if (isConst(&ins->arg2) && ins->arg2.imm == 2) {
                            ins->op = IR_SHL;
                            ins->arg2 = irImm(1);
                        }
                        else if (isConst(&ins->arg2) && ins->arg2.imm == 4) {
                            ins->op = IR_SHL;
                            ins->arg2 = irImm(2);
                        }
                        break;
                    case IR_DIV:
                        /* x / 1 = x */
                        if (isConst(&ins->arg2) && ins->arg2.imm == 1) {
                            ins->op = IR_MOVE;
                            ins->arg2 = irImm(0);
                        }
                        break;
                    default:
                        break;
                }

                /* Constant branch folding */
                if (ins->op == IR_BRANCHZ && isConst(&ins->arg1)) {
                    if (ins->arg1.imm == 0) {
                        /* Condition is false → always branch */
                        ins->op = IR_JUMP;
                        ins->arg1 = ins->arg2; /* label */
                        ins->arg2 = irImm(0);
                    } else {
                        /* Condition is true → never branch, make NOP */
                        ins->op = IR_NOP;
                    }
                }
                if (ins->op == IR_BRANCH && isConst(&ins->arg1)) {
                    if (ins->arg1.imm != 0) {
                        ins->op = IR_JUMP;
                        ins->arg1 = ins->arg2;
                        ins->arg2 = irImm(0);
                    } else {
                        ins->op = IR_NOP;
                    }
                }
            }
        }
    }
}

/* ── Copy Propagation ────────────────────────────────── */

void optimizerCopyPropagation(IrModule *m) {
    if (!m) return;

    /* For each MOVE t_dest = t_src, replace subsequent uses of t_dest with t_src. */
    for (int f = 0; f < m->functionCount; f++) {
        IrFunction *fn = &m->functions[f];

        /* Build copy map: copyOf[temp] = source temp/imm */
        IrOperand copyOf[256];
        memset(copyOf, 0, sizeof(copyOf));
        for (int i = 0; i < 256; i++) copyOf[i].id = -99; /* sentinel: no copy */

        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            for (int i = 0; i < blk->instrCount; i++) {
                IrInstr *ins = &blk->instrs[i];

                /* Record copies */
                if (ins->op == IR_MOVE && ins->result.id >= 0 && ins->result.id < 256) {
                    copyOf[ins->result.id] = ins->arg1;
                }
                if (ins->op == IR_CONST && ins->result.id >= 0 && ins->result.id < 256) {
                    copyOf[ins->result.id] = ins->arg1;
                }

                /* Replace operands with their copy sources */
                if (ins->arg1.id >= 0 && ins->arg1.id < 256 && copyOf[ins->arg1.id].id != -99) {
                    ins->arg1 = copyOf[ins->arg1.id];
                }
                if (ins->arg2.id >= 0 && ins->arg2.id < 256 && copyOf[ins->arg2.id].id != -99) {
                    ins->arg2 = copyOf[ins->arg2.id];
                }

                /* Invalidate if the temp is redefined by a non-copy instruction */
                if (ins->op != IR_MOVE && ins->op != IR_CONST &&
                    ins->result.id >= 0 && ins->result.id < 256) {
                    copyOf[ins->result.id].id = -99;
                }
            }
        }
    }
}

/* ── Dead Code Elimination ───────────────────────────── */

void optimizerDeadCodeElim(IrModule *m) {
    if (!m) return;

    for (int f = 0; f < m->functionCount; f++) {
        IrFunction *fn = &m->functions[f];

        /* Mark which temporaries are used */
        bool used[256];
        memset(used, 0, sizeof(used));

        /* Pass 1: Mark all used temporaries */
        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            for (int i = 0; i < blk->instrCount; i++) {
                IrInstr *ins = &blk->instrs[i];
                if (ins->arg1.id >= 0 && ins->arg1.id < 256) used[ins->arg1.id] = true;
                if (ins->arg2.id >= 0 && ins->arg2.id < 256) used[ins->arg2.id] = true;
            }
        }

        /* Pass 2: Remove instructions whose result temps are never used */
        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            int write = 0;
            for (int i = 0; i < blk->instrCount; i++) {
                IrInstr *ins = &blk->instrs[i];

                /* Keep instructions with side effects */
                if (hasSideEffect(ins->op)) {
                    blk->instrs[write++] = *ins;
                    continue;
                }

                /* Keep if result is used, or no result temp */
                if (ins->result.id < 0 || ins->result.id >= 256 ||
                    used[ins->result.id]) {
                    blk->instrs[write++] = *ins;
                    continue;
                }

                /* Dead instruction — skip it */
            }
            blk->instrCount = write;
        }

        /* Remove NOP instructions */
        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            int write = 0;
            for (int i = 0; i < blk->instrCount; i++) {
                if (blk->instrs[i].op != IR_NOP)
                    blk->instrs[write++] = blk->instrs[i];
            }
            blk->instrCount = write;
        }
    }
}

/* ── Top-level ───────────────────────────────────────── */

void optimizerRun(IrModule *m, int optLevel) {
    if (optLevel >= 1) {
        optimizerConstantFold(m);
        optimizerCopyPropagation(m);
    }
    if (optLevel >= 2) {
        optimizerDeadCodeElim(m);
        /* Second pass of constant folding after propagation */
        optimizerConstantFold(m);
    }
}
