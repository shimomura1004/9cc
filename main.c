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
    Function *prog = program();

    // 各関数で使われる各ローカル変数にオフセットの情報を割り当てる
    for (Function *fn = prog; fn; fn = fn->next) {
        int offset = 0;
        for (VarList *vl = fn->locals; vl; vl = vl->next) {
            offset += 8;
            vl->var->offset = offset;
        }
        fn->stack_size = offset;
    }

    codegen(prog);

    return 0;
}
