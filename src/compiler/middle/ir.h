/* ir.h — Intermediate Representation (three-address code) */
#ifndef M68K_CC_IR_H
#define M68K_CC_IR_H

#include "common/types.h"
#include "compiler/frontend/cc_types.h"

/*
 * TAC (Three-Address Code) IR.
 *
 * Each instruction: result = arg1 OP arg2
 * Temporaries are numbered: t0, t1, t2, ...
 */

typedef enum {
    IR_NOP,
    /* Arithmetic */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD, IR_NEG,
    /* Bitwise */
    IR_AND, IR_OR, IR_XOR, IR_NOT, IR_SHL, IR_SHR,
    /* Comparison (result = 0 or 1) */
    IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,
    /* Data movement */
    IR_LOAD,     /* result = MEM[arg1]           */
    IR_STORE,    /* MEM[arg1] = arg2             */
    IR_MOVE,     /* result = arg1                */
    IR_CONST,    /* result = immediate           */
    /* Control flow */
    IR_LABEL,
    IR_JUMP,     /* unconditional jump           */
    IR_BRANCH,   /* if (arg1) goto label         */
    IR_BRANCHZ,  /* if (!arg1) goto label        */
    IR_CALL,     /* result = call arg1(args...)   */
    IR_RETURN,   /* return arg1                  */
    IR_PARAM,    /* push parameter               */
    /* Address / pointer */
    IR_ADDR,     /* result = &arg1               */
    IR_DEREF,    /* result = *arg1               */
    /* Type */
    IR_CAST,     /* result = (type)arg1          */
} IrOpcode;

typedef struct {
    int   id;       /* temporary number (t0, t1, ...) or -1 for constants */
    s64   imm;      /* immediate value (when id == -1) */
    CcType *type;
    char  name[64]; /* for named variables / labels */
} IrOperand;

typedef struct {
    IrOpcode  op;
    IrOperand result;
    IrOperand arg1;
    IrOperand arg2;
    int       line;  /* source line for debug */
} IrInstr;

/* Basic block */
typedef struct {
    char     label[64];
    IrInstr *instrs;
    int      instrCount;
    int      instrCapacity;
} IrBlock;

/* Function */
typedef struct {
    char     name[128];
    IrBlock *blocks;
    int      blockCount;
    int      blockCapacity;
    int      tempCount;  /* next available temporary */
    CcType  *returnType;
} IrFunction;

/* Module (translation unit) */
typedef struct {
    IrFunction *functions;
    int         functionCount;
    int         functionCapacity;
} IrModule;

/* Construction */
IrModule   *irModuleCreate(void);
void        irModuleDestroy(IrModule *m);
IrFunction *irModuleAddFunction(IrModule *m, const char *name, CcType *retType);
IrBlock    *irFunctionAddBlock(IrFunction *fn, const char *label);
void        irBlockAddInstr(IrBlock *blk, IrInstr instr);

/* Operand helpers */
IrOperand irTemp(int id);
IrOperand irImm(s64 value);
IrOperand irNamed(const char *name);

/* Debug printing */
void irModulePrint(IrModule *m);

#endif /* M68K_CC_IR_H */
