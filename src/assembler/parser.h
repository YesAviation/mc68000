/* parser.h — Assembly language parser */
#ifndef M68K_ASM_PARSER_H
#define M68K_ASM_PARSER_H

#include "common/types.h"
#include "assembler/lexer.h"

/* Maximum operands per instruction */
#define ASM_MAX_OPERANDS  2
#define ASM_MAX_LABEL_LEN 64
#define ASM_MAX_MNEMONIC  16
#define ASM_MAX_OPERAND_TOKENS 16

/* Operand types (68000 addressing modes + assembler extras) */
typedef enum {
    OPER_NONE = 0,
    OPER_DATA_REG,          /* Dn                    */
    OPER_ADDR_REG,          /* An                    */
    OPER_ADDR_IND,          /* (An)                  */
    OPER_ADDR_IND_POST,     /* (An)+                 */
    OPER_ADDR_IND_PRE,      /* -(An)                 */
    OPER_ADDR_DISP,         /* d16(An)               */
    OPER_ADDR_INDEX,        /* d8(An,Xn.S)           */
    OPER_ABS_SHORT,         /* xxx.W                 */
    OPER_ABS_LONG,          /* xxx.L                 */
    OPER_PC_DISP,           /* d16(PC)               */
    OPER_PC_INDEX,          /* d8(PC,Xn.S)           */
    OPER_IMMEDIATE,         /* #<data>               */
    OPER_REG_LIST,          /* register list for MOVEM */
    OPER_SR,                /* SR                    */
    OPER_CCR,               /* CCR                   */
    OPER_USP,               /* USP                   */
    OPER_EXPRESSION         /* numeric expression / label */
} AsmOperandType;

typedef struct {
    AsmOperandType type;
    int            reg;        /* register number (0-7)        */
    int            indexReg;   /* index register number        */
    bool           indexIsAddr;/* true = An, false = Dn        */
    bool           indexIsLong;/* .L vs .W index size          */
    s32            displacement;
    s64            immediate;
    u16            regListMask;
    char           symbol[ASM_MAX_LABEL_LEN]; /* label reference */
} AsmOperand;

typedef struct {
    char         label[ASM_MAX_LABEL_LEN];
    char         mnemonic[ASM_MAX_MNEMONIC];
    u8           size;         /* 0=default, 1=B, 2=W, 4=L  */
    AsmOperand   operands[ASM_MAX_OPERANDS];
    int          operandCount;
    bool         isDirective;
    int          line;
    /* For DC.B/W/L with multiple values */
    s64          dataValues[256];
    int          dataCount;
} AsmLine;

typedef struct AsmParser AsmParser;

AsmParser *asmParserCreate(void);
void       asmParserDestroy(AsmParser *p);
bool       asmParserParseLine(AsmParser *p, AsmLexer *lex, AsmLine *out);

#endif /* M68K_ASM_PARSER_H */
