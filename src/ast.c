#include "ast.h"
#include <stdlib.h>
#include <stdio.h>

ASTNode* ast_create_node(ASTNodeType type, Token token) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = type;
    node->token = token;
    node->data_type = (Token){0}; // init to empty
    node->child_count = 0;
    node->child_capacity = 4;
    node->is_postfix = 0;
    node->array_size = 0;
    node->children = (ASTNode**)malloc(sizeof(ASTNode*) * node->child_capacity);
    return node;
}

void ast_add_child(ASTNode* parent, ASTNode* child) {
    if (!child) return;
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = (ASTNode**)realloc(parent->children, sizeof(ASTNode*) * parent->child_capacity);
    }
    parent->children[parent->child_count++] = child;
}

static const char* type_to_string(ASTNodeType type) {
    switch (type) {
        case AST_PROGRAM: return "Program";
        case AST_FUNC_DECL: return "FuncDecl";
        case AST_VAR_DECL: return "VarDecl";
        case AST_PARAM: return "Param";
        case AST_BLOCK: return "Block";
        case AST_IF_STMT: return "IfStmt";
        case AST_WHILE_STMT: return "WhileStmt";
        case AST_FOR_STMT: return "ForStmt";
        case AST_RET_STMT: return "Return";
        case AST_EXPR_STMT: return "ExprStmt";
        case AST_UNARY_EXPR: return "UnaryExpr";
        case AST_BINARY_EXPR: return "BinaryExpr";
        case AST_LITERAL: return "Literal";
        case AST_VARIABLE: return "Variable";
        case AST_FUNC_CALL: return "FuncCall";
        case AST_ARRAY: return "Array";
        case AST_ARRAY_ACCESS: return "ArrayAccess";
        case AST_GAIN: return "Gain";
        case AST_DROP: return "Drop";
        case AST_SYSCALL: return "Syscall";
        case AST_USING: return "Using";
        default: return "Unknown";
    }
}

void ast_print(ASTNode* node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s", type_to_string(node->type));
    
    if (node->token.length > 0 && node->type != AST_PROGRAM && node->type != AST_BLOCK) {
        printf(" ('%.*s')", node->token.length, node->token.start);
    }
    printf("\n");
    
    for (int i = 0; i < node->child_count; i++) {
        ast_print(node->children[i], depth + 1);
    }
}

void ast_free(ASTNode* node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        ast_free(node->children[i]);
    }
    free(node->children);
    free(node);
}
