#include <stdarg.h>
#include <string.h>
#include "9cc.h"

char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        error("cannot open %s: %s", path, strerror(errno));
    }

    int filemax = 10 * 1024 * 1024;
    char *buf = malloc(filemax);
    // 確保した領域を全部使って読む
    int size = fread(buf, 1, filemax - 2, fp);
    if (!feof(fp)) {
        // 読みきれなかったらエラー
        error("%s: file too large");
    }

    // ソースファイルの末尾は改行文字で終わることを強制する
    if (size == 0 || buf[size - 1] != '\n') {
        buf[size++] = '\n';
    }
    buf[size] = '\0';
    return buf;
}

// エラーメッセージを出力して終了する
// foo.c:10: x = y + 1:
//               ^ <error message here>
void verror_at(char *loc, char *fmt, va_list ap) {
    char *line = loc;
    // loc から1文字ずつ戻りながら改行直後の位置を探す
    while (user_input < line && line[-1] != '\n') {
        line--;
    }

    // loc から1文字ずつ進めながら行末の位置を探す
    char *end = loc;
    while (*end != '\n') {
        end++;
    }

    // 先頭から改行文字を数え、loc の行番号を求める
    int line_num = 1;
    for (char *p = user_input; p < line; p++) {
        if (*p == '\n') {
            line_num++;
        }
    }

    // ファイル名、行数、行の内容を表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    // コードはヌル終端になっていないので1行で強制的に切る必要がある
    // "." と "s" を組み合わせると、指定した文字数で切り捨てることができる
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // エラーメッセージを表示
    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (tok) {
        verror_at(tok->str, fmt, ap);
    }

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// 数値 n を、次の align の倍数に切り上げる
int align_to(int n, int align) {
    return (n + align - 1) & ~(align - 1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        error("Wrong number of arguments\n");
    }

    // トークナイズし、パースして AST を作る
    filename = argv[1];
    user_input = read_file(argv[1]);
    token = tokenize();
    Program *prog = program();
    add_type(prog);

    // 各関数で使われる各ローカル変数にオフセットの情報を割り当てる
    for (Function *fn = prog->fns; fn; fn = fn->next) {
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next) {
            Var *var = vl->var;
            offset += size_of(var->ty);
            var->offset = offset;
        }
        // char などでスタックサイズが8の倍数からずれる可能性があるので調整
        fn->stack_size = align_to(offset, 8);
    }

    codegen(prog);

    return 0;
}
