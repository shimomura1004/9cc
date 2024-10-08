#ifndef __9CC_H__
#define __9CC_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>

typedef struct Type Type;
typedef struct Member Member;
typedef struct Initializer Initializer;

//
// Tokenizer
//

// トークナイザは、記号か数字か、くらいの区分しかしない
// 記号の種類を見るのはパーサ
typedef enum {
    TK_RESERVED,    // 記号
    TK_IDENT,       // 識別子
    TK_STR,         // 文字列リテラル
    TK_NUM,         // 整数トークン
    TK_EOF,         // 入力の終わり
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;     // トークンの型
    Token *next;        // 次の入力トークン
    long val;            // kind が TK_NUM の場合はその数値
    char *str;          // トークンの文字列
    int len;            // 文字列長

    char *contents;     // トークンが文字列リテラルの場合、その文字列(NULL 文字を含む)
    char cont_len;      // トークンが文字列リテラルの場合、その文字列の長さ
};

// 変数の型
typedef struct Var Var;
struct Var {
    char *name;     // 変数名
    Type *ty;       // 型
    Token *tok;     // エラーメッセージ用
    bool is_local;  // ローカル変数かグローバル変数か

    // ローカル変数用
    int offset;     // RBP からのオフセット(ローカル変数のときのみ使用)

    // グローバル変数用
    Initializer *initializer;
};

// 変数のリスト
typedef struct VarList VarList;
struct VarList {
    VarList *next;
    Var *var;
};

extern char *filename;      // ソースコードのファイル名
extern char *user_input;    // 入力プログラム
extern Token *token;        // 現在見ているトークン

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);

bool at_eof();
Token *peek(char *s);
Token *consume(char *s);
Token *consume_ident();
void expect(char *s);
long expect_number();
char *expect_ident();
char *strndup(char *p, int len);
Token *tokenize();

//
// Parser
//

// AST のノードの種類
typedef enum {
    ND_ADD = 0,     // +
    ND_SUB,         // -
    ND_MUL,         // *
    ND_DIV,         // /
    ND_BITAND,      // &
    ND_BITOR,       // |
    ND_BITXOR,      // ^
    ND_SHL,         // <<
    ND_SHR,         // >>
    ND_ASSIGN,      // =
    ND_TERNARY,     // ?:
    ND_PRE_INC,     // pre ++
    ND_PRE_DEC,     // pre --
    ND_POST_INC,    // post ++
    ND_POST_DEC,    // post --
    ND_A_ADD,       // +=
    ND_A_SUB,       // -=
    ND_A_MUL,       // *=
    ND_A_DIV,       // /=
    ND_A_SHL,       // <<=
    ND_A_SHR,       // >>=
    ND_COMMA,       // ,
    ND_MEMBER,      // . 構造体のメンバアクセス
    ND_ADDR,        // 単項 &
    ND_DEREF,       // 単項 *
    ND_NOT,         // !
    ND_BITNOT,      // ~
    ND_LOGAND,      // &&
    ND_LOGOR,       // ||
    ND_VAR,         // 変数
    ND_EQ,          // ==
    ND_NE,          // !=
    ND_LT,          // <
    ND_LE,          // <=
    ND_RETURN,      // "return"
    ND_IF,          // "if"
    ND_WHILE,       // "while"
    ND_FOR,         // "for"
    ND_SWITCH,      // "switch"
    ND_CASE,        // "case"
    ND_SIZEOF,      // "sizeof"
    ND_BLOCK,       // "{ ... }"
    ND_BREAK,       // "break"
    ND_CONTINUE,    // "continue"
    ND_GOTO,        // "goto"
    ND_LABEL,       // Labeled statement
    ND_FUNCALL,     // 関数呼び出し
    ND_EXPR_STMT,   // Expression のみの文
    ND_STMT_EXPR,   // ({ stmt+ }) 文を複数並べて最後の文が値になる (GNU C 拡張)
    ND_NUM,         // 整数リテラル
    ND_CAST,        // キャスト
    ND_NULL,        // 空の文
} NodeKind;

// AST のノードの型
typedef struct Node Node;
struct Node {
    NodeKind kind;      // ノードの型
    Node *next;         // 次のノード
    Type *ty;           // 型情報 (int か *int)
    Token *tok;         // エラーメッセージ用に対応するトークンを保持

    Node *lhs;          // 左辺
    Node *rhs;          // 右辺

    Node *cond;         // 条件文
    Node *then;         // 真の場合のコード、もしくは for/while のループ本体のコード
    Node *els;          // 偽の場合のコード
    Node *init;         // for の初期化部のコード
    Node *inc;          // for のインクリメント部のコード

    Node *body;         // ブロックもしくはステートメント式の中身の複数の文のコード

    char *member_name;  // 構造体メンバへのアクセス時に使う
    Member *member;     // 構造体のメンバ一覧 (構造体定義のときに使う)

    char *funcname;     // 関数呼び出し
    Node *args;         // 関数引数

    char *label_name;   // goto 文もしくはラベルで使う

    Node *case_next;
    Node *default_case;
    int case_label;
    int case_end_label;

    Var *var;           // kind が ND_VAR の場合のみ使う
    long val;           // kind が ND_NUM の場合のみ使う
};

// グローバル変数の初期化子を保持する構造体
// グローバル変数は定数式かほかのグローバル変数へのポインタで初期化される
struct Initializer {
    Initializer *next;

    // 定数式用
    int sz;
    long val;

    // 他のグローバル変数へのポインタ用
    char *label;
};

// 関数の情報を保持する構造体
typedef struct Function Function;
struct Function {
    Function *next;     // 次の関数定義
    char *name;         // 定義した関数の名前
    VarList *params;    // 引数のリスト

    Node *node;         // 関数の中身
    VarList *locals;    // 関数が使うローカル変数のリスト
    int stack_size;     // 関数が使うスタックのサイズ
};

// プログラムの情報を保持する構造体
// 関数とグローバル変数のリストからなる
typedef struct {
    VarList *globals;
    Function *fns;
} Program;

Program *program();

//
// Type
//

typedef enum {
    TY_VOID,
    TY_BOOL,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_ENUM,
    TY_PTR,
    TY_ARRAY,
    TY_STRUCT,
    TY_FUNC,
} TypeKind;

struct Type {
    TypeKind kind;
    bool is_typedef;
    bool is_static;
    bool is_incomplete; // incomplete array
    int align;
    Type *base;         // ポインタか配列型のとき、ベースとなる型が入る
    int array_size;
    Member *members;    // 構造体のメンバ
    Type *return_ty;    // 関数の戻り値型
};

struct Member {
    Member *next;
    Type *ty;
    Token *tok;
    char *name;
    int offset;
};

int align_to(int n, int align);
Type *void_type();
Type *bool_type();
Type *char_type();
Type *short_type();
Type *int_type();
Type *long_type();
Type *enum_type();
Type *struct_type();
Type *func_type(Type *return_ty);
Type *pointer_to(Type *base);
Type *array_of(Type *base, int size);
int size_of(Type *ty, Token* tok);

void add_type(Program *prog);

//
// Code generator
//

void codegen(Program *prog);

#endif
