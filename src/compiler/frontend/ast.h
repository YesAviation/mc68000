/* ast.h — Abstract Syntax Tree */
#ifndef M68K_CC_AST_H
#define M68K_CC_AST_H

#include "common/types.h"

typedef enum {
    /* Top-level */
    AST_TRANSLATION_UNIT,
    AST_FUNCTION_DEF,
    AST_VAR_DECL,
    AST_TYPEDEF_DECL,

    /* Statements */
    AST_COMPOUND_STMT,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_DO_WHILE_STMT,
    AST_FOR_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_SWITCH_STMT,
    AST_CASE_STMT,
    AST_DEFAULT_STMT,
    AST_GOTO_STMT,
    AST_LABEL_STMT,
    AST_EXPR_STMT,

    /* Expressions */
    AST_INT_LITERAL,
    AST_STRING_LITERAL,
    AST_CHAR_LITERAL,
    AST_FLOAT_LITERAL,
    AST_IDENT,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_CALL,
    AST_INDEX,
    AST_MEMBER,
    AST_PTR_MEMBER,
    AST_CAST,
    AST_SIZEOF,
    AST_TERNARY,
    AST_ASSIGN,
    AST_COMMA_EXPR,

    /* Types */
    AST_TYPE_SPEC,
    AST_POINTER,
    AST_ARRAY,
    AST_STRUCT_DEF,
    AST_UNION_DEF,
    AST_ENUM_DEF,
    AST_PARAM
} AstNodeType;

#define AST_MAX_CHILDREN 16
#define AST_MAX_NAME     128

typedef struct AstNode {
    AstNodeType    type;
    char           name[AST_MAX_NAME];    /* identifiers, operators */
    s64            intValue;
    double         floatValue;
    struct AstNode **children;
    int            childCount;
    int            childCapacity;
    int            line;
    /* Type info (filled during semantic analysis) */
    struct CcType  *resolvedType;
} AstNode;

AstNode *astCreateNode(AstNodeType type);
void     astDestroyNode(AstNode *node);
void     astAddChild(AstNode *parent, AstNode *child);
void     astPrint(AstNode *node, int indent);

#endif /* M68K_CC_AST_H */
