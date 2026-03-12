/* parser.c — Assembly language parser */
#include "assembler/parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct AsmParser {
    int dummy; /* reserved for future state */
};

AsmParser *asmParserCreate(void) { return calloc(1, sizeof(AsmParser)); }
void asmParserDestroy(AsmParser *p) { free(p); }

/* ── helpers ─────────────────────────────────────────── */

static bool tokMatch(AsmToken *t, AsmTokenType type) { return t->type == type; }

static void tokToStr(AsmToken *t, char *buf, int maxLen) {
    int n = t->length < maxLen - 1 ? t->length : maxLen - 1;
    memcpy(buf, t->start, (size_t)n);
    buf[n] = '\0';
}

static void strToUpper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

static int parseRegister(const char *name) {
    /* D0-D7, A0-A7, SP (=A7), PC, SR, CCR, USP */
    if ((name[0] == 'D' || name[0] == 'd') && name[1] >= '0' && name[1] <= '7' && name[2] == '\0')
        return name[1] - '0';
    if ((name[0] == 'A' || name[0] == 'a') && name[1] >= '0' && name[1] <= '7' && name[2] == '\0')
        return name[1] - '0' + 8;
    if (strcasecmp(name, "SP") == 0) return 15; /* A7 */
    return -1;
}

/* ── Register list parsing (for MOVEM): D0-D3/A0-A1, D1, etc. ── */
static bool parseRegList(AsmLexer *lex, AsmOperand *op, int firstReg) {
    u16 mask = (u16)(1 << firstReg);
    int lastReg = firstReg;

    while (true) {
        AsmToken peek = asmLexerPeek(lex);
        if (peek.type == TOK_MINUS) {
            /* Range: lastReg-endReg */
            asmLexerNext(lex); /* consume - */
            AsmToken regTok = asmLexerNext(lex);
            char name[8]; tokToStr(&regTok, name, sizeof(name));
            int endReg = parseRegister(name);
            if (endReg < 0) return false;
            int lo = lastReg < endReg ? lastReg : endReg;
            int hi = lastReg > endReg ? lastReg : endReg;
            for (int i = lo; i <= hi; i++)
                mask |= (u16)(1 << i);
            lastReg = endReg;
        } else if (peek.type == TOK_SLASH) {
            /* Separator: next register or range */
            asmLexerNext(lex); /* consume / */
            AsmToken regTok = asmLexerNext(lex);
            char name[8]; tokToStr(&regTok, name, sizeof(name));
            int reg = parseRegister(name);
            if (reg < 0) return false;
            mask |= (u16)(1 << reg);
            lastReg = reg;
        } else {
            break;
        }
    }

    op->type = OPER_REG_LIST;
    op->regListMask = mask;
    return true;
}

static u8 parseSizeSuffix(AsmLexer *lex) {
    AsmToken peek = asmLexerPeek(lex);
    if (peek.type == TOK_DOT) {
        asmLexerNext(lex); /* consume dot */
        AsmToken sz = asmLexerNext(lex);
        if (sz.type == TOK_IDENTIFIER && sz.length == 1) {
            char c = (char)toupper((unsigned char)sz.start[0]);
            if (c == 'B') return 1;
            if (c == 'W') return 2;
            if (c == 'L') return 4;
            if (c == 'S') return 1; /* short branch */
        }
    }
    return 0;
}

static bool parseOperand(AsmLexer *lex, AsmOperand *op) {
    memset(op, 0, sizeof(*op));
    AsmToken t = asmLexerPeek(lex);

    /* Immediate: #expr */
    if (t.type == TOK_HASH) {
        asmLexerNext(lex);
        t = asmLexerNext(lex);
        op->type = OPER_IMMEDIATE;
        if (t.type == TOK_NUMBER) {
            op->immediate = t.numValue;
        } else if (t.type == TOK_IDENTIFIER) {
            tokToStr(&t, op->symbol, ASM_MAX_LABEL_LEN);
        }
        return true;
    }

    /* Pre-decrement: -(An) */
    if (t.type == TOK_MINUS) {
        asmLexerNext(lex);
        t = asmLexerNext(lex); /* ( */
        if (t.type != TOK_LPAREN) return false;
        t = asmLexerNext(lex); /* register */
        char regName[8]; tokToStr(&t, regName, sizeof(regName));
        int r = parseRegister(regName);
        if (r < 8) return false; /* must be address register */
        asmLexerNext(lex); /* ) */
        op->type = OPER_ADDR_IND_PRE;
        op->reg = r - 8;
        return true;
    }

    /* Register or label */
    if (t.type == TOK_IDENTIFIER) {
        asmLexerNext(lex);
        char name[ASM_MAX_LABEL_LEN];
        tokToStr(&t, name, sizeof(name));

        /* Check for special registers */
        if (strcasecmp(name, "SR") == 0)  { op->type = OPER_SR; return true; }
        if (strcasecmp(name, "CCR") == 0) { op->type = OPER_CCR; return true; }
        if (strcasecmp(name, "USP") == 0) { op->type = OPER_USP; return true; }
        if (strcasecmp(name, "PC") == 0)  { op->type = OPER_EXPRESSION; strncpy(op->symbol, "PC", ASM_MAX_LABEL_LEN); return true; }

        int r = parseRegister(name);
        if (r >= 0 && r < 16) {
            /* Check for register list: D0/A0-A1 or D1-D3 */
            AsmToken peek = asmLexerPeek(lex);
            if (peek.type == TOK_SLASH || peek.type == TOK_MINUS) {
                return parseRegList(lex, op, r);
            }
            if (r < 8)  { op->type = OPER_DATA_REG; op->reg = r; return true; }
            if (r >= 8) { op->type = OPER_ADDR_REG; op->reg = r - 8; return true; }
        }

        /* Must be a label / expression */
        op->type = OPER_EXPRESSION;
        strncpy(op->symbol, name, ASM_MAX_LABEL_LEN);

        /* Check for (An) or (PC) suffix → displacement mode */
        AsmToken peek = asmLexerPeek(lex);
        if (peek.type == TOK_LPAREN) {
            asmLexerNext(lex); /* ( */
            AsmToken reg = asmLexerNext(lex);
            char regName[16]; tokToStr(&reg, regName, sizeof(regName));
            if (strcasecmp(regName, "PC") == 0) {
                op->type = OPER_PC_DISP;
                /* displacement will be resolved later from symbol */
            } else {
                int r = parseRegister(regName);
                if (r >= 8) {
                    op->type = OPER_ADDR_DISP;
                    op->reg = r - 8;
                }
            }
            asmLexerNext(lex); /* ) */
        }
        return true;
    }

    /* Number (could be absolute address or displacement) */
    if (t.type == TOK_NUMBER) {
        asmLexerNext(lex);
        op->type = OPER_EXPRESSION;
        op->immediate = t.numValue;
        /* Check for (An) suffix → displacement mode */
        AsmToken peek = asmLexerPeek(lex);
        if (peek.type == TOK_LPAREN) {
            asmLexerNext(lex); /* ( */
            AsmToken reg = asmLexerNext(lex);
            char regName[8]; tokToStr(&reg, regName, sizeof(regName));
            int r = parseRegister(regName);
            if (r >= 8) {
                op->type = OPER_ADDR_DISP;
                op->reg = r - 8;
                op->displacement = (s32)t.numValue;
                /* TODO: check for index register */
            }
            /* consume ) */
            asmLexerNext(lex);
        }
        return true;
    }

    /* (An), (An)+, d(An), d(An,Xn) */
    if (t.type == TOK_LPAREN) {
        asmLexerNext(lex); /* ( */
        t = asmLexerNext(lex);
        char name[16]; tokToStr(&t, name, sizeof(name));
        int r = parseRegister(name);
        if (r < 8) return false;
        asmLexerNext(lex); /* ) */

        AsmToken peek = asmLexerPeek(lex);
        if (peek.type == TOK_PLUS) {
            asmLexerNext(lex);
            op->type = OPER_ADDR_IND_POST;
            op->reg = r - 8;
        } else {
            op->type = OPER_ADDR_IND;
            op->reg = r - 8;
        }
        return true;
    }

    return false;
}

/* ── main parse ──────────────────────────────────────── */

bool asmParserParseLine(AsmParser *p, AsmLexer *lex, AsmLine *out) {
    (void)p;
    memset(out, 0, sizeof(*out));
    out->line = asmLexerGetLine(lex);

    /* skip blank lines and comments */
    for (;;) {
        AsmToken t = asmLexerPeek(lex);
        if (t.type == TOK_NEWLINE || t.type == TOK_COMMENT) { asmLexerNext(lex); continue; }
        break;
    }

    AsmToken t = asmLexerPeek(lex);
    if (t.type == TOK_EOF) return false;

    /* Label: identifier followed by : or at column 0 */
    if (t.type == TOK_IDENTIFIER) {
        AsmToken id = asmLexerNext(lex);
        AsmToken next = asmLexerPeek(lex);
        if (next.type == TOK_COLON) {
            tokToStr(&id, out->label, ASM_MAX_LABEL_LEN);
            asmLexerNext(lex); /* consume : */
        } else {
            /* Not a label — this is the mnemonic */
            tokToStr(&id, out->mnemonic, ASM_MAX_MNEMONIC);
            strToUpper(out->mnemonic);
            out->size = parseSizeSuffix(lex);
            goto parseOperands;
        }
    }

    /* Mnemonic */
    t = asmLexerPeek(lex);
    if (t.type == TOK_IDENTIFIER) {
        AsmToken mn = asmLexerNext(lex);
        tokToStr(&mn, out->mnemonic, ASM_MAX_MNEMONIC);
        strToUpper(out->mnemonic);
        out->size = parseSizeSuffix(lex);
    }

parseOperands:
    /* Check for directive keywords */
    if (strcmp(out->mnemonic, "ORG") == 0 || strcmp(out->mnemonic, "EQU") == 0 ||
        strcmp(out->mnemonic, "SET") == 0 || strcmp(out->mnemonic, "DC") == 0 ||
        strcmp(out->mnemonic, "DS") == 0  || strcmp(out->mnemonic, "MACRO") == 0 ||
        strcmp(out->mnemonic, "ENDM") == 0 || strcmp(out->mnemonic, "INCLUDE") == 0 ||
        strcmp(out->mnemonic, "INCBIN") == 0 || strcmp(out->mnemonic, "SECTION") == 0 ||
        strcmp(out->mnemonic, "ALIGN") == 0 || strcmp(out->mnemonic, "EVEN") == 0 ||
        strcmp(out->mnemonic, "END") == 0 || strcmp(out->mnemonic, "IF") == 0 ||
        strcmp(out->mnemonic, "ELSE") == 0 || strcmp(out->mnemonic, "ENDIF") == 0 ||
        strcmp(out->mnemonic, "REPT") == 0 || strcmp(out->mnemonic, "ENDR") == 0) {
        out->isDirective = true;
    }

    /* Parse operands — DC directives get special handling for strings and
       comma-separated data values */
    if (out->isDirective && strcmp(out->mnemonic, "DC") == 0) {
        /* DC.B/W/L: parse comma-separated values into dataValues[].
           String literals are expanded to individual bytes. */
        t = asmLexerPeek(lex);
        while (t.type != TOK_NEWLINE && t.type != TOK_EOF && t.type != TOK_COMMENT) {
            if (out->dataCount >= 256) break;
            if (t.type == TOK_STRING) {
                AsmToken str = asmLexerNext(lex);
                for (int i = 0; i < str.length && out->dataCount < 256; i++)
                    out->dataValues[out->dataCount++] = (u8)str.start[i];
            } else if (t.type == TOK_NUMBER) {
                AsmToken num = asmLexerNext(lex);
                out->dataValues[out->dataCount++] = num.numValue;
            } else if (t.type == TOK_IDENTIFIER) {
                /* Symbol reference — store as operand for later resolution */
                if (out->operandCount < ASM_MAX_OPERANDS) {
                    if (!parseOperand(lex, &out->operands[out->operandCount])) break;
                    out->operandCount++;
                } else break;
            } else if (t.type == TOK_HASH) {
                /* #immediate — skip hash, parse number */
                asmLexerNext(lex);
                t = asmLexerPeek(lex);
                if (t.type == TOK_NUMBER) {
                    AsmToken num = asmLexerNext(lex);
                    out->dataValues[out->dataCount++] = num.numValue;
                }
            } else break;
            t = asmLexerPeek(lex);
            if (t.type == TOK_COMMA) { asmLexerNext(lex); t = asmLexerPeek(lex); }
            else break;
        }
    } else {
        t = asmLexerPeek(lex);
        while (t.type != TOK_NEWLINE && t.type != TOK_EOF && t.type != TOK_COMMENT) {
            if (out->operandCount >= ASM_MAX_OPERANDS) break;
            if (!parseOperand(lex, &out->operands[out->operandCount])) break;
            out->operandCount++;
            t = asmLexerPeek(lex);
            if (t.type == TOK_COMMA) { asmLexerNext(lex); t = asmLexerPeek(lex); }
            else break;
        }
    }

    /* Consume rest of line */
    while (t.type != TOK_NEWLINE && t.type != TOK_EOF) {
        asmLexerNext(lex);
        t = asmLexerPeek(lex);
    }

    return true;
}
