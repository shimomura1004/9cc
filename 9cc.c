#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TK_RESERVED,    // 記号
    TK_NUM,         // 整数トークン
    TK_EOF,         // 入力の終わり
} TokenKind;

typedef struct Token Token;

struct Token {
    TokenKind kind;     // トークンの型
    Token *next;        // 次の入力トークン
    int val;            // kind が TK_NUM の場合はその数値
    char *str;          // トークンの文字列
};

// 入力プログラム
char *user_input;

// 現在見ているトークン
Token *token;

// エラーを出力し終了する
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");       // pos個の空白を出力
    fprintf(stderr, "^ ");
    fprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 次のトークンが期待している記号と同じであればトークンを1つ読み進め真を返す
// それ以外の場合は偽を返す
bool consume(char op) {
    if (token->kind != TK_RESERVED || token->str[0] !=  op) {
        return false;
    }
    token = token->next;
    return true;
}

// 次のトークンが期待している記号と同じであればトークンを1つ読み進める
// それ以外の場合はエラーを報告する
void expect(char op) {
    if (token->kind != TK_RESERVED || token->str[0] != op) {
        error_at(token->str, "not '%c'", op);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンをひとつ読み進めてその数値を返す
// それ以外の場合はエラーを報告する
long int expect_number() {
    if (token->kind != TK_NUM) {
        error_at(token->str, "not a number");
    }
    int val = token->val;
    token = token->next;
    return val;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

// 新しいトークンを作成して cur につなげる
Token *new_token(TokenKind kind, Token *cur, char *str) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    cur->next = tok;
    return tok;
}

// 入力文字列 p をトークナイズしてそれを返す
Token *tokenize() {
    char *p = user_input;
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (*p == '+' || *p == '-') {
            cur = new_token(TK_RESERVED, cur, p++);
            continue;
        }

        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error_at(p, "expected a number");
    }

    new_token(TK_EOF, cur, p);
    return head.next;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Wrong number of arguments\n");
        return 1;
    }

    user_input = argv[1];
    // トークナイズを実施
    token = tokenize();

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // 式の最初は数値でないといけないのでチェックし、最初の mov 命令を出力
    printf("  mov rax, %ld\n", expect_number());

    // 終端に到達するまで、"+数字" か "-数字" が並ぶ想定で動作
    while (!at_eof()) {
        if (consume('+')) {
            printf("  add rax, %ld\n", expect_number());
            continue;
        }

        // '+' ではないということは絶対に '-' なので、戻り値は不要
        expect('-');
        printf("  sub rax, %ld\n", expect_number());
    }

    printf("  ret\n");
    return 0;
}
