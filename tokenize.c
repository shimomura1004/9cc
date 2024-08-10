#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "9cc.h"

char *user_input;
Token *token;

char *strndup(char *p, int len) {
    char *buf = malloc(len + 1);
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

// 次のトークンが期待している記号と同じであればトークンを1つ読み進め真を返す
// それ以外の場合は偽を返す
bool consume(char *op) {
    if (token->kind != TK_RESERVED ||
        strlen(op) != token->len ||
        memcmp(token->str, op, token->len)) {
        return false;
    }
    token = token->next;
    return true;
}

// 次のトークンが識別子ならトークンを1つ読み進めトークンを返す
// そうでなければ NULL を返す
Token *consume_ident() {
    if (token->kind != TK_IDENT) {
        return NULL;
    }
    Token *t = token;
    token = token->next;
    return t;
}

// 次のトークンが期待している記号と同じであればトークンを1つ読み進める
// それ以外の場合はエラーを報告する
void expect(char *op) {
    if (token->kind != TK_RESERVED ||
        strlen(op) != token->len ||
        memcmp(token->str, op, token->len)) {
        error_at(token->str, "not \"%s\"", op);
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
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    cur->next = tok;
    return tok;
}

bool is_alpha(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

bool is_alnum(char c) {
    return is_alpha(c) || ('0' <= c && c <= '9');
}

// 文字列 p の先頭が文字列 q と一致するかを判定
bool startswith(char *p, char *q) {
    return memcmp(p, q, strlen(q)) == 0;
}

// 文字列 p が予約語で始まるかを判定し、その文字列を返す
char *starts_with_reserved(char *p) {
    // Keywords
    static char *kw[] = {
        "return",
        "if",
        "else",
        "while",
        "for",
    };

    for (int i=0; i < sizeof(kw) / sizeof(*kw); i++) {
        int len = strlen(kw[i]);
        // 予約語 "if" が、識別子 "iff" を誤認識しないように1文字先読み
        if (startswith(p, kw[i]) && !is_alnum(p[len])) {
            return kw[i];
        }
    }

    return NULL;
}

// 文字列 p が演算子で始まるかを判定し、その文字列を返す
char *starts_with_reserved_ops(char *p) {
    static char *ops[] = {
        "==", "!=", "<=", ">=",
        "+", "-", "*", "/", "(", ")", "<", ">", ";", "=", "{", "}",
    };

    // Multi/single-letter punctuator
    for (int i=0; i < sizeof(ops) / sizeof(*ops); i++) {
        if (startswith(p, ops[i])) {
            return ops[i];
        }
    }

    return NULL;
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

        // Keyword
        char *kw = starts_with_reserved(p);
        if (kw) {
            int len = strlen(kw);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
            continue;
        }
        
        char *op = starts_with_reserved_ops(p);
        if (op) {
            int len = strlen(op);
            cur = new_token(TK_RESERVED, cur, p, len);
            p += len;
            continue;
        }

        // Identifier
        if (isalpha(*p)) {
            char *q = p++;
            while (is_alnum(*p)) {
                p++;
            }
            cur = new_token(TK_IDENT, cur, q, p - q);
            continue;
        }

        // Integer literal
        if (isdigit(*p)) {
            cur = new_token(TK_NUM, cur, p, 0);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        error_at(p, "invalid token");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}
