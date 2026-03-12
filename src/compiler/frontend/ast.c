/* ast.c — Abstract Syntax Tree */
#include "compiler/frontend/ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

AstNode *astCreateNode(AstNodeType type) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->type = type;
    return n;
}

void astDestroyNode(AstNode *node) {
    if (!node) return;
    for (int i = 0; i < node->childCount; i++)
        astDestroyNode(node->children[i]);
    free(node->children);
    free(node);
}

void astAddChild(AstNode *parent, AstNode *child) {
    if (!parent || !child) return;
    if (parent->childCount >= parent->childCapacity) {
        int newCap = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        parent->children = realloc(parent->children, (size_t)newCap * sizeof(AstNode *));
        parent->childCapacity = newCap;
    }
    parent->children[parent->childCount++] = child;
}

static const char *astTypeName(AstNodeType t) {
    switch (t) {
        case AST_TRANSLATION_UNIT: return "TranslationUnit";
        case AST_FUNCTION_DEF:     return "FunctionDef";
        case AST_VAR_DECL:         return "VarDecl";
        case AST_COMPOUND_STMT:    return "CompoundStmt";
        case AST_RETURN_STMT:      return "ReturnStmt";
        case AST_IF_STMT:          return "IfStmt";
        case AST_WHILE_STMT:       return "WhileStmt";
        case AST_FOR_STMT:         return "ForStmt";
        case AST_INT_LITERAL:      return "IntLiteral";
        case AST_STRING_LITERAL:   return "StringLiteral";
        case AST_IDENT:            return "Ident";
        case AST_BINARY_OP:        return "BinaryOp";
        case AST_UNARY_OP:         return "UnaryOp";
        case AST_CALL:             return "Call";
        case AST_ASSIGN:           return "Assign";
        case AST_EXPR_STMT:        return "ExprStmt";
        default:                   return "Node";
    }
}

void astPrint(AstNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s", astTypeName(node->type));
    if (node->name[0]) printf(" '%s'", node->name);
    if (node->type == AST_INT_LITERAL) printf(" = %lld", (long long)node->intValue);
    printf("\n");
    for (int i = 0; i < node->childCount; i++)
        astPrint(node->children[i], indent + 1);
}
