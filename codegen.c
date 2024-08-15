#include "9cc.h"

void gen(Node *node);

// ユニークなラベルを作るための連番
static int labelseq = 0;

// 今コード生成している関数の名称
static char *funcname;

// System V AMD64 ABI で、関数呼び出し時の引数を指定するのに使うレジスタ
char *argreg[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

// スタックトップにアドレスが入っている前提で
// アドレスを取り出し、代わりにアドレスが指す値をスタックトップにいれる
void load() {
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

// スタックトップに値、その次にアドレスが入っている前提で
// アドレスに値を入れ、値を再度スタックトップに入れなおす
void store() {
    // 右辺の計算結果を rdi に取り出し
    printf("  pop rdi\n");
    // 左辺の変数のアドレスを rax に取り出し
    printf("  pop rax\n");
    // 左辺の変数のメモリ領域に右辺の計算結果を入れる
    printf("  mov [rax], rdi\n");
    // 代入式は右辺の値を返すので、再び rdi をスタックトップにいれる
    printf("  push rdi\n");
}

// 変数のオフセットを計算してスタックトップに置く
void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR: {
        Var *var = node->var;

        if (var->is_local) {
            printf("  mov rax, rbp\n");
            printf("  sub rax, %d\n", var->offset);
            printf("  push rax\n");
        }
        else {
            printf("  push offset %s\n", var->name);
        }
        return;
    }
    case ND_DEREF:
        // 代入文の左辺にデリファレンスがあった場合
        gen(node->lhs);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// 左辺値のためのコードを生成する
void gen_lval(Node *node) {
    if (node->ty->kind == TY_ARRAY) {
        // 配列型変数への代入はできない
        // int x[2] のとき x = NULL はコンパイルエラーになる
        error_tok(node->tok, "not an lvalue");
    }
    gen_addr(node);
}

void gen(Node *node) {
    switch (node->kind) {
    case ND_NULL:
        return;
    case ND_NUM:
        printf("  push %d\n", node->val);
        return;
    case ND_EXPR_STMT:
        // 代入されない文の場合、スタックトップに入った戻り値は捨てないといけない
        gen(node->lhs);
        printf("  add rsp, 8\n");
        return;
    case ND_FUNCALL: {
        // 引数を第一引数から順番に評価し、結果を対応するレジスタにセットする
        // 実行後は第一引数の評価結果がスタックの深いところに入っている
        // 最後の引数の評価結果がスタックトップに入っている
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen(arg);
            nargs++;
        }

        // スタックはトップからしかアクセスできない
        // 後ろの引数から順番に pop してレジスタに入れていく
        for (int i = nargs - 1; i >= 0; i--) {
            printf("  pop %s\n", argreg[i]);
        }

        // 関数を呼び出す前に、ABI に準拠するため RSP を16の倍数にしないといけない
        // 実行時に RSP の値を見て分岐するコードを生成することで対応する
        int seq = labelseq++;
        // RSP と 15 のビット論理積を取り、ゼロなら16の倍数になっているのでそのまま
        // そうでなければ調整が必要
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n");
        printf("  jnz .Lcall%d\n", seq);
        printf("  mov rax, 0\n");
        // 特に調整せずそのまま関数呼び出し
        printf("  call %s\n", node->funcname);
        printf("  jmp .Lend%d\n", seq);

        // 16の倍数になっていない場合は 8 バイトずらす(RSP は8バイト単位で動くため)
        printf(".Lcall%d:\n", seq);
        printf("  sub rsp, 8\n");
        printf("  mov rax, 0\n");
        printf("  call %s\n", node->funcname);
        // この場合は RSP を調整した8バイトだけ戻す
        printf("  add rsp, 8\n");
        printf(".Lend%d:\n", seq);

        // 戻り値は rax に入って戻って来る
        // 関数呼び出しの結果は関数の戻り値なので、それをスタックトップにいれる
        printf("  push rax\n");
        return;
    }
    case ND_RETURN:
        // return する値を計算しスタックトップに入れる
        gen(node->lhs);
        printf("  pop rax\n");
        // 関数を抜ける前の共通処理(epilogue)があるので直接 ret せずジャンプ
        printf("  jmp .Lreturn.%s\n", funcname);
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
        // 指定された変数に対応するアドレスをスタックトップにいれる
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY) {
            // 変数の型が配列でなければアドレスを指す先の値に入れ替え
            // 変数の型が配列のときになにもしない理由は、
            // int x[2] のとき x は配列の先頭アドレスそのものを表すから
            load();
        }
        return;
    case ND_ASSIGN:
        // 左辺の変数のアドレスをスタックトップにいれる
        gen_lval(node->lhs);
        // 右辺を計算しスタックトップにいれる
        gen(node->rhs);
        // スタックに入っている値を、スタックに入っているアドレスに保存
        store();
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_DEREF:
        gen(node->lhs);
        if (node->ty->kind != TY_ARRAY) {
            // この node の型が配列型ということなので、deref した結果の型が配列ということ
            // 上記のコードの結果、スタックトップにはアドレスが入っているので、ロード
            // int x[2][2] とのき x と *x は同じくアドレスを指すので load は不要
            // int x[2] のとき *x は値となるので load が必要だが、
            // その場合はこの node の型が配列ではなく var になるので問題ない
            load();
        }
        return;
    }

    // スタックマシンとして計算する
    gen(node->lhs);
    gen(node->rhs);

    printf("  pop rdi\n");
    printf("  pop rax\n");

    switch (node->kind) {
    case ND_ADD:
        if (node->ty->base) {
            // ポインタ型か配列型の加算の場合の特別処理
            // ポインタへの加算は、ポインタの参照先の型のサイズ分の加算になる
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        }
        printf("  add rax, rdi\n");
        break;
    case ND_SUB:
        if (node->ty->base) {
            printf("  imul rdi, %d\n", size_of(node->ty->base));
        }
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

// データ領域を出力
void emit_data(Program *prog) {
    // 0 初期化するのであれば .data ではなく .bss に置くほうがベター
    // ELF ファイルのサイズが小さくなるため
    printf(".data\n");

    for (VarList *vl = prog->globals; vl; vl = vl->next) {
        Var *var = vl->var;
        // グローバル変数もアセンブラ上のラベルで表現される
        // ラベルは単にアドレスのエイリアス
        printf("%s:\n", var->name);
        // .zero は、指定したバイト数分の領域を 0 初期化して確保する
        printf("  .zero %d\n", size_of(var->ty));
    }
}

// テキスト領域を出力
void emit_text(Program *prog) {
    printf(".text\n");

    for (Function *fn = prog->fns; fn; fn = fn->next) {
        printf(".globl %s\n", fn->name);
        printf("%s:\n", fn->name);
        funcname = fn->name;

        // プロローグ
        printf("# prologue\n");
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", fn->stack_size);

        // レジスタに置かれた引数をスタックに書き込む
        int i = 0;
        for (VarList *vl = fn->params;vl; vl = vl->next) {
            Var *var = vl->var;
            printf("  mov [rbp-%d], %s\n", var->offset, argreg[i++]);
        }

        printf("# program body\n");
        // AST を読み取りコードを生成する
        for (Node *node = fn->node; node; node = node->next) {
            gen(node);
        }

        // エピローグ
        printf("# epilogue\n");
        printf(".Lreturn.%s:\n", funcname);
        // スタックを戻す
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        // 最後の式の評価結果が rax に残っているのでそのまま ret すればいい
        printf("  ret\n");
    }
}

void codegen(Program *prog) {
    printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}
