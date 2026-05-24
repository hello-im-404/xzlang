#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

void lexer_init(Lexer* lexer, const char* source) {
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
}

static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char advance(Lexer* lexer) {
    lexer->current++;
    return lexer->current[-1];
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer->current++;
    return true;
}

static Token make_token(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    return token;
}

static Token error_token(Lexer* lexer, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    return token;
}

static void skip_whitespace(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(lexer);
                break;
            case '\n':
                lexer->line++;
                advance(lexer);
                break;
            case ';':
                while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                    advance(lexer);
                }
                break;
            default:
                return;
        }
    }
}

static Token string(Lexer* lexer) {
    while (peek(lexer) != '"' && !is_at_end(lexer)) {
        if (peek(lexer) == '\n') lexer->line++;
        advance(lexer);
    }

    if (is_at_end(lexer)) return error_token(lexer, "Unterminated string.");

    advance(lexer);
    return make_token(lexer, TOKEN_STRING_LITERAL);
}

static Token number(Lexer* lexer) {
    while (isdigit(peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        advance(lexer);

        while (isdigit(peek(lexer))) advance(lexer);
        return make_token(lexer, TOKEN_FLOAT_LITERAL);
    }

    return make_token(lexer, TOKEN_INT_LITERAL);
}

static Token identifier(Lexer* lexer) {
    while (isalpha(peek(lexer)) || isdigit(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    int len = (int)(lexer->current - lexer->start);
    if (len == 4 && strncmp(lexer->start, "addr", 4) == 0) return make_token(lexer, TOKEN_ADDR);
    if (len == 3 && strncmp(lexer->start, "ptr", 3) == 0) return make_token(lexer, TOKEN_PTR);
    if (len == 4 && strncmp(lexer->start, "gain", 4) == 0) return make_token(lexer, TOKEN_GAIN);
    if (len == 4 && strncmp(lexer->start, "drop", 4) == 0) return make_token(lexer, TOKEN_DROP);
    if (len == 7 && strncmp(lexer->start, "syscall", 7) == 0) return make_token(lexer, TOKEN_SYSCALL);
    
    return make_token(lexer, TOKEN_IDENTIFIER);
}

Token lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);
    lexer->start = lexer->current;

    if (is_at_end(lexer)) return make_token(lexer, TOKEN_EOF);

    char c = advance(lexer);

    if (isalpha(c) || c == '_') return identifier(lexer);
    if (isdigit(c)) return number(lexer);

    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case '[': return make_token(lexer, TOKEN_LBRACKET);
        case ']': return make_token(lexer, TOKEN_RBRACKET);
        case '+':
            if (match(lexer, '+')) return make_token(lexer, TOKEN_PLUS_PLUS);
            return make_token(lexer, TOKEN_PLUS);
        case '-':
            if (match(lexer, '-')) return make_token(lexer, TOKEN_MINUS_MINUS);
            return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/':
            if (match(lexer, '/')) {
                while (peek(lexer) != '\n' && !is_at_end(lexer)) advance(lexer);
                return lexer_next_token(lexer);
            }
            return make_token(lexer, TOKEN_SLASH);
        case '.': return make_token(lexer, TOKEN_DOT);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case ':': return make_token(lexer, TOKEN_COLON);
        case '~': return make_token(lexer, TOKEN_TILDE);
        case '=':
            if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW);
            if (match(lexer, '=')) return make_token(lexer, TOKEN_EQ_EQ);
            return make_token(lexer, TOKEN_EQUALS);
        case '|': return make_token(lexer, TOKEN_PIPE);
        case '^': return make_token(lexer, TOKEN_CARET);
        case '<':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_LESS_EQ);
            return make_token(lexer, TOKEN_LT);
        case '>':
            if (match(lexer, '=')) return make_token(lexer, TOKEN_GREATER_EQ);
            return make_token(lexer, TOKEN_GT);
        case '!':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_BANG_EQ);
            }
            return error_token(lexer, "Unexpected character '!'.");
        case '"': return string(lexer);
    }

    return error_token(lexer, "Unexpected character.");
}
