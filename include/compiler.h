#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"

void compile_to_asm(ASTNode* ast, const char* out_filename);

#endif 
