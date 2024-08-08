#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "9cc.h"

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

int main(int argc, char **argv) {
    if (argc != 2) {
        error("Wrong number of arguments\n");
    }

    // トークナイズし、パースして AST を作る
    user_input = argv[1];
    token = tokenize();
    program();

    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // プロローグ
    // 変数26個分の領域を固定で確保する
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, 208\n");

    // AST を読み取りコードを生成する
    for (int i=0; code[i]; i++) {
        gen(code[i]);
        // 文の実行結果がスタックに残っているので rax に捨てる
        printf("  pop rax\n");
    }

    // エピローグ
    // スタックを戻す
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    // 最後の式の評価結果が rax に残っているのでそのまま ret すればいい
    printf("  ret\n");

    return 0;
}
