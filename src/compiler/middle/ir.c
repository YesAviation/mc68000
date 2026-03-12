/* ir.c — Intermediate Representation */
#include "compiler/middle/ir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Module ──────────────────────────────────────────── */

IrModule *irModuleCreate(void) {
    IrModule *m = calloc(1, sizeof(IrModule));
    m->functionCapacity = 16;
    m->functions = calloc((size_t)m->functionCapacity, sizeof(IrFunction));
    return m;
}

void irModuleDestroy(IrModule *m) {
    if (!m) return;
    for (int f = 0; f < m->functionCount; f++) {
        IrFunction *fn = &m->functions[f];
        for (int b = 0; b < fn->blockCount; b++)
            free(fn->blocks[b].instrs);
        free(fn->blocks);
    }
    free(m->functions);
    free(m);
}

IrFunction *irModuleAddFunction(IrModule *m, const char *name, CcType *retType) {
    if (m->functionCount >= m->functionCapacity) {
        m->functionCapacity *= 2;
        m->functions = realloc(m->functions, (size_t)m->functionCapacity * sizeof(IrFunction));
    }
    IrFunction *fn = &m->functions[m->functionCount++];
    memset(fn, 0, sizeof(*fn));
    strncpy(fn->name, name, sizeof(fn->name) - 1);
    fn->returnType = retType;
    fn->blockCapacity = 8;
    fn->blocks = calloc((size_t)fn->blockCapacity, sizeof(IrBlock));
    return fn;
}

/* ── Block ───────────────────────────────────────────── */

IrBlock *irFunctionAddBlock(IrFunction *fn, const char *label) {
    if (fn->blockCount >= fn->blockCapacity) {
        fn->blockCapacity *= 2;
        fn->blocks = realloc(fn->blocks, (size_t)fn->blockCapacity * sizeof(IrBlock));
    }
    IrBlock *blk = &fn->blocks[fn->blockCount++];
    memset(blk, 0, sizeof(*blk));
    strncpy(blk->label, label, sizeof(blk->label) - 1);
    blk->instrCapacity = 32;
    blk->instrs = calloc((size_t)blk->instrCapacity, sizeof(IrInstr));
    return blk;
}

void irBlockAddInstr(IrBlock *blk, IrInstr instr) {
    if (blk->instrCount >= blk->instrCapacity) {
        blk->instrCapacity *= 2;
        blk->instrs = realloc(blk->instrs, (size_t)blk->instrCapacity * sizeof(IrInstr));
    }
    blk->instrs[blk->instrCount++] = instr;
}

/* ── Operand helpers ─────────────────────────────────── */

IrOperand irTemp(int id)          { IrOperand o = {0}; o.id = id; return o; }
IrOperand irImm(s64 value)        { IrOperand o = {0}; o.id = -1; o.imm = value; return o; }
IrOperand irNamed(const char *name) {
    IrOperand o = {0}; o.id = -2;
    strncpy(o.name, name, sizeof(o.name) - 1);
    return o;
}

/* ── Debug printing ──────────────────────────────────── */

static const char *irOpName(IrOpcode op) {
    switch (op) {
        case IR_NOP:     return "nop";
        case IR_ADD:     return "add";   case IR_SUB: return "sub";
        case IR_MUL:     return "mul";   case IR_DIV: return "div";
        case IR_MOD:     return "mod";   case IR_NEG: return "neg";
        case IR_AND:     return "and";   case IR_OR:  return "or";
        case IR_XOR:     return "xor";   case IR_NOT: return "not";
        case IR_SHL:     return "shl";   case IR_SHR: return "shr";
        case IR_EQ:      return "eq";    case IR_NE:  return "ne";
        case IR_LT:      return "lt";    case IR_LE:  return "le";
        case IR_GT:      return "gt";    case IR_GE:  return "ge";
        case IR_LOAD:    return "load";  case IR_STORE: return "store";
        case IR_MOVE:    return "move";  case IR_CONST: return "const";
        case IR_LABEL:   return "label"; case IR_JUMP:  return "jump";
        case IR_BRANCH:  return "br";    case IR_BRANCHZ: return "brz";
        case IR_CALL:    return "call";  case IR_RETURN:  return "ret";
        case IR_PARAM:   return "param"; case IR_ADDR: return "addr";
        case IR_DEREF:   return "deref"; case IR_CAST: return "cast";
    }
    return "???";
}

static void printOperand(IrOperand *o) {
    if (o->id == -1)      printf("#%lld", (long long)o->imm);
    else if (o->id == -2) printf("%s", o->name);
    else if (o->id >= 0)  printf("t%d", o->id);
}

void irModulePrint(IrModule *m) {
    for (int f = 0; f < m->functionCount; f++) {
        IrFunction *fn = &m->functions[f];
        printf("function %s:\n", fn->name);
        for (int b = 0; b < fn->blockCount; b++) {
            IrBlock *blk = &fn->blocks[b];
            printf("  %s:\n", blk->label);
            for (int i = 0; i < blk->instrCount; i++) {
                IrInstr *ins = &blk->instrs[i];
                printf("    ");
                if (ins->result.id >= 0) { printOperand(&ins->result); printf(" = "); }
                printf("%s ", irOpName(ins->op));
                printOperand(&ins->arg1);
                if (ins->arg2.id >= -1) { printf(", "); printOperand(&ins->arg2); }
                printf("\n");
            }
        }
    }
}
