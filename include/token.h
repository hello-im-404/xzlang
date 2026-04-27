#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_PLUS,      // +
    TOKEN_PLUS_PLUS, // ++
    TOKEN_MINUS,     // -
    TOKEN_MINUS_MINUS, // --
    TOKEN_STAR,      // *
    TOKEN_SLASH,     // /
    TOKEN_DOT,       // .
    TOKEN_COMMA,     // ,
    TOKEN_COLON,     // :
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_EQUALS,    // =
    TOKEN_EQ_EQ,     // ==
    TOKEN_BANG_EQ,   // !=
    TOKEN_LESS,      // <
    TOKEN_LESS_EQ,   // <=
    TOKEN_GREATER,   // >
    TOKEN_GREATER_EQ,// >=
    TOKEN_ARROW,     // =>
    TOKEN_PIPE,      // |
    TOKEN_CARET,     // ^
    TOKEN_ADDR,      // addr
    TOKEN_PTR,       // ptr
    TOKEN_GAIN,      // gain
    TOKEN_DROP,      // drop
    TOKEN_SYSCALL,   // syscall
    TOKEN_LT,        // <
    TOKEN_GT,        // >
    TOKEN_TILDE,     // ~
    TOKEN_EOF,       // End of file
    TOKEN_ERROR      // Lexer error
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

#endif 
