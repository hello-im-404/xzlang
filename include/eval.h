#ifndef EVAL_H
#define EVAL_H

#include "ast.h"
#include <stdbool.h>

typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_FUNC
} ValueType;

typedef struct {
    ValueType type;
    union {
        long long i;
        double f;
        char* s;
        bool b;
        ASTNode* func_node;
    } as;
} Value;

typedef struct EnvNode {
    const char* name;
    int name_length;
    Value value;
    struct EnvNode* next;
} EnvNode;

typedef struct {
    EnvNode* head;
} Environment;

void env_init(Environment* env);
void env_define(Environment* env, const char* name, int length, Value value);
bool env_get(Environment* env, const char* name, int length, Value* out_value);
void env_free(Environment* env);

void eval_init();
Value eval(ASTNode* node, Environment* env);

#endif 
