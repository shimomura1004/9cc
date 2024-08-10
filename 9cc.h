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

// ローカル変数の型
typedef struct Var Var;
struct Var {
    Var *next;
    char *name;
    int offset;
};

extern char *user_input;    // 入力プログラム
extern Token *token;        // 現在見ているトークン

bool at_eof();
bool consume(char *op);
Token *consume_ident();
void expect(char *op);
long int expect_number();
char *strndup(char *p, int len);
Token *tokenize();

//
// Parser
//

// AST のノードの種類
typedef enum {
    ND_ADD,         // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_ASSIGN,      // =
    ND_VAR,         // 変数
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_RETURN,      // "return"
    ND_IF,          // "if"
    ND_EXPR_STMT,   // Expression のみの文
    ND_NUM,         // 整数リテラル
} NodeKind;

// AST のノードの型
typedef struct Node Node;
struct Node {
    NodeKind kind;  // ノードの型
    Node *next;     // 次のノード

    Node *lhs;      // 左辺
    Node *rhs;      // 右辺

    Node *cond;     // if の条件文
    Node *then;     // 真の場合のコード
    Node *els;      // 偽の場合のコード

    Var *var;       // kind が ND_VAR の場合のみ使う
    int val;        // kind が ND_NUM の場合のみ使う
};

// プログラム全体の情報を保持する構造体
typedef struct {
    Node *node;
    Var *locals;
    int stack_size;
} Program;

Program *program();

//
// Code generator
//

void codegen(Program *prog);

#endif
