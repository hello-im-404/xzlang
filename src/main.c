#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "eval.h"
#include "compiler.h"

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

void load_libraries(ASTNode* ast) {
    for (int i = 0; i < ast->child_count; i++) {
        if (ast->children[i]->type == AST_USING) {
            ASTNode* u = ast->children[i];
            for (int j = 0; j < u->child_count; j++) {
                char path[256];
                const char* lib_name = u->children[j]->token.start + 1;
                int len = u->children[j]->token.length - 2;
                snprintf(path, sizeof(path), "lib/%.*s.xzl", len, lib_name);
                
                char* src = read_file(path);
                if (src) {
                    Lexer lib_lexer;
                    lexer_init(&lib_lexer, src);
                    Parser lib_parser;
                    parser_init(&lib_parser, &lib_lexer);
                    ASTNode* lib_ast = parser_parse(&lib_parser);
                    for (int k = 0; k < lib_ast->child_count; k++) {
                        ast_add_child(ast, lib_ast->children[k]);
                    }
                } else {
                    fprintf(stderr, "Warning: Could not find library '%s' at %s\n", lib_name, path);
                }
            }
        }
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: xzlang [run|build] [path.xzl] [-o output_name]\n");
        exit(64);
    }

    const char* command = argv[1];
    const char* path = argv[2];
    const char* output_name = "app";

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        }
    }

    char* source = read_file(path);
    if (!source) { fprintf(stderr, "Error: Could not read file %s\n", path); exit(1); }

    Lexer lexer;
    lexer_init(&lexer, source);
    Parser parser;
    parser_init(&parser, &lexer);
    ASTNode* ast = parser_parse(&parser);
    if (parser.had_error) exit(1);

    load_libraries(ast);

    if (strcmp(command, "run") == 0) {
        printf("\n--- Execution Output ---\n");
        Environment env; env_init(&env); eval_init();
        eval(ast, &env);
        env_free(&env);
        printf("------------------------\n");
    } else if (strcmp(command, "build") == 0) {
        compile_to_asm(ast, "xz_temp.asm");
        char build_cmd[512];
        snprintf(build_cmd, sizeof(build_cmd), "nasm -f elf64 xz_temp.asm -o xz_temp.o && ld xz_temp.o -o %s && strip %s", output_name, output_name);
        int res = system(build_cmd);
        if (res == 0) {
            system("rm xz_temp.asm xz_temp.o");
            printf("Successfully built native binary '%s' using NASM/LD! Run with ./%s\n", output_name, output_name);
        } else {
            printf("Build failed during NASM/LD compilation.\n");
        }
    } else {
        fprintf(stderr, "Unknown command: %s. Use 'run' or 'build'.\n", command);
    }

    ast_free(ast);
    free(source);
    return 0;
}
