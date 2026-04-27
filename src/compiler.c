#include "compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char strings[256][512];
static int string_count = 0;

typedef struct {
    char name[64];
    int offset;
    int is_array;
} Var;

static Var vars[128];
static int var_count = 0;
static int stack_offset = 0;
static int if_label_count = 0;
static int is_in_start = 0;

static void compile_expr(ASTNode* node, FILE* out);

static Var* find_var_info(Token tok) {
    for (int i = 0; i < var_count; i++) {
        if (vars[i].name[tok.length] == '\0' && strncmp(vars[i].name, tok.start, tok.length) == 0) return &vars[i];
    }
    return NULL;
}

static int find_var(Token tok) {
    Var* v = find_var_info(tok);
    return v ? v->offset : -1;
}

static ASTNode* unwrap(ASTNode* node) {
    if (!node) return NULL;
    if (node->type == AST_UNKNOWN && node->child_count == 1) return unwrap(node->children[0]);
    return node;
}

static void compile_expr(ASTNode* node, FILE* out) {
    if (!node) return;
    if (node->type == AST_UNKNOWN && node->child_count == 1) { compile_expr(node->children[0], out); return; }

    switch (node->type) {
        case AST_LITERAL:
            if (node->token.type == TOKEN_INT_LITERAL) fprintf(out, "    mov rax, %.*s\n    push rax\n", node->token.length, node->token.start);
            else if (node->token.type == TOKEN_STRING_LITERAL) {
                int idx = string_count++;
                int fi = 0;
                for (int i=1; i<node->token.length-1; i++) {
                    if (node->token.start[i] == '\\' && i+1 < node->token.length-1) {
                        if (node->token.start[i+1] == 'n') { strings[idx][fi++] = 10; i++; continue; }
                    }
                    strings[idx][fi++] = node->token.start[i];
                }
                strings[idx][fi] = 0;
                fprintf(out, "    lea rax, [LC%d]\n    push rax\n", idx);
            }
            break;
        case AST_ARRAY_ACCESS: {
            Var* v = find_var_info(node->token);
            if (v) {
                compile_expr(node->children[0], out);
                fprintf(out, "    pop rax\n    imul rax, 8\n");
                if (v->is_array) fprintf(out, "    mov rbx, rbp\n    sub rbx, %d\n    sub rbx, rax\n", v->offset);
                else { fprintf(out, "    mov rbx, [rbp - %d]\n    add rbx, rax\n", v->offset); }
                fprintf(out, "    mov rax, [rbx]\n    push rax\n");
            } else fprintf(out, "    push 0\n");
            break;
        }
        case AST_VARIABLE:
            if (node->token.length == 4 && strncmp(node->token.start, "true", 4) == 0) fprintf(out, "    push 1\n");
            else if (node->token.length == 5 && strncmp(node->token.start, "false", 5) == 0) fprintf(out, "    push 0\n");
            else {
                int off = find_var(node->token);
                if (off != -1) fprintf(out, "    mov rax, [rbp - %d]\n    push rax\n", off);
                else fprintf(out, "    push 0\n");
            }
            break;
        case AST_UNARY_EXPR:
            if (node->token.type == TOKEN_CARET) { compile_expr(node->children[0], out); fprintf(out, "    pop rax\n    mov rax, [rax]\n    push rax\n"); }
            else if (node->token.type == TOKEN_ADDR) {
                ASTNode* vnode = unwrap(node->children[0]);
                Var* v = find_var_info(vnode->token);
                if (v) fprintf(out, "    lea rax, [rbp - %d]\n    push rax\n", v->offset);
                else fprintf(out, "    push 0\n");
            } else if (node->token.type == TOKEN_TILDE) compile_expr(node->children[0], out);
            else {
                ASTNode* vnode = unwrap(node->children[0]);
                int off = find_var(vnode->token);
                if (off != -1) {
                    fprintf(out, "    mov rax, [rbp - %d]\n", off);
                    if (node->is_postfix) fprintf(out, "    push rax\n");
                    if (node->token.type == TOKEN_PLUS_PLUS) fprintf(out, "    add rax, 1\n");
                    else fprintf(out, "    sub rax, 1\n");
                    fprintf(out, "    mov [rbp - %d], rax\n", off);
                    if (!node->is_postfix) fprintf(out, "    push rax\n");
                } else fprintf(out, "    push 0\n");
            }
            break;
        case AST_SYSCALL: {
            const char* regs[] = {"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"};
            for (int i = 0; i < node->child_count && i < 7; i++) compile_expr(node->children[i], out);
            for (int i = node->child_count - 1; i >= 0 && i < 7; i--) fprintf(out, "    pop %s\n", regs[i]);
            fprintf(out, "    syscall\n    push rax\n");
            break;
        }
        case AST_GAIN:
            compile_expr(node->children[0], out);
            fprintf(out, "    pop rax\n    imul rax, 8\n    mov rdi, rax\n    call _xz_alloc\n    push rax\n");
            break;
        case AST_UNKNOWN:
            if (node->child_count == 0) return;
            if (node->child_count >= 3 && node->children[1]->token.type == TOKEN_EQUALS) {
                ASTNode* left = unwrap(node->children[0]);
                compile_expr(node->children[2], out);
                if (left->type == AST_VARIABLE) {
                    fprintf(out, "    pop rax\n"); int off = find_var(left->token);
                    if (off != -1) fprintf(out, "    mov [rbp - %d], rax\n", off);
                    fprintf(out, "    push rax\n");
                } else if (left->type == AST_ARRAY_ACCESS) {
                    compile_expr(left->children[0], out);
                    fprintf(out, "    pop rax\n    imul rax, 8\n    pop rbx\n");
                    Var* v = find_var_info(left->token);
                    if (v) {
                        if (v->is_array) fprintf(out, "    mov rcx, rbp\n    sub rcx, %d\n    sub rcx, rax\n", v->offset);
                        else { fprintf(out, "    mov rcx, [rbp - %d]\n    add rcx, rax\n", v->offset); }
                        fprintf(out, "    mov [rcx], rbx\n");
                    }
                    fprintf(out, "    push rbx\n");
                } else if (left->type == AST_UNARY_EXPR && left->token.type == TOKEN_CARET) {
                    compile_expr(left->children[0], out);
                    fprintf(out, "    pop rax\n    pop rbx\n    mov [rax], rbx\n    push rbx\n");
                }
                return;
            }
            compile_expr(node->children[0], out);
            for (int i = 1; i < node->child_count - 1; i += 2) {
                compile_expr(node->children[i+1], out); fprintf(out, "    pop rbx\n    pop rax\n");
                TokenType op = node->children[i]->token.type;
                if (op == TOKEN_PLUS) fprintf(out, "    add rax, rbx\n");
                else if (op == TOKEN_MINUS) fprintf(out, "    sub rax, rbx\n");
                else if (op == TOKEN_STAR) fprintf(out, "    imul rax, rbx\n");
                else if (op == TOKEN_SLASH) { fprintf(out, "    cqo\n    idiv rbx\n"); }
                else if (op == TOKEN_EQ_EQ) { fprintf(out, "    cmp rax, rbx\n    sete al\n    movzx rax, al\n"); }
                else if (op == TOKEN_BANG_EQ) { fprintf(out, "    cmp rax, rbx\n    setne al\n    movzx rax, al\n"); }
                else if (op == TOKEN_LESS || op == TOKEN_LT) { fprintf(out, "    cmp rax, rbx\n    setl al\n    movzx rax, al\n"); }
                else if (op == TOKEN_GT) { fprintf(out, "    cmp rax, rbx\n    setg al\n    movzx rax, al\n"); }
                fprintf(out, "    push rax\n");
            }
            break;
        case AST_FUNC_CALL:
            if (strncmp(node->token.start, "prms", 4) == 0) {
                if (node->child_count > 0) {
                    ASTNode* fmt_node = unwrap(node->children[0]);
                    if (fmt_node->type == AST_LITERAL && fmt_node->token.type == TOKEN_STRING_LITERAL) {
                        const char* s = fmt_node->token.start;
                        int arg_ptr = 1;
                        for (int i = 1; i < fmt_node->token.length - 1; i++) {
                            if (s[i] == '~' && i + 1 < fmt_node->token.length - 1) {
                                char spec = s[i+1]; i++;
                                if (arg_ptr < node->child_count) {
                                    compile_expr(node->children[arg_ptr++], out);
                                    fprintf(out, "    pop rdi\n");
                                    if (spec == 'i') fprintf(out, "    call _xz_print_int\n");
                                    else if (spec == 's') fprintf(out, "    call _xz_print_str\n");
                                    else if (spec == 'p') fprintf(out, "    call _xz_print_hex\n");
                                    else if (spec == 'b') fprintf(out, "    call _xz_print_bool\n");
                                }
                            } else {
                                if (s[i] == '\\' && s[i+1] == 'n') { fprintf(out, "    mov rdi, 10\n    call _xz_print_char\n"); i++; }
                                else { fprintf(out, "    mov rdi, %d\n    call _xz_print_char\n", (int)s[i]); }
                            }
                        }
                        fprintf(out, "    mov rdi, 10\n    call _xz_print_char\n");
                    }
                }
                fprintf(out, "    push 0\n");
            } else {
                for (int i=0; i<node->child_count; i++) compile_expr(node->children[i], out);
                const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
                for (int i = node->child_count - 1; i >= 0; i--) { if (i < 6) fprintf(out, "    pop %s\n", regs[i]); else fprintf(out, "    pop rax\n"); }
                fprintf(out, "    call %.*s\n    push rax\n", node->token.length, node->token.start);
            }
            break;
        default: break;
    }
}

static void compile_stmt(ASTNode* node, FILE* out) {
    if (!node) return;
    switch (node->type) {
        case AST_VAR_DECL: {
            ASTNode* val_expr = node->child_count > 0 ? node->children[0] : NULL;
            ASTNode* first = val_expr ? unwrap(val_expr) : NULL;
            int is_arr = (first && first->token.type == TOKEN_LBRACKET);
            int arr_sz = 1;
            if (is_arr) { ASTNode* sz = unwrap(first->children[0]); if (sz->type == AST_LITERAL) arr_sz = atoi(sz->token.start); }
            ASTNode* init = NULL;
            for (int i=0; i<node->child_count; i++) if (node->children[i]->type == AST_ARRAY) init = node->children[i];
            if (init) { if (init->child_count > arr_sz) arr_sz = init->child_count; for (int i = 0; i < init->child_count; i++) compile_expr(init->children[i], out); }
            stack_offset += 8 * arr_sz;
            strncpy(vars[var_count].name, node->token.start, node->token.length); vars[var_count].name[node->token.length] = '\0';
            vars[var_count].offset = stack_offset; vars[var_count].is_array = is_arr; var_count++;
            if (init) { for (int i = init->child_count - 1; i >= 0; i--) fprintf(out, "    pop rax\n    mov [rbp - %d], rax\n", stack_offset - (i * 8)); }
            else {
                if (val_expr && !is_arr) { compile_expr(val_expr, out); fprintf(out, "    pop rax\n    mov [rbp - %d], rax\n", stack_offset); }
                else if (!is_arr) fprintf(out, "    mov qword [rbp - %d], 0\n", stack_offset);
            }
            break;
        }
        case AST_FUNC_CALL: compile_expr(node, out); fprintf(out, "    pop rax\n"); break;
        case AST_BLOCK: for (int i=0; i<node->child_count; i++) compile_stmt(node->children[i], out); break;
        case AST_IF_STMT: {
            int label_id = if_label_count++; compile_expr(node->children[0], out); fprintf(out, "    pop rax\n    cmp rax, 0\n");
            if (node->child_count > 2) fprintf(out, "    je .L_else_%d\n", label_id); else fprintf(out, "    je .L_if_end_%d\n", label_id);
            compile_stmt(node->children[1], out);
            if (node->child_count > 2) { fprintf(out, "    jmp .L_if_end_%d\n.L_else_%d:\n", label_id, label_id); compile_stmt(node->children[2], out); }
            fprintf(out, ".L_if_end_%d:\n", label_id); break;
        }
        case AST_RET_STMT:
            if (node->child_count > 0) { compile_expr(node->children[0], out); fprintf(out, "    pop rax\n"); }
            else fprintf(out, "    mov rax, 0\n");
            if (is_in_start) fprintf(out, "    mov rdi, rax\n    mov rax, 60\n    syscall\n");
            else fprintf(out, "    leave\n    ret\n");
            break;
        case AST_WHILE_STMT: {
            int label_id = if_label_count++; fprintf(out, ".L_while_start_%d:\n", label_id); compile_expr(node->children[0], out);
            fprintf(out, "    pop rax\n    cmp rax, 0\n    je .L_while_end_%d\n", label_id); compile_stmt(node->children[1], out);
            fprintf(out, "    jmp .L_while_start_%d\n.L_while_end_%d:\n", label_id, label_id); break;
        }
        case AST_FOR_STMT: {
            int label_id = if_label_count++;
            if (node->child_count > 0 && node->children[0]->type != AST_UNKNOWN) compile_stmt(node->children[0], out);
            fprintf(out, ".L_for_start_%d:\n", label_id);
            if (node->child_count > 1 && node->children[1]->token.length > 0) { compile_expr(node->children[1], out); fprintf(out, "    pop rax\n    cmp rax, 0\n    je .L_for_end_%d\n", label_id); }
            compile_stmt(node->children[3], out);
            if (node->child_count > 2 && node->children[2]->token.length > 0) compile_stmt(node->children[2], out);
            fprintf(out, "    jmp .L_for_start_%d\n.L_for_end_%d:\n", label_id, label_id); break;
        }
        case AST_DROP: compile_expr(node->children[0], out); break;
        case AST_EXPR_STMT: if (node->child_count > 0) { compile_expr(node->children[0], out); fprintf(out, "    pop rax\n"); } break;
        default: break;
    }
}

void compile_to_asm(ASTNode* ast, const char* out_filename) {
    FILE* out = fopen(out_filename, "w"); string_count = 0; if_label_count = 0;
    fprintf(out, "bits 64\nglobal _start\n\nsection .text\n");
    fprintf(out, "_xz_print_char:\n    push rbp\n    mov rbp, rsp\n    sub rsp, 16\n    mov [rbp-1], dil\n    mov rax, 1\n    mov rdi, 1\n    lea rsi, [rbp-1]\n    mov rdx, 1\n    syscall\n    leave\n    ret\n\n");
    fprintf(out, "_xz_print_str:\n    push rbp\n    mov rbp, rsp\n    mov rsi, rdi\n    xor rdx, rdx\n.L_sl: cmp byte [rsi+rdx], 0\n    je .L_sw\n    inc rdx\n    jmp .L_sl\n.L_sw: mov rax, 1\n    mov rdi, 1\n    syscall\n    leave\n    ret\n\n");
    fprintf(out, "_xz_print_int:\n    push rbp\n    mov rbp, rsp\n    sub rsp, 32\n    mov rax, rdi\n    mov rcx, 10\n    lea rsi, [rbp-1]\n    mov byte [rsi], 0\n    test rax, rax\n    jns .L_it_p\n    neg rax\n    push rax\n    mov rdi, 45\n    call _xz_print_char\n    pop rax\n.L_it_p:\n.L_il: xor rdx, rdx\n    div rcx\n    add dl, 48\n    dec rsi\n    mov [rsi], dl\n    test rax, rax\n    jnz .L_il\n    mov rdi, rsi\n    call _xz_print_str\n    leave\n    ret\n\n");
    fprintf(out, "_xz_print_hex:\n    push rbp\n    mov rbp, rsp\n    sub rsp, 48\n    mov [rbp-8], rdi\n    mov rdi, 48\n    call _xz_print_char\n    mov rdi, 120\n    call _xz_print_char\n    mov rax, [rbp-8]\n    mov rcx, 16\n    lea rsi, [rbp-1]\n    mov byte [rsi], 0\n.L_hl: xor rdx, rdx\n    div rcx\n    cmp dl, 10\n    jl .L_hd\n    add dl, 39\n.L_hd: add dl, 48\n    dec rsi\n    mov [rsi], dl\n    test rax, rax\n    jnz .L_hl\n    mov rdi, rsi\n    call _xz_print_str\n    leave\n    ret\n\n");
    fprintf(out, "_xz_print_bool:\n    push rbp\n    mov rbp, rsp\n    test rdi, rdi\n    jz .L_bf\n    lea rdi, [LB_true]\n    call _xz_print_str\n    jmp .L_be\n.L_bf: lea rdi, [LB_false]\n    call _xz_print_str\n.L_be: leave\n    ret\n\n");
    fprintf(out, "_xz_alloc:\n    push rbp\n    mov rbp, rsp\n    mov [rbp-8], rdi\n    mov rax, 12\n    xor rdi, rdi\n    syscall\n    mov rbx, rax\n    add rax, [rbp-8]\n    mov rdi, rax\n    mov rax, 12\n    syscall\n    mov rax, rbx\n    leave\n    ret\n\n");

    for (int i=0; i<ast->child_count; i++) {
        ASTNode* child = ast->children[i];
        if (child->type == AST_FUNC_DECL) {
            var_count = 0; stack_offset = 0;
            is_in_start = (strncmp(child->token.start, "start", 5) == 0 && child->token.length == 5);
            if (is_in_start) fprintf(out, "_start:\n");
            else fprintf(out, "%.*s:\n", child->token.length, child->token.start);
            fprintf(out, "    push rbp\n    mov rbp, rsp\n    sub rsp, 1024\n");
            int param_idx = 0; const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
            for (int j=0; j<child->child_count; j++) {
                if (child->children[j]->type == AST_PARAM) {
                    ASTNode* p = child->children[j];
                    if (p->token.start != p->data_type.start) {
                        stack_offset += 8; strncpy(vars[var_count].name, p->token.start, p->token.length); vars[var_count].name[p->token.length] = '\0';
                        vars[var_count].offset = stack_offset; vars[var_count].is_array = 0; var_count++;
                        if (param_idx < 6) fprintf(out, "    mov [rbp - %d], %s\n", stack_offset, regs[param_idx]);
                        else fprintf(out, "    mov rax, [rbp + 16 + (param_idx - 6) * 8]\n    mov [rbp - %d], rax\n", stack_offset);
                    }
                    param_idx++;
                }
            }
            for (int j=0; j<child->child_count; j++) if (child->children[j]->type == AST_BLOCK) compile_stmt(child->children[j], out);
            if (is_in_start) fprintf(out, "    mov rax, 60\n    xor rdi, rdi\n    syscall\n\n");
            else fprintf(out, "    mov rax, 0\n    leave\n    ret\n\n");
        }
    }
    fprintf(out, "\nsection .data\nLB_true: db \"true\", 0\nLB_false: db \"false\", 0\n");
    for (int i=0; i<string_count; i++) {
        fprintf(out, "LC%d: db ", i);
        if (strlen(strings[i]) == 0) fprintf(out, "0\n");
        else {
            for (int j=0; strings[i][j]; j++) fprintf(out, "%d, ", (unsigned char)strings[i][j]);
            fprintf(out, "0\n");
        }
    }
    fclose(out);
}
