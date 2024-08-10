#include <stdio.h>
#include "9cc.h"

// ユニークなラベルを作るための連番
static int labelseq = 0;

// 変数のオフセットを計算してスタックトップに置く
void gen_lval(Node *node) {
    if (node->kind != ND_VAR) {
        error("L-value of assignment is not a variable");
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->var->offset);
    printf("  push rax\n");
}

void gen(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_EXPR_STMT:
        // 代入されない文の場合、スタックトップに入った戻り値は捨てないといけない
        gen(node->lhs);
        printf("  add rsp, 8\n");
        return;
    case ND_FUNCALL:
        // 適切な関数を呼び出す
        printf("  call %s\n", node->funcname);
        // 戻り値は rax に入って戻って来る
        // 関数呼び出しの結果は関数の戻り値なので、それをスタックトップにいれる
        printf("  push rax\n");
        return;
    case ND_RETURN:
        // return する値を計算しスタックトップに入れる
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        // あとの文があっても無視して ret を出力し脱出
        printf("  ret\n");
        return;
    case ND_IF: {
        int seq = labelseq++;

        // 条件式を評価しスタックトップに結果を入れる
        gen(node->cond);
        // 結果を取り出して比較
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");

        if (node->els) {
            // else 節がある場合
            // 偽だったら else 節にジャンプ
            printf("  je  .Lelse%d\n", seq);
            // 真だった場合のコードを生成
            gen(node->then);
            printf("  jmp  .Lend%d\n", seq);
            printf(".Lelse%d:\n", seq);
            // 偽だった場合のコードを生成
            gen(node->els);
        }
        else {
            // else 節がない場合
            // 偽だったら if 文のあとにジャンプ
            printf("  je  .Lend%d\n", seq);
            // 真だった場合のコードを生成
            gen(node->then);
        }

        printf(".Lend%d:\n", seq);

        return;
    }
    case ND_WHILE: {
        int seq = labelseq++;
        // ループで戻って来るときのためのラベルを追加
        printf(".Lbegin%d:\n", seq);
        // 条件部を評価
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        // 条件を満たしたら末尾にジャンプ
        printf("  je  .Lend%d\n", seq);
        gen(node->then);
        printf("  jmp .Lbegin%d\n", seq);
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_FOR: {
        int seq = labelseq++;
        if (node->init) {
            // ループに入る前に初期化部を実行
            gen(node->init);
        }
        printf(".Lbegin%d:\n", seq);
        if (node->cond) {
            // 条件部を評価しスタックトップにいれる
            gen(node->cond);
            // スタックトップから値を取り出して比較
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je  .Lend%d\n", seq);
        }
        // ループ本体を実行
        gen(node->then);
        if (node->inc) {
            // ループ本体が終了したら、インクリメント部を実行
            gen(node->inc);
        }
        printf("  jmp  .Lbegin%d\n", seq);
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next) {
            gen(n);
        }
        return;
    case ND_VAR:
        gen_lval(node);
        // スタックトップに置かれた代入先のアドレスが指す値を rax にいれる
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    case ND_ASSIGN:
        // 左辺の変数のアドレスをスタックトップにいれる
        gen_lval(node->lhs);
        // 右辺を計算しスタックトップにいれる
        gen(node->rhs);

        // 右辺の計算結果を rdi に取り出し
        printf("  pop rdi\n");
        // 左辺の変数のアドレスを rax に取り出し
        printf("  pop rax\n");
        // 左辺の変数のメモリ領域に右辺の計算結果を入れる
        printf("  mov [rax], rdi\n");
        // 代入式は右辺の値を返すので、再び rdi をスタックトップにいれる
        printf("  push rdi\n");
        return;
    }

    // スタックマシンとして計算する
    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
    case ND_ADD:
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        printf("  sub rax, rdi\n");
        break;
    case ND_MUL:
        printf("  imul rax, rdi\n");
        break;
    case ND_DIV:
        printf("  cqo\n");
        printf("  idiv rdi\n");
        break;
    case ND_EQ:
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_NE:
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LT:
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
        break;
    case ND_LE:
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
        break;
    default:
        error("Unknown node: %d\n", node->kind);
        break;
    }

    printf("  push rax\n");
}

void codegen(Program *prog) {
    // アセンブリの前半部分を出力
    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // プロローグ
    // 変数26個分の領域を固定で確保する
    printf("# prologue\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", prog->stack_size);

    printf("# program body\n");
    // AST を読み取りコードを生成する
    for (Node *node = prog->node; node; node = node->next) {
        gen(node);
    }

    // エピローグ
    // スタックを戻す
    printf("# epilogue\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    // 最後の式の評価結果が rax に残っているのでそのまま ret すればいい
    printf("  ret\n");
}