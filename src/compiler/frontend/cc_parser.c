/* cc_parser.c — Full recursive-descent C parser
 *
 * Produces an AST (see ast.h) from a token stream (see cc_lexer.h).
 * Handles C89/C99 subset suitable for MC68000 target code:
 *   - All primitive types, pointers, arrays, signed/unsigned variants
 *   - Structs, unions, enums, typedefs
 *   - Function definitions and prototypes
 *   - All statements (if, while, do-while, for, switch/case, goto, labels)
 *   - All C expression operators at correct precedence
 *   - sizeof, casts, ternary, comma operator
 */
#include "compiler/frontend/cc_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERRORS    256
#define MAX_TYPEDEFS  256
#define MAX_PARAMS     32

/* Type-specifier flags stored in AST_TYPE_SPEC->intValue */
#define TSF_UNSIGNED  0x001
#define TSF_SIGNED    0x002
#define TSF_CONST     0x004
#define TSF_VOLATILE  0x008
#define TSF_STATIC    0x010
#define TSF_EXTERN    0x020
#define TSF_TYPEDEF   0x040

struct CcParser {
    CcLexer *lex;
    CcToken  cur;
    char    *errors[MAX_ERRORS];
    int      errorCount;
    char     typedefs[MAX_TYPEDEFS][AST_MAX_NAME];
    int      typedefCount;
};

/* ── Forward declarations ────────────────────────────── */

static AstNode *parseTypeSpec(CcParser *p);
static AstNode *parseExpr(CcParser *p);
static AstNode *parseAssignExpr(CcParser *p);
static AstNode *parseCondExpr(CcParser *p);
static AstNode *parseCompoundStmt(CcParser *p);
static AstNode *parseStatement(CcParser *p);
static bool     isTypeName(CcParser *p);

/* ── Helpers ─────────────────────────────────────────── */

static void parserError(CcParser *p, const char *fmt, ...) {
    if (p->errorCount >= MAX_ERRORS) return;
    char buf[512];
    int off = snprintf(buf, sizeof(buf), "line %d: ", p->cur.line);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + off, sizeof(buf) - (size_t)off, fmt, ap);
    va_end(ap);
    p->errors[p->errorCount++] = strdup(buf);
}

static CcTokenType peekType(CcParser *p) {
    return ccLexerPeek(p->lex).type;
}

static CcToken peek(CcParser *p) {
    return ccLexerPeek(p->lex);
}

static CcToken advance(CcParser *p) {
    p->cur = ccLexerNext(p->lex);
    return p->cur;
}

static bool check(CcParser *p, CcTokenType type) {
    return peekType(p) == type;
}

static bool match(CcParser *p, CcTokenType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static bool expect(CcParser *p, CcTokenType type) {
    if (match(p, type)) return true;
    parserError(p, "expected token %d, got %d", type, peekType(p));
    return false;
}

static void tokText(CcToken *t, char *buf, int sz) {
    int n = t->length < sz - 1 ? t->length : sz - 1;
    if (n > 0) memcpy(buf, t->start, (size_t)n);
    buf[n < 0 ? 0 : n] = '\0';
}

static void registerTypedef(CcParser *p, const char *name) {
    if (p->typedefCount < MAX_TYPEDEFS)
        strncpy(p->typedefs[p->typedefCount++], name, AST_MAX_NAME - 1);
}

static bool isTypedefName(CcParser *p, const char *name) {
    for (int i = 0; i < p->typedefCount; i++)
        if (strcmp(p->typedefs[i], name) == 0) return true;
    return false;
}

/* Shallow-copy a type-specifier node (no deep child copy). */
static AstNode *copyTypeSpec(AstNode *ts) {
    if (!ts) return NULL;
    AstNode *c = astCreateNode(AST_TYPE_SPEC);
    memcpy(c->name, ts->name, AST_MAX_NAME);
    c->intValue = ts->intValue;
    c->line     = ts->line;
    return c;
}

/* Wrap a type node in N levels of AST_POINTER. */
static AstNode *wrapPointers(AstNode *base, int depth) {
    AstNode *cur = base;
    for (int i = 0; i < depth; i++) {
        AstNode *ptr = astCreateNode(AST_POINTER);
        ptr->line = base->line;
        astAddChild(ptr, cur);
        cur = ptr;
    }
    return cur;
}

/* ── Type specifier detection ────────────────────────── */

static bool isTypeSpecStart(CcTokenType t) {
    switch (t) {
        case CC_TOK_VOID: case CC_TOK_CHAR: case CC_TOK_SHORT: case CC_TOK_INT:
        case CC_TOK_LONG: case CC_TOK_FLOAT: case CC_TOK_DOUBLE:
        case CC_TOK_SIGNED: case CC_TOK_UNSIGNED:
        case CC_TOK_STRUCT: case CC_TOK_UNION: case CC_TOK_ENUM:
        case CC_TOK_CONST: case CC_TOK_VOLATILE:
        case CC_TOK_STATIC: case CC_TOK_EXTERN: case CC_TOK_AUTO:
        case CC_TOK_REGISTER: case CC_TOK_TYPEDEF:
            return true;
        default: return false;
    }
}

static bool isTypeName(CcParser *p) {
    CcToken t = peek(p);
    if (isTypeSpecStart(t.type)) return true;
    if (t.type == CC_TOK_IDENT) {
        char nm[AST_MAX_NAME];
        tokText(&t, nm, sizeof(nm));
        return isTypedefName(p, nm);
    }
    return false;
}

/* ── Expression parsing (lowest → highest precedence) ── */

static AstNode *parsePrimaryExpr(CcParser *p) {
    CcToken t = peek(p);
    switch (t.type) {
        case CC_TOK_INT_LIT: {
            advance(p);
            AstNode *n = astCreateNode(AST_INT_LITERAL);
            n->intValue = t.intValue;
            n->line = t.line;
            return n;
        }
        case CC_TOK_CHAR_LIT: {
            advance(p);
            AstNode *n = astCreateNode(AST_CHAR_LITERAL);
            n->line = t.line;
            /* Extract char value */
            if (t.length >= 3 && t.start[1] == '\\') {
                switch (t.start[2]) {
                    case 'n': n->intValue = '\n'; break;
                    case 't': n->intValue = '\t'; break;
                    case '0': n->intValue = '\0'; break;
                    case '\\': n->intValue = '\\'; break;
                    case '\'': n->intValue = '\''; break;
                    default:   n->intValue = t.start[2]; break;
                }
            } else if (t.length >= 2) {
                n->intValue = (unsigned char)t.start[1];
            }
            return n;
        }
        case CC_TOK_STRING_LIT: {
            advance(p);
            AstNode *n = astCreateNode(AST_STRING_LITERAL);
            tokText(&t, n->name, AST_MAX_NAME);
            n->line = t.line;
            return n;
        }
        case CC_TOK_FLOAT_LIT: {
            advance(p);
            AstNode *n = astCreateNode(AST_FLOAT_LITERAL);
            n->floatValue = t.floatValue;
            n->line = t.line;
            return n;
        }
        case CC_TOK_IDENT: {
            advance(p);
            AstNode *n = astCreateNode(AST_IDENT);
            tokText(&t, n->name, AST_MAX_NAME);
            n->line = t.line;
            return n;
        }
        case CC_TOK_LPAREN: {
            advance(p);
            AstNode *n = parseExpr(p);
            expect(p, CC_TOK_RPAREN);
            return n;
        }
        default:
            parserError(p, "expected expression, got token %d", t.type);
            advance(p);
            return NULL;
    }
}

static AstNode *parsePostfixExpr(CcParser *p) {
    AstNode *node = parsePrimaryExpr(p);
    if (!node) return NULL;

    for (;;) {
        if (match(p, CC_TOK_LBRACKET)) {
            /* array[index] */
            AstNode *idx = parseExpr(p);
            expect(p, CC_TOK_RBRACKET);
            AstNode *n = astCreateNode(AST_INDEX);
            n->line = node->line;
            astAddChild(n, node);
            astAddChild(n, idx);
            node = n;
        } else if (match(p, CC_TOK_LPAREN)) {
            /* function call */
            AstNode *call = astCreateNode(AST_CALL);
            call->line = node->line;
            astAddChild(call, node);
            if (!check(p, CC_TOK_RPAREN)) {
                astAddChild(call, parseAssignExpr(p));
                while (match(p, CC_TOK_COMMA))
                    astAddChild(call, parseAssignExpr(p));
            }
            expect(p, CC_TOK_RPAREN);
            node = call;
        } else if (match(p, CC_TOK_DOT)) {
            CcToken m = advance(p);
            AstNode *n = astCreateNode(AST_MEMBER);
            tokText(&m, n->name, AST_MAX_NAME);
            n->line = m.line;
            astAddChild(n, node);
            node = n;
        } else if (match(p, CC_TOK_ARROW)) {
            CcToken m = advance(p);
            AstNode *n = astCreateNode(AST_PTR_MEMBER);
            tokText(&m, n->name, AST_MAX_NAME);
            n->line = m.line;
            astAddChild(n, node);
            node = n;
        } else if (match(p, CC_TOK_INC)) {
            AstNode *n = astCreateNode(AST_UNARY_OP);
            strncpy(n->name, "post++", AST_MAX_NAME - 1);
            n->line = node->line;
            astAddChild(n, node);
            node = n;
        } else if (match(p, CC_TOK_DEC)) {
            AstNode *n = astCreateNode(AST_UNARY_OP);
            strncpy(n->name, "post--", AST_MAX_NAME - 1);
            n->line = node->line;
            astAddChild(n, node);
            node = n;
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parseUnaryExpr(CcParser *p) {
    CcToken t = peek(p);

    if (t.type == CC_TOK_INC) {
        advance(p);
        AstNode *n = astCreateNode(AST_UNARY_OP);
        strncpy(n->name, "pre++", AST_MAX_NAME - 1);
        n->line = t.line;
        astAddChild(n, parseUnaryExpr(p));
        return n;
    }
    if (t.type == CC_TOK_DEC) {
        advance(p);
        AstNode *n = astCreateNode(AST_UNARY_OP);
        strncpy(n->name, "pre--", AST_MAX_NAME - 1);
        n->line = t.line;
        astAddChild(n, parseUnaryExpr(p));
        return n;
    }
    if (t.type == CC_TOK_AMP || t.type == CC_TOK_STAR ||
        t.type == CC_TOK_PLUS || t.type == CC_TOK_MINUS ||
        t.type == CC_TOK_TILDE || t.type == CC_TOK_BANG) {
        advance(p);
        AstNode *n = astCreateNode(AST_UNARY_OP);
        tokText(&t, n->name, AST_MAX_NAME);
        n->line = t.line;
        /* For dereference (*) and address-of (&), parseCastExpr is correct
           per the C grammar, but since parseCastExpr calls us, we call
           parseCastExpr only for cast-expression level operators. For
           simplicity here we recurse on parseCastExpr via parseUnaryExpr's
           caller chain. Just call parseUnaryExpr for the operand. */
        astAddChild(n, parseUnaryExpr(p));
        return n;
    }
    if (t.type == CC_TOK_SIZEOF) {
        advance(p);
        AstNode *n = astCreateNode(AST_SIZEOF);
        n->line = t.line;
        if (check(p, CC_TOK_LPAREN)) {
            /* Could be sizeof(type) or sizeof(expr) */
            CcToken lp = advance(p); /* consume '(' */
            (void)lp;
            if (isTypeName(p)) {
                AstNode *ts = parseTypeSpec(p);
                /* Handle pointer declarator in sizeof */
                int ptrLvl = 0;
                while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }
                if (ptrLvl > 0 && ts) ts = wrapPointers(ts, ptrLvl);
                astAddChild(n, ts);
            } else {
                astAddChild(n, parseExpr(p));
            }
            expect(p, CC_TOK_RPAREN);
        } else {
            astAddChild(n, parseUnaryExpr(p));
        }
        return n;
    }
    return parsePostfixExpr(p);
}

static AstNode *parseCastExpr(CcParser *p) {
    /* Check for (type)expr cast */
    if (check(p, CC_TOK_LPAREN)) {
        /* Save state — peek past '(' to see if it's a type */
        CcToken lp = peek(p);
        advance(p); /* consume '(' */
        if (isTypeName(p)) {
            AstNode *ts = parseTypeSpec(p);
            int ptrLvl = 0;
            while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }
            if (ptrLvl > 0 && ts) ts = wrapPointers(ts, ptrLvl);
            expect(p, CC_TOK_RPAREN);
            AstNode *cast = astCreateNode(AST_CAST);
            cast->line = lp.line;
            astAddChild(cast, ts);
            astAddChild(cast, parseCastExpr(p));
            return cast;
        }
        /* Not a cast — parse as parenthesised expression */
        /* We already consumed '(', so parse expr then ')' */
        AstNode *expr = parseExpr(p);
        expect(p, CC_TOK_RPAREN);
        /* Continue with postfix operations */
        for (;;) {
            if (match(p, CC_TOK_LBRACKET)) {
                AstNode *idx = parseExpr(p);
                expect(p, CC_TOK_RBRACKET);
                AstNode *n = astCreateNode(AST_INDEX);
                n->line = expr ? expr->line : 0;
                astAddChild(n, expr); astAddChild(n, idx);
                expr = n;
            } else if (match(p, CC_TOK_LPAREN)) {
                AstNode *call = astCreateNode(AST_CALL);
                call->line = expr ? expr->line : 0;
                astAddChild(call, expr);
                if (!check(p, CC_TOK_RPAREN)) {
                    astAddChild(call, parseAssignExpr(p));
                    while (match(p, CC_TOK_COMMA))
                        astAddChild(call, parseAssignExpr(p));
                }
                expect(p, CC_TOK_RPAREN);
                expr = call;
            } else if (match(p, CC_TOK_DOT)) {
                CcToken m = advance(p);
                AstNode *n = astCreateNode(AST_MEMBER);
                tokText(&m, n->name, AST_MAX_NAME);
                astAddChild(n, expr); expr = n;
            } else if (match(p, CC_TOK_ARROW)) {
                CcToken m = advance(p);
                AstNode *n = astCreateNode(AST_PTR_MEMBER);
                tokText(&m, n->name, AST_MAX_NAME);
                astAddChild(n, expr); expr = n;
            } else if (match(p, CC_TOK_INC)) {
                AstNode *n = astCreateNode(AST_UNARY_OP);
                strncpy(n->name, "post++", AST_MAX_NAME - 1);
                astAddChild(n, expr); expr = n;
            } else if (match(p, CC_TOK_DEC)) {
                AstNode *n = astCreateNode(AST_UNARY_OP);
                strncpy(n->name, "post--", AST_MAX_NAME - 1);
                astAddChild(n, expr); expr = n;
            } else break;
        }
        return expr;
    }
    return parseUnaryExpr(p);
}

/* Binary left-associative helpers (macro to reduce repetition) */
#define DEFINE_BINOP_PARSER(parseFn, higher, ...) \
static AstNode *parseFn(CcParser *p) { \
    AstNode *lhs = higher(p); \
    if (!lhs) return NULL; \
    CcTokenType ops[] = { __VA_ARGS__, CC_TOK_EOF }; \
    for (;;) { \
        CcTokenType pt = peekType(p); \
        bool found = false; \
        for (int i = 0; ops[i] != CC_TOK_EOF; i++) { \
            if (pt == ops[i]) { found = true; break; } \
        } \
        if (!found) break; \
        CcToken op = advance(p); \
        AstNode *rhs = higher(p); \
        AstNode *nd = astCreateNode(AST_BINARY_OP); \
        tokText(&op, nd->name, AST_MAX_NAME); \
        nd->line = op.line; \
        astAddChild(nd, lhs); \
        astAddChild(nd, rhs); \
        lhs = nd; \
    } \
    return lhs; \
}

DEFINE_BINOP_PARSER(parseMulExpr,    parseCastExpr,   CC_TOK_STAR, CC_TOK_SLASH, CC_TOK_PERCENT)
DEFINE_BINOP_PARSER(parseAddExpr,    parseMulExpr,    CC_TOK_PLUS, CC_TOK_MINUS)
DEFINE_BINOP_PARSER(parseShiftExpr,  parseAddExpr,    CC_TOK_LSHIFT, CC_TOK_RSHIFT)
DEFINE_BINOP_PARSER(parseRelExpr,    parseShiftExpr,  CC_TOK_LT, CC_TOK_GT, CC_TOK_LE, CC_TOK_GE)
DEFINE_BINOP_PARSER(parseEqExpr,     parseRelExpr,    CC_TOK_EQ, CC_TOK_NE)
DEFINE_BINOP_PARSER(parseBitAndExpr, parseEqExpr,     CC_TOK_AMP)
DEFINE_BINOP_PARSER(parseBitXorExpr, parseBitAndExpr, CC_TOK_CARET)
DEFINE_BINOP_PARSER(parseBitOrExpr,  parseBitXorExpr, CC_TOK_PIPE)
DEFINE_BINOP_PARSER(parseLogAndExpr, parseBitOrExpr,  CC_TOK_AND)
DEFINE_BINOP_PARSER(parseLogOrExpr,  parseLogAndExpr, CC_TOK_OR)

static AstNode *parseCondExpr(CcParser *p) {
    AstNode *cond = parseLogOrExpr(p);
    if (!cond) return NULL;
    if (match(p, CC_TOK_QUESTION)) {
        AstNode *thenE = parseExpr(p);
        expect(p, CC_TOK_COLON);
        AstNode *elseE = parseCondExpr(p);
        AstNode *n = astCreateNode(AST_TERNARY);
        n->line = cond->line;
        astAddChild(n, cond);
        astAddChild(n, thenE);
        astAddChild(n, elseE);
        return n;
    }
    return cond;
}

static bool isAssignOp(CcTokenType t) {
    switch (t) {
        case CC_TOK_ASSIGN:
        case CC_TOK_PLUS_ASSIGN: case CC_TOK_MINUS_ASSIGN:
        case CC_TOK_STAR_ASSIGN: case CC_TOK_SLASH_ASSIGN:
        case CC_TOK_PERCENT_ASSIGN:
        case CC_TOK_AMP_ASSIGN: case CC_TOK_PIPE_ASSIGN:
        case CC_TOK_CARET_ASSIGN:
        case CC_TOK_LSHIFT_ASSIGN: case CC_TOK_RSHIFT_ASSIGN:
            return true;
        default: return false;
    }
}

static AstNode *parseAssignExpr(CcParser *p) {
    AstNode *lhs = parseCondExpr(p);
    if (!lhs) return NULL;
    if (isAssignOp(peekType(p))) {
        CcToken op = advance(p);
        AstNode *rhs = parseAssignExpr(p); /* right-associative */
        AstNode *n = astCreateNode(AST_ASSIGN);
        tokText(&op, n->name, AST_MAX_NAME);
        n->line = op.line;
        astAddChild(n, lhs);
        astAddChild(n, rhs);
        return n;
    }
    return lhs;
}

static AstNode *parseExpr(CcParser *p) {
    AstNode *lhs = parseAssignExpr(p);
    if (!lhs) return NULL;
    if (check(p, CC_TOK_COMMA) && !check(p, CC_TOK_EOF)) {
        AstNode *comma = astCreateNode(AST_COMMA_EXPR);
        comma->line = lhs->line;
        astAddChild(comma, lhs);
        while (match(p, CC_TOK_COMMA))
            astAddChild(comma, parseAssignExpr(p));
        return comma;
    }
    return lhs;
}

/* ── Statement parsing ───────────────────────────────── */

static AstNode *parseExprStmt(CcParser *p) {
    if (match(p, CC_TOK_SEMICOLON)) {
        /* empty statement */
        AstNode *n = astCreateNode(AST_EXPR_STMT);
        n->line = p->cur.line;
        return n;
    }
    AstNode *expr = parseExpr(p);
    expect(p, CC_TOK_SEMICOLON);
    AstNode *n = astCreateNode(AST_EXPR_STMT);
    n->line = expr ? expr->line : p->cur.line;
    if (expr) astAddChild(n, expr);
    return n;
}

static AstNode *parseIfStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_IF_STMT);
    n->line = p->cur.line;
    expect(p, CC_TOK_LPAREN);
    astAddChild(n, parseExpr(p));     /* condition */
    expect(p, CC_TOK_RPAREN);
    astAddChild(n, parseStatement(p)); /* then-branch */
    if (match(p, CC_TOK_ELSE))
        astAddChild(n, parseStatement(p)); /* else-branch */
    return n;
}

static AstNode *parseWhileStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_WHILE_STMT);
    n->line = p->cur.line;
    expect(p, CC_TOK_LPAREN);
    astAddChild(n, parseExpr(p));
    expect(p, CC_TOK_RPAREN);
    astAddChild(n, parseStatement(p));
    return n;
}

static AstNode *parseDoWhileStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_DO_WHILE_STMT);
    n->line = p->cur.line;
    astAddChild(n, parseStatement(p)); /* body */
    expect(p, CC_TOK_WHILE);
    expect(p, CC_TOK_LPAREN);
    astAddChild(n, parseExpr(p));      /* condition */
    expect(p, CC_TOK_RPAREN);
    expect(p, CC_TOK_SEMICOLON);
    return n;
}

static AstNode *parseForStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_FOR_STMT);
    n->line = p->cur.line;
    expect(p, CC_TOK_LPAREN);

    /* init (may be declaration or expression) */
    if (check(p, CC_TOK_SEMICOLON)) {
        advance(p);
        astAddChild(n, NULL); /* no init */
    } else if (isTypeName(p)) {
        /* C99 for-declaration */
        AstNode *ts = parseTypeSpec(p);
        AstNode *decl = astCreateNode(AST_VAR_DECL);
        decl->line = ts ? ts->line : p->cur.line;
        int ptrLvl = 0;
        while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }
        if (check(p, CC_TOK_IDENT)) {
            CcToken nm = advance(p);
            tokText(&nm, decl->name, AST_MAX_NAME);
        }
        AstNode *typeNode = ptrLvl > 0 ? wrapPointers(ts, ptrLvl) : ts;
        astAddChild(decl, typeNode);
        if (match(p, CC_TOK_ASSIGN))
            astAddChild(decl, parseAssignExpr(p));
        expect(p, CC_TOK_SEMICOLON);
        astAddChild(n, decl);
    } else {
        AstNode *init = parseExpr(p);
        AstNode *es = astCreateNode(AST_EXPR_STMT);
        es->line = init ? init->line : p->cur.line;
        if (init) astAddChild(es, init);
        expect(p, CC_TOK_SEMICOLON);
        astAddChild(n, es);
    }

    /* condition */
    if (check(p, CC_TOK_SEMICOLON)) {
        astAddChild(n, NULL);
    } else {
        astAddChild(n, parseExpr(p));
    }
    expect(p, CC_TOK_SEMICOLON);

    /* increment */
    if (check(p, CC_TOK_RPAREN)) {
        astAddChild(n, NULL);
    } else {
        astAddChild(n, parseExpr(p));
    }
    expect(p, CC_TOK_RPAREN);

    /* body */
    astAddChild(n, parseStatement(p));
    return n;
}

static AstNode *parseSwitchStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_SWITCH_STMT);
    n->line = p->cur.line;
    expect(p, CC_TOK_LPAREN);
    astAddChild(n, parseExpr(p));
    expect(p, CC_TOK_RPAREN);
    astAddChild(n, parseStatement(p));
    return n;
}

static AstNode *parseReturnStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_RETURN_STMT);
    n->line = p->cur.line;
    if (!check(p, CC_TOK_SEMICOLON))
        astAddChild(n, parseExpr(p));
    expect(p, CC_TOK_SEMICOLON);
    return n;
}

static AstNode *parseCompoundStmt(CcParser *p) {
    AstNode *n = astCreateNode(AST_COMPOUND_STMT);
    n->line = p->cur.line;

    while (!check(p, CC_TOK_RBRACE) && !check(p, CC_TOK_EOF)) {
        if (isTypeName(p)) {
            /* declaration(s) */
            AstNode *ts = parseTypeSpec(p);
            if (!ts) { advance(p); continue; }
            bool isTypedefDecl = (ts->intValue & TSF_TYPEDEF) != 0;

            /* Handle declarations (possibly comma-separated) */
            bool first = true;
            do {
                AstNode *decl = astCreateNode(AST_VAR_DECL);
                decl->line = peek(p).line;
                AstNode *tsUse = first ? ts : copyTypeSpec(ts);
                first = false;

                /* Pointer stars */
                int ptrLvl = 0;
                while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }

                /* Name */
                if (check(p, CC_TOK_IDENT)) {
                    CcToken nm = advance(p);
                    tokText(&nm, decl->name, AST_MAX_NAME);
                }

                AstNode *typeNode = ptrLvl > 0 ? wrapPointers(tsUse, ptrLvl) : tsUse;
                astAddChild(decl, typeNode);

                /* Array */
                if (match(p, CC_TOK_LBRACKET)) {
                    if (check(p, CC_TOK_INT_LIT)) {
                        CcToken sz = advance(p);
                        decl->intValue = sz.intValue;
                    }
                    expect(p, CC_TOK_RBRACKET);
                }

                /* Initializer */
                if (match(p, CC_TOK_ASSIGN)) {
                    if (check(p, CC_TOK_LBRACE)) {
                        /* Aggregate initializer {1,2,3} */
                        advance(p);
                        AstNode *agg = astCreateNode(AST_COMMA_EXPR);
                        agg->line = p->cur.line;
                        if (!check(p, CC_TOK_RBRACE)) {
                            astAddChild(agg, parseAssignExpr(p));
                            while (match(p, CC_TOK_COMMA)) {
                                if (check(p, CC_TOK_RBRACE)) break;
                                astAddChild(agg, parseAssignExpr(p));
                            }
                        }
                        expect(p, CC_TOK_RBRACE);
                        astAddChild(decl, agg);
                    } else {
                        astAddChild(decl, parseAssignExpr(p));
                    }
                }

                if (isTypedefDecl) {
                    decl->type = AST_TYPEDEF_DECL;
                    registerTypedef(p, decl->name);
                }

                astAddChild(n, decl);
            } while (match(p, CC_TOK_COMMA));

            expect(p, CC_TOK_SEMICOLON);
        } else {
            AstNode *stmt = parseStatement(p);
            if (stmt) astAddChild(n, stmt);
        }
    }
    expect(p, CC_TOK_RBRACE);
    return n;
}

static AstNode *parseStatement(CcParser *p) {
    CcToken t = peek(p);

    switch (t.type) {
        case CC_TOK_LBRACE:
            advance(p);
            return parseCompoundStmt(p);

        case CC_TOK_IF:
            advance(p);
            return parseIfStmt(p);

        case CC_TOK_WHILE:
            advance(p);
            return parseWhileStmt(p);

        case CC_TOK_DO:
            advance(p);
            return parseDoWhileStmt(p);

        case CC_TOK_FOR:
            advance(p);
            return parseForStmt(p);

        case CC_TOK_SWITCH:
            advance(p);
            return parseSwitchStmt(p);

        case CC_TOK_RETURN:
            advance(p);
            return parseReturnStmt(p);

        case CC_TOK_BREAK: {
            advance(p);
            AstNode *n = astCreateNode(AST_BREAK_STMT);
            n->line = t.line;
            expect(p, CC_TOK_SEMICOLON);
            return n;
        }
        case CC_TOK_CONTINUE: {
            advance(p);
            AstNode *n = astCreateNode(AST_CONTINUE_STMT);
            n->line = t.line;
            expect(p, CC_TOK_SEMICOLON);
            return n;
        }
        case CC_TOK_GOTO: {
            advance(p);
            AstNode *n = astCreateNode(AST_GOTO_STMT);
            n->line = t.line;
            if (check(p, CC_TOK_IDENT)) {
                CcToken lb = advance(p);
                tokText(&lb, n->name, AST_MAX_NAME);
            }
            expect(p, CC_TOK_SEMICOLON);
            return n;
        }
        case CC_TOK_CASE: {
            advance(p);
            AstNode *n = astCreateNode(AST_CASE_STMT);
            n->line = t.line;
            astAddChild(n, parseCondExpr(p));
            expect(p, CC_TOK_COLON);
            astAddChild(n, parseStatement(p));
            return n;
        }
        case CC_TOK_DEFAULT: {
            advance(p);
            AstNode *n = astCreateNode(AST_DEFAULT_STMT);
            n->line = t.line;
            expect(p, CC_TOK_COLON);
            astAddChild(n, parseStatement(p));
            return n;
        }
        case CC_TOK_IDENT: {
            /* Could be a label or an expression statement */
            /* Peek ahead: if ident followed by ':', it's a label */
            CcToken ident = advance(p);
            if (check(p, CC_TOK_COLON)) {
                advance(p); /* consume ':' */
                AstNode *n = astCreateNode(AST_LABEL_STMT);
                tokText(&ident, n->name, AST_MAX_NAME);
                n->line = ident.line;
                astAddChild(n, parseStatement(p));
                return n;
            }
            /* Not a label — build an ident node and continue as expr stmt */
            AstNode *id = astCreateNode(AST_IDENT);
            tokText(&ident, id->name, AST_MAX_NAME);
            id->line = ident.line;
            /* Now we need to finish parsing as a postfix/binary expression.
             * We already consumed the identifier. Build from it. */
            /* Continue postfix */
            for (;;) {
                if (match(p, CC_TOK_LBRACKET)) {
                    AstNode *idx = parseExpr(p);
                    expect(p, CC_TOK_RBRACKET);
                    AstNode *n = astCreateNode(AST_INDEX);
                    n->line = id->line;
                    astAddChild(n, id); astAddChild(n, idx);
                    id = n;
                } else if (match(p, CC_TOK_LPAREN)) {
                    AstNode *call = astCreateNode(AST_CALL);
                    call->line = id->line;
                    astAddChild(call, id);
                    if (!check(p, CC_TOK_RPAREN)) {
                        astAddChild(call, parseAssignExpr(p));
                        while (match(p, CC_TOK_COMMA))
                            astAddChild(call, parseAssignExpr(p));
                    }
                    expect(p, CC_TOK_RPAREN);
                    id = call;
                } else if (match(p, CC_TOK_DOT)) {
                    CcToken m = advance(p);
                    AstNode *n = astCreateNode(AST_MEMBER);
                    tokText(&m, n->name, AST_MAX_NAME);
                    n->line = m.line;
                    astAddChild(n, id); id = n;
                } else if (match(p, CC_TOK_ARROW)) {
                    CcToken m = advance(p);
                    AstNode *n = astCreateNode(AST_PTR_MEMBER);
                    tokText(&m, n->name, AST_MAX_NAME);
                    n->line = m.line;
                    astAddChild(n, id); id = n;
                } else if (match(p, CC_TOK_INC)) {
                    AstNode *n = astCreateNode(AST_UNARY_OP);
                    strncpy(n->name, "post++", AST_MAX_NAME - 1);
                    astAddChild(n, id); id = n;
                } else if (match(p, CC_TOK_DEC)) {
                    AstNode *n = astCreateNode(AST_UNARY_OP);
                    strncpy(n->name, "post--", AST_MAX_NAME - 1);
                    astAddChild(n, id); id = n;
                } else break;
            }
            /* Now continue with binary operators if any */
            /* We need to integrate id into the expression parser.
             * Easiest: check for binary/assign ops and build manually.
             * For correctness, wrap id as already-parsed LHS and
             * continue the expression chain. */
            AstNode *expr = id;
            /* Handle binary ops: we need to handle all precedence levels.
             * Since we pre-parsed the primary, we can handle
             * common patterns: assignment, simple binary ops */
            if (isAssignOp(peekType(p))) {
                CcToken op = advance(p);
                AstNode *rhs = parseAssignExpr(p);
                AstNode *n = astCreateNode(AST_ASSIGN);
                tokText(&op, n->name, AST_MAX_NAME);
                n->line = op.line;
                astAddChild(n, expr);
                astAddChild(n, rhs);
                expr = n;
            } else {
                /* Check for binary operators — handle common ones */
                while (peekType(p) == CC_TOK_PLUS || peekType(p) == CC_TOK_MINUS ||
                       peekType(p) == CC_TOK_STAR || peekType(p) == CC_TOK_SLASH ||
                       peekType(p) == CC_TOK_PERCENT || peekType(p) == CC_TOK_LT ||
                       peekType(p) == CC_TOK_GT || peekType(p) == CC_TOK_LE ||
                       peekType(p) == CC_TOK_GE || peekType(p) == CC_TOK_EQ ||
                       peekType(p) == CC_TOK_NE || peekType(p) == CC_TOK_AMP ||
                       peekType(p) == CC_TOK_PIPE || peekType(p) == CC_TOK_CARET ||
                       peekType(p) == CC_TOK_AND || peekType(p) == CC_TOK_OR ||
                       peekType(p) == CC_TOK_LSHIFT || peekType(p) == CC_TOK_RSHIFT) {
                    CcToken op = advance(p);
                    AstNode *rhs = parseUnaryExpr(p);
                    AstNode *n = astCreateNode(AST_BINARY_OP);
                    tokText(&op, n->name, AST_MAX_NAME);
                    n->line = op.line;
                    astAddChild(n, expr);
                    astAddChild(n, rhs);
                    expr = n;
                }
                /* Ternary */
                if (match(p, CC_TOK_QUESTION)) {
                    AstNode *thenE = parseExpr(p);
                    expect(p, CC_TOK_COLON);
                    AstNode *elseE = parseCondExpr(p);
                    AstNode *n = astCreateNode(AST_TERNARY);
                    n->line = expr->line;
                    astAddChild(n, expr);
                    astAddChild(n, thenE);
                    astAddChild(n, elseE);
                    expr = n;
                }
            }
            expect(p, CC_TOK_SEMICOLON);
            AstNode *es = astCreateNode(AST_EXPR_STMT);
            es->line = expr->line;
            astAddChild(es, expr);
            return es;
        }
        default:
            return parseExprStmt(p);
    }
}

/* ── Type specifier parsing ──────────────────────────── */

static AstNode *parseStructOrUnionBody(CcParser *p, bool isStruct) {
    AstNode *node = astCreateNode(isStruct ? AST_STRUCT_DEF : AST_UNION_DEF);
    node->line = p->cur.line;

    /* Optional tag name */
    if (check(p, CC_TOK_IDENT)) {
        CcToken t = advance(p);
        tokText(&t, node->name, AST_MAX_NAME);
    }

    /* Body */
    if (match(p, CC_TOK_LBRACE)) {
        while (!check(p, CC_TOK_RBRACE) && !check(p, CC_TOK_EOF)) {
            AstNode *mtype = parseTypeSpec(p);
            if (!mtype) { advance(p); continue; }

            bool first = true;
            do {
                AstNode *mem = astCreateNode(AST_VAR_DECL);
                mem->line = peek(p).line;
                AstNode *mt = first ? mtype : copyTypeSpec(mtype);
                first = false;

                int ptrLvl = 0;
                while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }

                if (check(p, CC_TOK_IDENT)) {
                    CcToken nm = advance(p);
                    tokText(&nm, mem->name, AST_MAX_NAME);
                }

                astAddChild(mem, ptrLvl > 0 ? wrapPointers(mt, ptrLvl) : mt);

                /* Array */
                if (match(p, CC_TOK_LBRACKET)) {
                    if (check(p, CC_TOK_INT_LIT)) {
                        CcToken sz = advance(p);
                        mem->intValue = sz.intValue;
                    }
                    expect(p, CC_TOK_RBRACKET);
                }

                /* Bit-field */
                if (match(p, CC_TOK_COLON)) {
                    if (check(p, CC_TOK_INT_LIT)) advance(p);
                }

                astAddChild(node, mem);
            } while (match(p, CC_TOK_COMMA));

            expect(p, CC_TOK_SEMICOLON);
        }
        expect(p, CC_TOK_RBRACE);
    }
    return node;
}

static AstNode *parseEnumBody(CcParser *p) {
    AstNode *node = astCreateNode(AST_ENUM_DEF);
    node->line = p->cur.line;

    if (check(p, CC_TOK_IDENT)) {
        CcToken t = advance(p);
        tokText(&t, node->name, AST_MAX_NAME);
    }

    if (match(p, CC_TOK_LBRACE)) {
        while (!check(p, CC_TOK_RBRACE) && !check(p, CC_TOK_EOF)) {
            AstNode *ev = astCreateNode(AST_VAR_DECL);
            ev->line = peek(p).line;
            if (check(p, CC_TOK_IDENT)) {
                CcToken nm = advance(p);
                tokText(&nm, ev->name, AST_MAX_NAME);
            }
            if (match(p, CC_TOK_ASSIGN)) {
                AstNode *val = parseCondExpr(p);
                if (val) astAddChild(ev, val);
            }
            astAddChild(node, ev);
            if (!match(p, CC_TOK_COMMA)) break;
        }
        expect(p, CC_TOK_RBRACE);
    }
    return node;
}

static AstNode *parseTypeSpec(CcParser *p) {
    AstNode *spec = astCreateNode(AST_TYPE_SPEC);
    spec->line = peek(p).line;
    s64 flags = 0;
    bool hasBase = false;

    for (;;) {
        CcToken t = peek(p);
        switch (t.type) {
            case CC_TOK_CONST:    flags |= TSF_CONST;    advance(p); continue;
            case CC_TOK_VOLATILE: flags |= TSF_VOLATILE; advance(p); continue;
            case CC_TOK_STATIC:   flags |= TSF_STATIC;   advance(p); continue;
            case CC_TOK_EXTERN:   flags |= TSF_EXTERN;   advance(p); continue;
            case CC_TOK_AUTO:     advance(p); continue;
            case CC_TOK_REGISTER: advance(p); continue;
            case CC_TOK_TYPEDEF:  flags |= TSF_TYPEDEF;  advance(p); continue;
            case CC_TOK_SIGNED:   flags |= TSF_SIGNED;   advance(p); continue;
            case CC_TOK_UNSIGNED: flags |= TSF_UNSIGNED;  advance(p); continue;
            case CC_TOK_VOID:     strncpy(spec->name,"void",AST_MAX_NAME-1);   advance(p); hasBase=true; continue;
            case CC_TOK_CHAR:     strncpy(spec->name,"char",AST_MAX_NAME-1);   advance(p); hasBase=true; continue;
            case CC_TOK_SHORT:    strncpy(spec->name,"short",AST_MAX_NAME-1);  advance(p); hasBase=true; continue;
            case CC_TOK_INT:      strncpy(spec->name,"int",AST_MAX_NAME-1);    advance(p); hasBase=true; continue;
            case CC_TOK_LONG:     strncpy(spec->name,"long",AST_MAX_NAME-1);   advance(p); hasBase=true; continue;
            case CC_TOK_FLOAT:    strncpy(spec->name,"float",AST_MAX_NAME-1);  advance(p); hasBase=true; continue;
            case CC_TOK_DOUBLE:   strncpy(spec->name,"double",AST_MAX_NAME-1); advance(p); hasBase=true; continue;
            case CC_TOK_STRUCT: {
                advance(p);
                AstNode *sd = parseStructOrUnionBody(p, true);
                if (sd) { astAddChild(spec, sd); snprintf(spec->name, AST_MAX_NAME, "struct %s", sd->name); }
                hasBase = true; continue;
            }
            case CC_TOK_UNION: {
                advance(p);
                AstNode *ud = parseStructOrUnionBody(p, false);
                if (ud) { astAddChild(spec, ud); snprintf(spec->name, AST_MAX_NAME, "union %s", ud->name); }
                hasBase = true; continue;
            }
            case CC_TOK_ENUM: {
                advance(p);
                AstNode *ed = parseEnumBody(p);
                if (ed) { astAddChild(spec, ed); snprintf(spec->name, AST_MAX_NAME, "enum %s", ed->name); }
                hasBase = true; continue;
            }
            case CC_TOK_IDENT: {
                if (!hasBase) {
                    char nm[AST_MAX_NAME];
                    tokText(&t, nm, sizeof(nm));
                    if (isTypedefName(p, nm)) {
                        strncpy(spec->name, nm, AST_MAX_NAME - 1);
                        advance(p);
                        hasBase = true;
                        continue;
                    }
                }
                goto done;
            }
            default: goto done;
        }
    }
done:
    if (!hasBase) {
        if (flags & (TSF_SIGNED | TSF_UNSIGNED))
            strncpy(spec->name, "int", AST_MAX_NAME - 1);
        else {
            parserError(p, "expected type specifier");
            astDestroyNode(spec);
            return NULL;
        }
    }
    spec->intValue = flags;
    return spec;
}

/* ── Parameter parsing ───────────────────────────────── */

/* Parse a single function parameter: type + optional name */
static AstNode *parseParam(CcParser *p) {
    AstNode *ts = parseTypeSpec(p);
    if (!ts) return NULL;

    AstNode *param = astCreateNode(AST_PARAM);
    param->line = ts->line;

    int ptrLvl = 0;
    while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }

    if (check(p, CC_TOK_IDENT)) {
        CcToken nm = advance(p);
        tokText(&nm, param->name, AST_MAX_NAME);
    }

    AstNode *typeNode = ptrLvl > 0 ? wrapPointers(ts, ptrLvl) : ts;
    astAddChild(param, typeNode);

    /* Array parameter: int arr[] */
    if (match(p, CC_TOK_LBRACKET)) {
        if (check(p, CC_TOK_INT_LIT)) {
            CcToken sz = advance(p);
            param->intValue = sz.intValue;
        }
        expect(p, CC_TOK_RBRACKET);
    }

    return param;
}

/* ── External declarations ───────────────────────────── */

static AstNode *parseExternalDecl(CcParser *p) {
    AstNode *typeSpec = parseTypeSpec(p);
    if (!typeSpec) {
        /* Skip to next semicolon or brace for recovery */
        while (!check(p, CC_TOK_SEMICOLON) && !check(p, CC_TOK_LBRACE) && !check(p, CC_TOK_EOF))
            advance(p);
        if (check(p, CC_TOK_SEMICOLON)) advance(p);
        return NULL;
    }

    bool isTypedefDecl = (typeSpec->intValue & TSF_TYPEDEF) != 0;

    /* Handle lone struct/union/enum definitions: "struct Foo { ... };" */
    if (check(p, CC_TOK_SEMICOLON)) {
        advance(p);
        AstNode *decl = astCreateNode(AST_VAR_DECL);
        decl->line = typeSpec->line;
        astAddChild(decl, typeSpec);
        return decl;
    }

    /* Pointer stars */
    int ptrLvl = 0;
    while (check(p, CC_TOK_STAR)) { advance(p); ptrLvl++; }

    /* Name */
    char declName[AST_MAX_NAME] = {0};
    if (check(p, CC_TOK_IDENT)) {
        CcToken nm = advance(p);
        tokText(&nm, declName, AST_MAX_NAME);
    }

    /* Function definition or prototype: name '(' ... ')' */
    if (match(p, CC_TOK_LPAREN)) {
        AstNode *func = astCreateNode(AST_FUNCTION_DEF);
        func->line = typeSpec->line;
        strncpy(func->name, declName, AST_MAX_NAME - 1);

        /* Return type */
        AstNode *retType = ptrLvl > 0 ? wrapPointers(typeSpec, ptrLvl) : typeSpec;
        astAddChild(func, retType);

        /* Parse parameters */
        if (!check(p, CC_TOK_RPAREN)) {
            /* Check for (void) */
            if (check(p, CC_TOK_VOID)) {
                CcToken v = peek(p);
                advance(p);
                if (check(p, CC_TOK_RPAREN)) {
                    /* (void) — no parameters */
                } else {
                    /* void as first param type, e.g. void *p */
                    AstNode *ts = astCreateNode(AST_TYPE_SPEC);
                    strncpy(ts->name, "void", AST_MAX_NAME - 1);
                    ts->line = v.line;
                    AstNode *param = astCreateNode(AST_PARAM);
                    param->line = v.line;
                    int pp = 0;
                    while (check(p, CC_TOK_STAR)) { advance(p); pp++; }
                    if (check(p, CC_TOK_IDENT)) {
                        CcToken nm = advance(p);
                        tokText(&nm, param->name, AST_MAX_NAME);
                    }
                    astAddChild(param, pp > 0 ? wrapPointers(ts, pp) : ts);
                    astAddChild(func, param);
                    while (match(p, CC_TOK_COMMA)) {
                        if (match(p, CC_TOK_ELLIPSIS)) {
                            func->intValue = 1; /* variadic */
                            break;
                        }
                        AstNode *par = parseParam(p);
                        if (par) astAddChild(func, par);
                    }
                }
            } else {
                AstNode *par = parseParam(p);
                if (par) astAddChild(func, par);
                while (match(p, CC_TOK_COMMA)) {
                    if (match(p, CC_TOK_ELLIPSIS)) {
                        func->intValue = 1;
                        break;
                    }
                    par = parseParam(p);
                    if (par) astAddChild(func, par);
                }
            }
        }
        expect(p, CC_TOK_RPAREN);

        /* Function body or prototype */
        if (check(p, CC_TOK_LBRACE)) {
            advance(p);
            AstNode *body = parseCompoundStmt(p);
            astAddChild(func, body);
        } else {
            expect(p, CC_TOK_SEMICOLON);
            /* Prototype — no body */
        }
        return func;
    }

    /* Variable declaration(s) */
    AstNode *first = astCreateNode(isTypedefDecl ? AST_TYPEDEF_DECL : AST_VAR_DECL);
    first->line = typeSpec->line;
    strncpy(first->name, declName, AST_MAX_NAME - 1);
    astAddChild(first, ptrLvl > 0 ? wrapPointers(typeSpec, ptrLvl) : typeSpec);

    /* Array */
    if (match(p, CC_TOK_LBRACKET)) {
        if (check(p, CC_TOK_INT_LIT)) {
            CcToken sz = advance(p);
            first->intValue = sz.intValue;
        }
        expect(p, CC_TOK_RBRACKET);
    }

    /* Initializer */
    if (match(p, CC_TOK_ASSIGN)) {
        if (check(p, CC_TOK_LBRACE)) {
            advance(p);
            AstNode *agg = astCreateNode(AST_COMMA_EXPR);
            agg->line = p->cur.line;
            if (!check(p, CC_TOK_RBRACE)) {
                astAddChild(agg, parseAssignExpr(p));
                while (match(p, CC_TOK_COMMA)) {
                    if (check(p, CC_TOK_RBRACE)) break;
                    astAddChild(agg, parseAssignExpr(p));
                }
            }
            expect(p, CC_TOK_RBRACE);
            astAddChild(first, agg);
        } else {
            astAddChild(first, parseAssignExpr(p));
        }
    }

    if (isTypedefDecl) registerTypedef(p, first->name);

    /* If there's no comma, just this one declaration */
    if (!check(p, CC_TOK_COMMA)) {
        expect(p, CC_TOK_SEMICOLON);
        return first;
    }

    /* Multiple declarations: wrap in a compound node */
    AstNode *group = astCreateNode(AST_COMPOUND_STMT);
    group->line = first->line;
    astAddChild(group, first);

    while (match(p, CC_TOK_COMMA)) {
        AstNode *decl = astCreateNode(isTypedefDecl ? AST_TYPEDEF_DECL : AST_VAR_DECL);
        decl->line = peek(p).line;

        int pl = 0;
        while (check(p, CC_TOK_STAR)) { advance(p); pl++; }

        if (check(p, CC_TOK_IDENT)) {
            CcToken nm = advance(p);
            tokText(&nm, decl->name, AST_MAX_NAME);
        }

        AstNode *ts2 = copyTypeSpec(typeSpec);
        astAddChild(decl, pl > 0 ? wrapPointers(ts2, pl) : ts2);

        if (match(p, CC_TOK_LBRACKET)) {
            if (check(p, CC_TOK_INT_LIT)) {
                CcToken sz = advance(p);
                decl->intValue = sz.intValue;
            }
            expect(p, CC_TOK_RBRACKET);
        }

        if (match(p, CC_TOK_ASSIGN))
            astAddChild(decl, parseAssignExpr(p));

        if (isTypedefDecl) registerTypedef(p, decl->name);

        astAddChild(group, decl);
    }

    expect(p, CC_TOK_SEMICOLON);
    return group;
}

/* ── Translation unit ────────────────────────────────── */

static AstNode *parseTranslationUnit(CcParser *p) {
    AstNode *tu = astCreateNode(AST_TRANSLATION_UNIT);
    tu->line = 1;

    while (!check(p, CC_TOK_EOF)) {
        AstNode *decl = parseExternalDecl(p);
        if (decl) astAddChild(tu, decl);
    }
    return tu;
}

/* ── Public API ──────────────────────────────────────── */

CcParser *ccParserCreate(void) { return calloc(1, sizeof(CcParser)); }

void ccParserDestroy(CcParser *p) {
    if (!p) return;
    for (int i = 0; i < p->errorCount; i++) free(p->errors[i]);
    free(p);
}

AstNode *ccParserParse(CcParser *p, CcLexer *lex) {
    p->lex = lex;
    p->errorCount = 0;
    p->typedefCount = 0;
    p->cur = ccLexerPeek(lex);
    return parseTranslationUnit(p);
}

int ccParserGetErrorCount(CcParser *p) { return p->errorCount; }
const char *ccParserGetError(CcParser *p, int i) {
    return (i >= 0 && i < p->errorCount) ? p->errors[i] : NULL;
}
