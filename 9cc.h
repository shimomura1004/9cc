#ifndef __9CC_H__
#define __9CC_H__

#include <stdbool.h>

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

//
// Tokenizer
//

// トークナイザは、記号か数字か、くらいの区分しかしない
// 記号の種類を見るのはパーサ
typedef enum {
    TK_RESERVED,    // 記号
    TK_IDENT,       // 識別子
    TK_NUM,         // 整数トークン
    TK_EOF,         // 入力の終わり
} TokenKind;

typedef struct Token Token;

struct Token {
    TokenKind kind;     // トークンの型
    Token *next;        // 次の入力トークン
    int val;            // kind が TK_NUM の場合はその数値
    char *str;          // トークンの文字列
    int len;
};

// 入力プログラム
extern char *user_input;

// 現在見ているトークン
extern Token *token;

bool at_eof();
bool consume(char *op);
Token *consume_ident();
void expect(char *op);
long int expect_number();
Token *tokenize();

//
// Parser
//

// AST のノードの種類
typedef enum {
    ND_ADD,     // +
    ND_SUB,     // -
    ND_MUL,     // *
    ND_DIV,     // /
    ND_ASSIGN,  // =
    ND_LVAR,    // ローカル変数
    ND_EQ,      // ==
    ND_NE,      // !=
    ND_LT,      // <
    ND_LE,      // <=
    ND_NUM,     // 整数
} NodeKind;

typedef struct Node Node;

// AST のノードの型
struct Node {
    NodeKind kind;  // ノードの型
    Node *lhs;      // 左辺
    Node *rhs;      // 右辺
    int val;        // kind が ND_NUM の場合のみ使う
    int offset;     // kind が ND_LVAR の場合のみ使う
};

Node *program();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// 各文の ast を入れる
extern Node *code[100];

//
// Code generator
//

void gen(Node *node);

#endif
