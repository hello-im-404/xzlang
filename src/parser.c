#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool is_valid_type(Token tok) {
    if (tok.type == TOKEN_PTR) return true;
    if (tok.length == 3) {
        if (strncmp(tok.start, "i64", 3) == 0) return true;
        if (strncmp(tok.start, "i32", 3) == 0) return true;
        if (strncmp(tok.start, "i16", 3) == 0) return true;
        if (strncmp(tok.start, "str", 3) == 0) return true;
        if (strncmp(tok.start, "chr", 3) == 0) return true;
        if (strncmp(tok.start, "f64", 3) == 0) return true;
        if (strncmp(tok.start, "f32", 3) == 0) return true;
        if (strncmp(tok.start, "ntg", 3) == 0) return true;
        if (strncmp(tok.start, "ptr", 3) == 0) return true;
    } else if (tok.length == 4) {
        if (strncmp(tok.start, "bool", 4) == 0) return true;
    }
    return false;
}

static void advance(Parser* parser) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = lexer_next_token(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;
        fprintf(stderr, "[line %d] Lexer error: %.*s\n", parser->current.line, parser->current.length, parser->current.start);
        parser->had_error = 1;
    }
}

static void error_at_current(Parser* parser, const char* message) {
    if (parser->panic_mode) return;
    parser->panic_mode = 1;
    fprintf(stderr, "[line %d] Error at '%.*s' (type %d): %s\n", parser->current.line, parser->current.length, parser->current.start, parser->current.type, message);
    parser->had_error = 1;
}

static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) { advance(parser); return; }
    error_at_current(parser, message);
}

static int match(Parser* parser, TokenType type) {
    if (parser->current.type == type) { advance(parser); return 1; }
    return 0;
}

void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->had_error = 0;
    parser->panic_mode = 0;
    advance(parser);
}

// Forward declarations
static ASTNode* declaration(Parser* parser);
static ASTNode* statement(Parser* parser);
static ASTNode* expression(Parser* parser);

static ASTNode* expression(Parser* parser) {
    ASTNode* expr = ast_create_node(AST_UNKNOWN, parser->current);
    int paren_depth = 0;
    int bracket_depth = 0;
    
    while (parser->current.type != TOKEN_EOF) {
        if (paren_depth <= 0 && bracket_depth <= 0) {
            if (parser->current.type == TOKEN_DOT || parser->current.type == TOKEN_RPAREN ||
                parser->current.type == TOKEN_RBRACE || parser->current.type == TOKEN_RBRACKET ||
                parser->current.type == TOKEN_COMMA) break;
        }
        
        if (parser->current.type == TOKEN_LPAREN) paren_depth++;
        else if (parser->current.type == TOKEN_RPAREN) paren_depth--;
        else if (parser->current.type == TOKEN_LBRACKET) bracket_depth++;
        else if (parser->current.type == TOKEN_RBRACKET) bracket_depth--;
        
        Token t = parser->current;
        if (t.type == TOKEN_TILDE || t.type == TOKEN_CARET) {
            advance(parser); ASTNode* un = ast_create_node(AST_UNARY_EXPR, t);
            ast_add_child(un, expression(parser)); ast_add_child(expr, un);
            continue;
        } else if (t.type == TOKEN_PLUS_PLUS || t.type == TOKEN_MINUS_MINUS) {
            if (expr->child_count > 0 && (expr->children[expr->child_count-1]->type == AST_VARIABLE || expr->children[expr->child_count-1]->type == AST_ARRAY_ACCESS)) {
                ASTNode* v = expr->children[expr->child_count-1];
                ASTNode* un = ast_create_node(AST_UNARY_EXPR, t); un->is_postfix = 1; ast_add_child(un, v);
                expr->children[expr->child_count-1] = un; advance(parser); continue;
            } else {
                advance(parser); ASTNode* un = ast_create_node(AST_UNARY_EXPR, t);
                ast_add_child(un, expression(parser)); ast_add_child(expr, un); continue;
            }
        } else if (t.type == TOKEN_GAIN) {
            advance(parser); if (match(parser, TOKEN_COLON)) {
                ASTNode* g = ast_create_node(AST_GAIN, t);
                if (is_valid_type(parser->current)) { g->data_type = parser->current; advance(parser);
                    if (match(parser, TOKEN_LBRACKET)) { ast_add_child(g, expression(parser)); consume(parser, TOKEN_RBRACKET, "Expected ']'."); }
                    else { ast_add_child(g, ast_create_node(AST_LITERAL, (Token){.type=TOKEN_INT_LITERAL, .start="1", .length=1})); }
                }
                ast_add_child(expr, g); continue;
            }
        } else if (t.type == TOKEN_SYSCALL) {
            advance(parser); if (match(parser, TOKEN_LBRACE)) {
                ASTNode* sys = ast_create_node(AST_SYSCALL, t);
                while (parser->current.type != TOKEN_RBRACE && parser->current.type != TOKEN_EOF) {
                    ast_add_child(sys, expression(parser));
                    if (parser->current.type == TOKEN_COMMA) advance(parser);
                }
                consume(parser, TOKEN_RBRACE, "Expected '}'.");
                ast_add_child(expr, sys); continue;
            }
        } else if (t.type == TOKEN_ADDR) {
            advance(parser); if (match(parser, TOKEN_COLON)) {
                ASTNode* un = ast_create_node(AST_UNARY_EXPR, t); ast_add_child(un, expression(parser));
                ast_add_child(expr, un); continue;
            }
            ast_add_child(expr, ast_create_node(AST_VARIABLE, t)); continue;
        } else if (t.type == TOKEN_PLUS || t.type == TOKEN_MINUS || t.type == TOKEN_STAR || t.type == TOKEN_SLASH ||
                   t.type == TOKEN_EQ_EQ || t.type == TOKEN_BANG_EQ || t.type == TOKEN_LESS || t.type == TOKEN_LESS_EQ ||
                   t.type == TOKEN_GREATER || t.type == TOKEN_GREATER_EQ || t.type == TOKEN_EQUALS || t.type == TOKEN_ARROW || t.type == TOKEN_PIPE) {
            ast_add_child(expr, ast_create_node(AST_BINARY_EXPR, t));
        } else if (t.type == TOKEN_IDENTIFIER) {
            const char* p = t.start + t.length;
            while (*p == ' ' || *p == '\r' || *p == '\t' || *p == '\n' || *p == ';') { if (*p == ';') while (*p != '\n' && *p != '\0') p++; else p++; }
            if (*p == '{') {
                ASTNode* call = ast_create_node(AST_FUNC_CALL, t); advance(parser); advance(parser);
                while (parser->current.type != TOKEN_RBRACE && parser->current.type != TOKEN_EOF) {
                    ast_add_child(call, expression(parser)); if (parser->current.type == TOKEN_COMMA) advance(parser);
                }
                consume(parser, TOKEN_RBRACE, "Expected '}'."); ast_add_child(expr, call); continue;
            } else if (*p == '[') {
                ASTNode* acc = ast_create_node(AST_ARRAY_ACCESS, t); advance(parser); advance(parser);
                ast_add_child(acc, expression(parser)); consume(parser, TOKEN_RBRACKET, "Expected ']'.");
                ast_add_child(expr, acc); continue;
            } else { ast_add_child(expr, ast_create_node(AST_VARIABLE, t)); }
        } else if (t.type == TOKEN_LBRACKET) {
            ASTNode* lit = ast_create_node(AST_ARRAY, t); advance(parser);
            while (parser->current.type != TOKEN_RBRACKET && parser->current.type != TOKEN_EOF) {
                ast_add_child(lit, expression(parser)); if (parser->current.type == TOKEN_COMMA) advance(parser);
            }
            consume(parser, TOKEN_RBRACKET, "Expected ']'."); ast_add_child(expr, lit); continue;
        } else if (t.type == TOKEN_INT_LITERAL || t.type == TOKEN_FLOAT_LITERAL || t.type == TOKEN_STRING_LITERAL) {
            ast_add_child(expr, ast_create_node(AST_LITERAL, t));
        }
        advance(parser);
    }
    return expr;
}

static ASTNode* block(Parser* parser) {
    ASTNode* blk = ast_create_node(AST_BLOCK, (Token){0});
    while (parser->current.type != TOKEN_RPAREN && parser->current.type != TOKEN_EOF) {
        ASTNode* decl = declaration(parser); if (decl) ast_add_child(blk, decl);
    }
    return blk;
}

static ASTNode* statement(Parser* parser) {
    if (match(parser, TOKEN_LPAREN)) {
        Token t = parser->current; consume(parser, TOKEN_IDENTIFIER, "Expected type."); consume(parser, TOKEN_RPAREN, "Expected ')'."); consume(parser, TOKEN_COLON, "Expected ':'.");
        ASTNode* c = ast_create_node(AST_EXPR_STMT, t); ast_add_child(c, expression(parser)); if (parser->current.type == TOKEN_DOT) advance(parser); return c;
    }
    Token id = parser->current;
    if (id.type == TOKEN_IDENTIFIER || id.type == TOKEN_ADDR || id.type == TOKEN_DROP || id.type == TOKEN_TILDE || id.type == TOKEN_CARET || id.type == TOKEN_SYSCALL) {
        if (id.type == TOKEN_SYSCALL) {
            ASTNode* es = ast_create_node(AST_EXPR_STMT, (Token){0});
            ast_add_child(es, expression(parser));
            if (parser->current.type == TOKEN_DOT) advance(parser);
            return es;
        }
        if (id.length == 5 && strncmp(id.start, "using", 5) == 0) {
            advance(parser); ASTNode* u = ast_create_node(AST_USING, id);
            if (match(parser, TOKEN_LBRACE) || match(parser, TOKEN_LPAREN)) {
                while (parser->current.type != TOKEN_RBRACE && parser->current.type != TOKEN_RPAREN && parser->current.type != TOKEN_EOF) {
                    if (parser->current.type == TOKEN_STRING_LITERAL) { ast_add_child(u, ast_create_node(AST_LITERAL, parser->current)); advance(parser); }
                    if (parser->current.type == TOKEN_COMMA) advance(parser);
                }
                advance(parser);
            }
            consume(parser, TOKEN_DOT, "Expected '.'."); return u;
        }
        if (id.length == 3 && strncmp(id.start, "ret", 3) == 0) {
            advance(parser); ASTNode* r = ast_create_node(AST_RET_STMT, id);
            if (parser->current.type != TOKEN_DOT) ast_add_child(r, expression(parser));
            consume(parser, TOKEN_DOT, "Expected '.'."); return r;
        }
        if (id.length == 5 && strncmp(id.start, "while", 5) == 0) {
            advance(parser); ASTNode* w = ast_create_node(AST_WHILE_STMT, id);
            if (match(parser, TOKEN_LBRACE)) { ast_add_child(w, expression(parser)); consume(parser, TOKEN_RBRACE, "Expected '}'."); }
            consume(parser, TOKEN_LPAREN, "Expected '('."); ast_add_child(w, block(parser)); consume(parser, TOKEN_RPAREN, "Expected ')'."); return w;
        }
        if (id.length == 3 && strncmp(id.start, "for", 3) == 0) {
            advance(parser); ASTNode* f = ast_create_node(AST_FOR_STMT, id);
            if (match(parser, TOKEN_LBRACE)) {
                if (parser->current.type != TOKEN_COMMA) ast_add_child(f, declaration(parser)); else { ast_add_child(f, ast_create_node(AST_UNKNOWN, (Token){0})); advance(parser); }
                if (parser->current.type != TOKEN_COMMA && parser->current.type != TOKEN_RBRACE) ast_add_child(f, expression(parser)); else ast_add_child(f, ast_create_node(AST_UNKNOWN, (Token){0}));
                if (match(parser, TOKEN_COMMA)) { if (parser->current.type != TOKEN_RBRACE) ast_add_child(f, expression(parser)); else ast_add_child(f, ast_create_node(AST_UNKNOWN, (Token){0})); }
                consume(parser, TOKEN_RBRACE, "Expected '}'.");
            }
            consume(parser, TOKEN_LPAREN, "Expected '('."); ast_add_child(f, block(parser)); consume(parser, TOKEN_RPAREN, "Expected ')'."); return f;
        }
        if (id.length == 2 && strncmp(id.start, "if", 2) == 0) {
            advance(parser); ASTNode* i = ast_create_node(AST_IF_STMT, id);
            if (match(parser, TOKEN_LBRACE)) { ast_add_child(i, expression(parser)); consume(parser, TOKEN_RBRACE, "Expected '}'."); }
            consume(parser, TOKEN_LPAREN, "Expected '('."); ast_add_child(i, block(parser));
            if (match(parser, TOKEN_RPAREN)) {
                if (parser->current.type == TOKEN_IDENTIFIER && strncmp(parser->current.start, "else", 4) == 0) {
                    advance(parser); if (parser->current.type == TOKEN_IDENTIFIER && strncmp(parser->current.start, "if", 2) == 0) ast_add_child(i, statement(parser));
                    else if (match(parser, TOKEN_LPAREN)) { ast_add_child(i, block(parser)); consume(parser, TOKEN_RPAREN, "Expected ')'."); }
                }
            }
            return i;
        }
        if (id.type == TOKEN_DROP) {
            advance(parser); if (match(parser, TOKEN_COLON)) {
                ASTNode* d = ast_create_node(AST_DROP, id); ast_add_child(d, expression(parser));
                consume(parser, TOKEN_DOT, "Expected '.'."); return d;
            }
        }
        ASTNode* es = ast_create_node(AST_EXPR_STMT, (Token){0}); ast_add_child(es, expression(parser));
        if (parser->current.type == TOKEN_DOT) advance(parser); return es;
    }
    error_at_current(parser, "Expected statement."); advance(parser); return NULL;
}

static ASTNode* declaration(Parser* parser) {
    if (parser->current.type == TOKEN_IDENTIFIER || parser->current.type == TOKEN_PTR) {
        Token type_tok = parser->current;
        bool kw = false;
        if (type_tok.length == 2 && strncmp(type_tok.start, "if", 2) == 0) kw = true;
        else if (type_tok.length == 5 && strncmp(type_tok.start, "while", 5) == 0) kw = true;
        else if (type_tok.length == 3 && strncmp(type_tok.start, "for", 3) == 0) kw = true;
        else if (type_tok.length == 3 && strncmp(type_tok.start, "ret", 3) == 0) kw = true;
        else if (type_tok.length == 5 && strncmp(type_tok.start, "using", 5) == 0) kw = true;
        else if (type_tok.length == 4 && strncmp(type_tok.start, "prms", 4) == 0) kw = true;
        else if (type_tok.type == TOKEN_DROP) kw = true;
        if (kw) return statement(parser);

        const char* p = type_tok.start + type_tok.length;
        while (*p == ' ' || *p == '\r' || *p == '\t' || *p == '\n' || *p == ';') { if (*p == ';') while (*p != '\n' && *p != '\0') p++; else p++; }
        if (*p == '{' || *p == '[' || *p == '.' || *p == '+' || *p == '-' || *p == '=' || *p == '!' || *p == '^' || *p == '*' || *p == '/') return statement(parser);

        if (type_tok.type == TOKEN_PTR) {
            advance(parser); if (match(parser, TOKEN_LT)) { if (is_valid_type(parser->current)) advance(parser); consume(parser, TOKEN_GT, "Expected '>'."); }
        } else advance(parser);
        
        if (match(parser, TOKEN_COLON)) {
            Token name = parser->current; consume(parser, TOKEN_IDENTIFIER, "Expected name.");
            ASTNode* vd = ast_create_node(AST_VAR_DECL, name); vd->data_type = type_tok;
            if (match(parser, TOKEN_LBRACKET)) {
                ASTNode* un = ast_create_node(AST_UNKNOWN, (Token){.type=TOKEN_LBRACKET});
                ast_add_child(un, expression(parser)); ast_add_child(vd, un);
                consume(parser, TOKEN_RBRACKET, "Expected ']'.");
            }
            if (match(parser, TOKEN_EQUALS)) ast_add_child(vd, expression(parser));
            consume(parser, TOKEN_DOT, "Expected '.'."); return vd;
        } else if (parser->current.type == TOKEN_IDENTIFIER) {
            Token name = parser->current; advance(parser);
            ASTNode* fd = ast_create_node(AST_FUNC_DECL, name); fd->data_type = type_tok;
            if (match(parser, TOKEN_LBRACE)) {
                while (parser->current.type != TOKEN_RBRACE && parser->current.type != TOKEN_EOF) {
                    if (parser->current.type != TOKEN_COMMA && parser->current.type != TOKEN_RBRACE) {
                        Token pt = parser->current; advance(parser);
                        if (pt.type == TOKEN_PTR) {
                            if (match(parser, TOKEN_LT)) {
                                if (is_valid_type(parser->current)) advance(parser);
                                consume(parser, TOKEN_GT, "Expected '>'.");
                            }
                        }
                        if (match(parser, TOKEN_COLON)) { ast_add_child(fd, ast_create_node(AST_PARAM, parser->current)); fd->children[fd->child_count-1]->data_type = pt; advance(parser); }
                        else { ast_add_child(fd, ast_create_node(AST_PARAM, pt)); fd->children[fd->child_count-1]->data_type = pt; }
                    }
                    if (parser->current.type == TOKEN_COMMA) advance(parser);
                }
                consume(parser, TOKEN_RBRACE, "Expected '}'.");
            }
            if (match(parser, TOKEN_LPAREN)) { ast_add_child(fd, block(parser)); consume(parser, TOKEN_RPAREN, "Expected ')'."); }
            return fd;
        }
    }
    return statement(parser);
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* prog = ast_create_node(AST_PROGRAM, (Token){0});
    while (parser->current.type != TOKEN_EOF) {
        ASTNode* d = declaration(parser); if (d) ast_add_child(prog, d);
    }
    return prog;
}
