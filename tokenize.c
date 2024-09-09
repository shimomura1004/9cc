#include <string.h>
#include <ctype.h>
#include "9cc.h"

char *filename;
char *user_input;
Token *token;

char *strndup(char *p, int len) {
    char *buf = malloc(len + 1);
    strncpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

// 先頭トークンが文字列 s とマッチしていれば真を返す
// トークンは進めない
Token *peek(char *s) {
    if (token->kind != TK_RESERVED ||
        strlen(s) != token->len ||
        memcmp(token->str, s, token->len)) {
        return NULL;
    }
    return token;
}

// 次のトークンが期待している記号と同じであればトークンを1つ読み進め真を返す
// それ以外の場合は偽を返す
Token *consume(char *s) {
    if (!peek(s)) {
        return NULL;
    }
    Token *t = token;
    token = token->next;
    return t;
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

// 次のトークンが期待している文字列と同じであればトークンを1つ読み進める
// それ以外の場合はエラーを報告する
void expect(char *s) {
    if (!peek(s)) {
        error_tok(token, "expected \"%s\"", s);
    }
    token = token->next;
}

// 次のトークンが数値の場合、トークンをひとつ読み進めてその数値を返す
// それ以外の場合はエラーを報告する
long expect_number() {
    if (token->kind != TK_NUM) {
        error_tok(token, "expected a number");
    }
    long val = token->val;
    token = token->next;
    return val;
}

// 次のトークンが識別子の場合、トークンをひとつ読み進めてその識別子を返す
// それ以外の場合はエラーを報告する
char *expect_ident() {
    if (token->kind != TK_IDENT) {
        error_tok(token, "expected an identifier");
    }
    char *s = strndup(token->str, token->len);
    token = token->next;
    return s;
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
        "int",
        "char",
        "sizeof",
        "struct",
        "typedef",
        "short",
        "long",
        "void",
        "_Bool",
        "enum",
        "static",
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
    // 先に長い演算子からマッチを試みる
    static char *ops[] = {
        "==", "!=", "<=", ">=", "->", "++", "--",
        "+=", "-=", "*=", "/=", 
        "+", "-", "*", "&", "/", "(", ")", "<", ">", ";", "=",
        "{", "}", ",", "[", "]", ".", ",", "!",
    };

    // Multi/single-letter punctuator
    for (int i=0; i < sizeof(ops) / sizeof(*ops); i++) {
        if (startswith(p, ops[i])) {
            return ops[i];
        }
    }

    return NULL;
}

char get_escape_char(char c) {
    switch (c)
    {
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'e': return '\e';
    case '0': return '\0';
    default:  return c;
    }
}

Token *read_string_literal(Token *cur, char *start) {
    char *p = start + 1;
    char buf[1024];
    int len = 0;

    for (;;) {
        if (len == sizeof(buf)) {
            // 文字列リテラルは1024文字まで
            error_at(start, "string literal too large");
        }
        if (*p == '\0') {
            // ダブルクオートがないまま末尾に達した場合
            error_at(start, "unclosed string literal");
        }
        if (*p == '"') {
            break;
        }

        if (*p == '\\') {
            p++;
            buf[len++] = get_escape_char(*p++);
        }
        else {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, cur, start, p - start + 1);
    tok->contents = malloc(len + 1);
    memcpy(tok->contents, buf, len);
    tok->contents[len] = '\0';
    tok->cont_len = len + 1;
    return tok;
}

Token *read_char_literal(Token *cur, char *start) {
    // 最初の一文字はシングルクォートなので飛ばす
    char *p = start + 1;
    if (*p == '\0') {
        error_at(start, "unclosed char literal");
    }

    char c;
    if (*p == '\\') {
        p++;
        c = get_escape_char(*p++);
    }
    else {
        c = *p++;
    }

    if (*p != '\'') {
        error_at(start, "char literal too long");
    }
    p++;

    Token *tok = new_token(TK_NUM, cur, start, p - start);
    tok->val = c;
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

        // 空白と同様にコメントも読み飛ばす
        if (startswith(p, "//")) {
            p += 2;
            while (*p != '\n') {
                p++;
            }
            continue;
        }

        if (startswith(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (!q) {
                error_at(p, "unclosed block comment");
            }
            p = q + 2;
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

        // String literal
        if (*p == '"') {
            cur = read_string_literal(cur, p);
            p += cur->len;
            continue;
        }

        // Character literal
        if (*p == '\'') {
            cur = read_char_literal(cur, p);
            p += cur->len;
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
