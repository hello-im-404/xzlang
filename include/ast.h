#ifndef AST_H
#define AST_H

#include "token.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_VAR_DECL,
    AST_PARAM,
    AST_BLOCK,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_RET_STMT,
    AST_EXPR_STMT,
    AST_UNARY_EXPR,
    AST_BINARY_EXPR,
    AST_LITERAL,
    AST_VARIABLE,
    AST_FUNC_CALL,
    AST_ARRAY,
    AST_ARRAY_ACCESS,
    AST_GAIN,
    AST_DROP,
    AST_SYSCALL,
    AST_USING,
    AST_UNKNOWN
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    Token token;      
    Token data_type; 
    struct ASTNode** children;
    int child_count;
    int child_capacity;
    int is_postfix;
    long long array_size; 
} ASTNode;

ASTNode* ast_create_node(ASTNodeType type, Token token);
void ast_add_child(ASTNode* parent, ASTNode* child);
void ast_print(ASTNode* node, int depth);
void ast_free(ASTNode* node);

#endif 
