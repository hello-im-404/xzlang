#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void env_init(Environment* env) {
    env->head = NULL;
}

void env_define(Environment* env, const char* name, int length, Value value) {
    EnvNode* node = (EnvNode*)malloc(sizeof(EnvNode));
    node->name = name;
    node->name_length = length;
    node->value = value;
    node->next = env->head;
    env->head = node;
}

bool env_get(Environment* env, const char* name, int length, Value* out_value) {
    EnvNode* current = env->head;
    while (current != NULL) {
        if (current->name_length == length && strncmp(current->name, name, length) == 0) {
            *out_value = current->value;
            return true;
        }
        current = current->next;
    }
    return false;
}

void env_free(Environment* env) {
    EnvNode* current = env->head;
    while (current != NULL) {
        EnvNode* next = current->next;
        free(current);
        current = next;
    }
    env->head = NULL;
}

void eval_init() {
    // idk.
}

static Value eval_literal(ASTNode* node) {
    Value val;
    if (node->token.type == TOKEN_INT_LITERAL) {
        val.type = VAL_INT;
        val.as.i = atoll(node->token.start);
    } else if (node->token.type == TOKEN_FLOAT_LITERAL) {
        val.type = VAL_FLOAT;
        val.as.f = atof(node->token.start);
    } else if (node->token.type == TOKEN_STRING_LITERAL) {
        val.type = VAL_STRING;
        int len = node->token.length - 2;
        val.as.s = (char*)malloc(len + 1);
        strncpy(val.as.s, node->token.start + 1, len);
        val.as.s[len] = '\0';
    } else if (strncmp(node->token.start, "true", node->token.length) == 0) {
        val.type = VAL_BOOL;
        val.as.b = true;
    } else if (strncmp(node->token.start, "false", node->token.length) == 0) {
        val.type = VAL_BOOL;
        val.as.b = false;
    } else {
        val.type = VAL_NULL;
    }
    return val;
}

static void print_value(Value val) {
    switch (val.type) {
        case VAL_INT: printf("%lld", val.as.i); break;
        case VAL_FLOAT: printf("%g", val.as.f); break;
        case VAL_STRING: printf("%s", val.as.s); break;
        case VAL_BOOL: printf(val.as.b ? "true" : "false"); break;
        case VAL_NULL: printf("ntg"); break;
    }
}

bool is_tml_loaded = false;

Value eval(ASTNode* node, Environment* env) {
    Value result = { .type = VAL_NULL };
    if (!node) return result;

    switch (node->type) {
        case AST_PROGRAM: {
            for (int i = 0; i < node->child_count; i++) {
                ASTNode* child = node->children[i];
                if (child->type == AST_USING || child->type == AST_VAR_DECL || child->type == AST_FUNC_DECL) {
                    eval(child, env);
                } else {
                    fprintf(stderr, "Runtime Error: Only declarations and using statements are allowed in global scope.\n");
                    exit(1);
                }
            }
            
            Value start_func;
            if (env_get(env, "start", 5, &start_func) && start_func.type == VAL_FUNC) {
                ASTNode* block = NULL;
                for (int i = 0; i < start_func.as.func_node->child_count; i++) {
                    if (start_func.as.func_node->children[i]->type == AST_BLOCK) {
                        block = start_func.as.func_node->children[i];
                        break;
                    }
                }
                if (block) {
                    result = eval(block, env);
                }
            }
            break;
        }

        case AST_BLOCK:
            for (int i = 0; i < node->child_count; i++) {
                eval(node->children[i], env);
            }
            break;

        case AST_USING: {
            for (int i = 0; i < node->child_count; i++) {
                Value lib = eval(node->children[i], env);
                if (lib.type == VAL_STRING) {
                    if (strcmp(lib.as.s, "tml") == 0) {
                        is_tml_loaded = true;
                    }
                }
            }
            break;
        }

        case AST_FUNC_DECL: {
            Value func_val;
            func_val.type = VAL_FUNC;
            func_val.as.func_node = node;
            env_define(env, node->token.start, node->token.length, func_val);
            break;
        }

        case AST_VAR_DECL: {
            Value var_val = { .type = VAL_NULL };
            if (node->child_count > 0) {
                var_val = eval(node->children[0], env); 
                
                if (node->data_type.length > 0) {
                    if (strncmp(node->data_type.start, "i64", 3) == 0 ||
                        strncmp(node->data_type.start, "i32", 3) == 0 ||
                        strncmp(node->data_type.start, "i16", 3) == 0) {
                        if (var_val.type != VAL_INT && var_val.type != VAL_NULL) {
                            fprintf(stderr, "Runtime Error: Type mismatch. Expected integer for variable '%.*s'\n", node->token.length, node->token.start);
                            exit(1);
                        }
                    } else if (strncmp(node->data_type.start, "f64", 3) == 0 ||
                               strncmp(node->data_type.start, "f32", 3) == 0) {
                        if (var_val.type != VAL_FLOAT && var_val.type != VAL_NULL) {
                            fprintf(stderr, "Runtime Error: Type mismatch. Expected float for variable '%.*s'\n", node->token.length, node->token.start);
                            exit(1);
                        }
                    } else if (strncmp(node->data_type.start, "str", 3) == 0) {
                        if (var_val.type != VAL_STRING && var_val.type != VAL_NULL) {
                            fprintf(stderr, "Runtime Error: Type mismatch. Expected string for variable '%.*s'\n", node->token.length, node->token.start);
                            exit(1);
                        }
                    } else if (strncmp(node->data_type.start, "bool", 4) == 0) {
                        if (var_val.type != VAL_BOOL && var_val.type != VAL_NULL) {
                            fprintf(stderr, "Runtime Error: Type mismatch. Expected boolean for variable '%.*s'\n", node->token.length, node->token.start);
                            exit(1);
                        }
                    }
                }
            }
            env_define(env, node->token.start, node->token.length, var_val);
            break;
        }

        case AST_LITERAL:
            return eval_literal(node);

        case AST_VARIABLE: {
            if (strncmp(node->token.start, "true", 4) == 0 || strncmp(node->token.start, "false", 5) == 0) {
                 return eval_literal(node);
            }
            if (!env_get(env, node->token.start, node->token.length, &result)) {
                fprintf(stderr, "Runtime error: Undefined variable '%.*s'\n", node->token.length, node->token.start);
            }
            return result;
        }

        case AST_FUNC_CALL: {
            if (node->token.length == 4 && strncmp(node->token.start, "prms", 4) == 0) {
                if (!is_tml_loaded) {
                    fprintf(stderr, "Runtime Error: 'prms' requires 'tml' library. Add using{\"tml\"}.\n");
                    exit(1);
                }
                if (node->child_count > 0) {
                    Value first_arg = eval(node->children[0], env);
                    
                    if (first_arg.type == VAL_STRING && strchr(first_arg.as.s, '~') != NULL) {
                        char* fmt = first_arg.as.s;
                        int arg_idx = 1;
                        for (int i = 0; fmt[i] != '\0'; i++) {
                            if (fmt[i] == '~' && fmt[i+1] != '\0') {
                                i++;
                                char spec = fmt[i];
                                if (arg_idx < node->child_count) {
                                    Value arg = eval(node->children[arg_idx++], env);
                                    print_value(arg);
                                } else {
                                    printf("~%c", spec);
                                }
                            } else {
                                putchar(fmt[i]);
                            }
                        }
                    } else {
                        print_value(first_arg);
                        if (node->child_count > 1) printf(" ");
                        for (int i = 1; i < node->child_count; i++) {
                            Value arg = eval(node->children[i], env);
                            print_value(arg);
                            if (i < node->child_count - 1) printf(" ");
                        }
                    }
                }
                printf("\n");
            } else if (node->token.length == 3 && strncmp(node->token.start, "get", 3) == 0) {
                if (!is_tml_loaded) {
                    fprintf(stderr, "Runtime Error: 'get' requires 'tml' library. Add using{\"tml\"}.\n");
                    exit(1);
                }
                printf("Runtime Warning: 'get' is not fully implemented yet.\n");
            } else {
                Value func;
                if (!env_get(env, node->token.start, node->token.length, &func)) {
                    fprintf(stderr, "Runtime Error: Undefined function '%.*s'\n", node->token.length, node->token.start);
                    exit(1);
                }
                if (func.type == VAL_FUNC) {
                    ASTNode* func_node = func.as.func_node;
                    ASTNode* block = NULL;
                    for (int i = 0; i < func_node->child_count; i++) {
                        if (func_node->children[i]->type == AST_BLOCK) {
                            block = func_node->children[i];
                            break;
                        }
                    }
                    if (block) {
                        return eval(block, env);
                    }
                } else {
                    fprintf(stderr, "Runtime Error: '%.*s' is not a function\n", node->token.length, node->token.start);
                    exit(1);
                }
            }
            break;
        }

        case AST_EXPR_STMT:
            if (node->child_count > 0) {
                return eval(node->children[0], env);
            }
            break;
            
        case AST_UNKNOWN: {
            if (node->child_count == 0) return result;
            if (node->child_count == 1) return eval(node->children[0], env);
            
            Value acc = eval(node->children[0], env);
            for (int i = 1; i < node->child_count - 1; i += 2) {
                ASTNode* op_node = node->children[i];
                Value right = eval(node->children[i+1], env);
                
                if (op_node->type == AST_BINARY_EXPR) {
                    if (acc.type == VAL_INT && right.type == VAL_INT) {
                        if (op_node->token.type == TOKEN_PLUS) acc.as.i += right.as.i;
                        else if (op_node->token.type == TOKEN_MINUS) acc.as.i -= right.as.i;
                        else if (op_node->token.type == TOKEN_STAR) acc.as.i *= right.as.i;
                        else if (op_node->token.type == TOKEN_SLASH) {
                            if (right.as.i == 0) { fprintf(stderr, "Runtime Error: Division by zero.\n"); exit(1); }
                            acc.as.i /= right.as.i;
                        }
                    } else if (acc.type == VAL_STRING && right.type == VAL_STRING && op_node->token.type == TOKEN_PLUS) {
                        int len = strlen(acc.as.s) + strlen(right.as.s);
                        char* new_str = (char*)malloc(len + 1);
                        strcpy(new_str, acc.as.s);
                        strcat(new_str, right.as.s);
                        acc.type = VAL_STRING;
                        acc.as.s = new_str;
                    } else {
                        double l = (acc.type == VAL_INT) ? (double)acc.as.i : acc.as.f;
                        double r = (right.type == VAL_INT) ? (double)right.as.i : right.as.f;
                        acc.type = VAL_FLOAT;
                        if (op_node->token.type == TOKEN_PLUS) acc.as.f = l + r;
                        else if (op_node->token.type == TOKEN_MINUS) acc.as.f = l - r;
                        else if (op_node->token.type == TOKEN_STAR) acc.as.f = l * r;
                        else if (op_node->token.type == TOKEN_SLASH) {
                            if (r == 0) { fprintf(stderr, "Runtime Error: Division by zero.\n"); exit(1); }
                            acc.as.f = l / r;
                        }
                    }
                }
            }
            return acc;
        }

        default:
            break;
    }

    return result;
}
