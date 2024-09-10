#include "9cc.h"

void gen(Node *node);

// ユニークなラベルを作るための連番
static int labelseq = 0;

// 今コード生成している関数の名称
static char *funcname;

// System V AMD64 ABI で、関数呼び出し時の引数を指定するのに使うレジスタ
char *argreg1[] = { "dil", "sil",  "dl",  "cl", "r8b", "r9b" };
char *argreg2[] = {  "di",  "si",  "dx",  "cx", "r8w", "r9w" };
char *argreg4[] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };
char *argreg8[] = { "rdi", "rsi", "rdx", "rcx",  "r8",  "r9" };

// スタックトップにアドレスが入っている前提で
// アドレスを取り出し、代わりにアドレスが指す値をスタックトップにいれる
// x86 では扱うデータのバイト数によってレジスタが異なる
void load(Type *ty) {
    printf("  pop rax\n");

    int sz = size_of(ty);
    if (sz == 1) {
        // rax が指すアドレスから1バイト読んで rax にいれる
        printf("  movsx rax, byte ptr [rax]\n");
    }
    else if (sz == 2) {
        printf("  movsx rax, word ptr [rax]\n");
    }
    else if (sz == 4) {
        // rax が指すアドレスから4バイト読んで rax にいれる
        printf("  movsxd rax, dword ptr [rax]\n");
    }
    else {
        assert(sz == 8);
        // rax が指すアドレスから8バイト読んで rax にいれる
        printf("  mov rax, [rax]\n");
    }

    printf("  push rax\n");
}

// スタックトップに値、その次にアドレスが入っている前提で
// アドレスに値を入れ、値を再度スタックトップに入れなおす
void store(Type *ty) {
    // 右辺の計算結果を rdi に取り出し
    printf("  pop rdi\n");
    // 左辺の変数のアドレスを rax に取り出し
    printf("  pop rax\n");

    // C ではブールはゼロか非ゼロかだけで判定するが
    // 処理のしやすさのためここで 0 か 1 に丸める
    if (ty->kind == TY_BOOL) {
        // cmp は eflag レジスタの ZF(zero flag) ビットを更新する
        printf("  cmp rdi, 0\n");
        // ZF が1でないとき(rdi と 0 が等しくないとき) dil を1にする
        printf("  setne dil\n");
        // dil は1バイトなので movzb で rdi(8バイト)に拡張する
        printf("  movzb rdi, dil\n");
    }

    int sz = size_of(ty);
    // 左辺の変数のメモリ領域に右辺の計算結果を入れる
    if (sz == 1) {
        // dil は rdi の最下位1バイト
        printf("  mov [rax], dil\n");
    }
    else if (sz == 2) {
        printf("  mov [rax], di\n");
    }
    else if (sz == 4) {
        printf("  mov [rax], edi\n");
    }
    else {
        assert(sz == 8);
        printf("  mov [rax], rdi\n");
    }

    // 代入式は右辺の値を返すので、再び rdi をスタックトップにいれる
    printf("  push rdi\n");
}

// スタックから値を取り出して、指定された型に丸めて、スタックに戻す
void truncate(Type *ty) {
    printf("  pop rax\n");

    if (ty->kind == TY_BOOL) {
        // bool へのキャストの場合、単に 0 と比較して、0  じゃなかったら 1 をセットする　
        printf("  cmp rax, 0\n");
        printf("  setne al\n");
    }

    int sz = size_of(ty);
    if (sz == 1) {
        printf("  movsx rax, al\n");
    }
    else if (sz == 2) {
        printf("  movsx eax, ax\n");
    }
    else if (sz == 4) {
        printf("  movsxd rax, eax\n");
    }

    // 最後にスタックトップに値を戻す
    printf("  push rax\n");
}

// スタックトップにある値をインクリメントして置き換える
void inc(Type *ty) {
    printf("  pop rax\n");
    // ty->base に値が入っているということは、この型は基本型ではなく配列やポインタなど
    // この場合は単純に1を足すのではなく変数の型に応じて足さないといけない
    // アドレスに対する加減算と同じ
    printf("  add rax, %d\n", ty->base ? size_of(ty->base) : 1);
    printf("  push rax\n");
}

// スタックトップにある値をデクリメントして置き換える
void dec(Type *ty) {
    printf("  pop rax\n");
    printf("  sub rax, %d\n", ty->base ? size_of(ty->base) : 1);
    printf("  push rax\n");
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
    case ND_MEMBER:
        // 代入文の左辺に構造体メンバアクセスがあった場合
        // 構造体のアドレスをスタックトップに置く
        gen_addr(node->lhs);
        printf("  pop rax\n");
        // 構造体メンバのオフセットを追加してスタックトップに置き直す
        printf("  add rax, %d\n", node->member->offset);
        printf("  push rax\n");
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// 左辺値のためのコードを生成する
// エラーチェックだけして gen_addr を呼ぶだけ
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
        // 数字には int の場合と long の場合がある
        if (node->val == (int)node->val) {
            printf("  push %ld\n", node->val);
        }
        else {
            // 64bit の即値の場合は mov ではなく movabs を使う
            printf("  movabs rax, %ld\n", node->val);
            printf("  push rax\n");
        }
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

        // スタックはトップからしかアクセスできないので
        // 後ろの引数から順番に pop してレジスタに入れていく
        // 引数の型が char でも int と同じレジスタを使う
        for (int i = nargs - 1; i >= 0; i--) {
            printf("  pop %s\n", argreg8[i]);
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

        // 関数の戻り値の型に合わせて丸める
        truncate(node->ty);
        return;
    }
    case ND_RETURN:
        // return する値を計算しスタックトップに入れる
        gen(node->lhs);
        printf("  pop rax\n");
        // 関数を抜ける前の共通処理(epilogue)があるので直接 ret せずジャンプ
        printf("  jmp .Lreturn.%s\n", funcname);
        return;
    case ND_NOT:
        // 値を計算しスタックトップに置く
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        // sete は直前の cmp の結果が equal だったら al に 1 を書き込む
        printf("  sete al\n");
        // 64ビットに拡張した上でスタックトップに置く
        printf("  movzb rax, al\n");
        printf("  push rax\n");
        return;
    case ND_BITNOT:
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  not rax\n");
        printf("  push rax\n");
        return;
    case ND_LOGAND: {
        int seq = labelseq++;
        // && は短絡の可能性がある
        // まず左側の式を計算しスタックトップに置く
        gen(node->lhs);
        // 左側の式の値が 0 かどうかを判定
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        // 0 の場合(すなわち右側の式を評価する必要がなくなったとき)はジャンプ
        printf("  je  .Lfalse%d\n", seq);
        // 右側の式でも同様の処理を実施
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je  .Lfalse%d\n", seq);
        // どちらも 0 でなかった場合は 1 を push して終了
        printf("  push 1\n");
        printf("  jmp .Lend%d\n", seq);
        // 0 になった場合のジャンプ先をここに出力
        printf(".Lfalse%d:\n", seq);
        printf("  push 0\n");
        // 出口にもラベル
        printf(".Lend%d:\n", seq);
        return;
    }
    case ND_LOGOR: {
        // LOGAND の場合と同じだが、1 のとき短絡する
        int seq = labelseq++;
        gen(node->lhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", seq);
        gen(node->rhs);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", seq);
        printf("  push 0\n");
        printf("  jmp .Lend%d\n", seq);
        printf(".Ltrue%d:\n", seq);
        printf("  push 1\n");
        printf(".Lend%d:\n", seq);
        return;
    }
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
    case ND_STMT_EXPR:
        // stmt_expr の場合、最後のノードは必ず式になっている
        // パースするときに expr_stmt (文) の中身(式)を取り出しているため
        // よってそのままコード生成すれば最終的にスタックトップに最後の式の結果が残る
        // (stmt_expr の場合はスタックトップの値を最後に捨てるようになっている)
        for (Node *n = node->body; n; n = n->next) {
            gen(n);
        }
        return;
    case ND_VAR:
    case ND_MEMBER:
        // 指定された変数に対応するアドレスをスタックトップにいれる
        gen_addr(node);
        if (node->ty->kind != TY_ARRAY) {
            // 変数の型が配列でなければアドレスを指す先の値に入れ替え
            // 変数の型が配列のときになにもしない理由は、
            // int x[2] のとき x は配列の先頭アドレスそのものを表すから
            load(node->ty);
        }
        return;
    case ND_ASSIGN:
        // 左辺の変数のアドレスをスタックトップにいれる
        gen_lval(node->lhs);
        // 右辺を計算しスタックトップにいれる
        gen(node->rhs);
        // スタックに入っている値を、スタックに入っているアドレスに保存
        store(node->ty);
        return;
    case ND_PRE_INC:
        // まずインクリメントの対象となっている式のアドレスを計算してスタックトップに置く
        gen_lval(node->lhs);
        // rsp の指す場所にあるデータ(つまり gen_lval で計算した結果)をスタックトップに置く
        printf("  push [rsp]\n");
        // この時点でスタックの最上位2つのデータはインクリメントの対象となっている式のアドレス
        // load でスタックトップのアドレスが指す値を取り出し置き換え
        load(node->ty);
        // インクリメント
        inc(node->ty);
        // この時点でスタックの最上位にはインクリメント済みの値、その次にアドレスが入っている
        // よって store でインクリメントした結果を書き戻すことができる
        store(node->ty);
        // この時点でインクリメント対象が保存されているメモリの内容が更新完了
        // さらに store はスタックトップに値を残すので、インクリメント済みの値が
        // 式と評価結果としてスタックトップに残っている
        return;
    case ND_PRE_DEC:
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        load(node->ty);
        dec(node->ty);
        store(node->ty);
        return;
    case ND_POST_INC:
        // PRE_INC と同様に、スタックの上位2つにインクリメント対象の式のアドレスを準備する
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        // load/inc/store で、最上位の値をインクリメントした後の値に変換する
        load(node->ty);
        inc(node->ty);
        store(node->ty);
        // この段階でインクリメント対象が保存されているメモリの内容は書き換わっている
        // 後置++の場合はインクリメント前の値でその後の計算をしないといけないのでデクリメントする
        dec(node->ty);
        // この段階で、インクリメント対象が保存されているメモリはインクリメント後の値で書き換わり
        // スタックトップはインクリメント前の値に戻っている
        return;
    case ND_POST_DEC:
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        load(node->ty);
        dec(node->ty);
        store(node->ty);
        inc(node->ty);
        return;
    case ND_A_ADD:
    case ND_A_SUB:
    case ND_A_MUL:
    case ND_A_DIV: {
        // x += y は  x = x + y と同じ
        // まず左辺値のアドレスを2つスタックトップに置く
        gen_lval(node->lhs);
        printf("  push [rsp]\n");
        // スタックトップの値を値で置き換え
        load(node->lhs->ty);
        // 加算する値を計算しスタックトップに置く
        gen(node->rhs);
        // 式が x += y のとき、この時点でスタックは上から (y の値) (x の値) (x のアドレス)
        printf("  pop rdi\n");
        printf("  pop rax\n");
        // この時点で rdi と rax に y と x の値がそれぞれ入っている
        // スタックトップには x のアドレスが入っている

        // rdi と rax を使って右辺を計算する
        switch (node->kind) {
        case ND_A_ADD:
            if (node->ty->base) {
                // 配列やポインタ型の場合は変数のサイズをかけた値を足す必要がある
                printf("  imul rdi, %d\n", size_of(node->ty->base));
            }
            printf("  add rax, rdi\n");
            break;
        case ND_A_SUB:
            if (node->ty->base) {
                printf("  imul rdi, %d\n", size_of(node->ty->base));
            }
            printf("  sub rax, rdi\n");
            break;
        case ND_A_MUL:
            printf("  imul rax, rdi\n");
            break;
        case ND_A_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        }

        // rax に入った計算結果をスタックトップに置く
        printf("  push rax\n");
        // x のアドレスに値を書き戻す
        store(node->ty);
        return;
    }
    case ND_COMMA:
        gen(node->lhs);
        gen(node->rhs);
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
            load(node->ty);
        }
        return;
    case ND_CAST:
        // 普通に計算するコードを出力したあと truncate する　
        gen(node->lhs);
        truncate(node->ty);
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
    case ND_BITAND:
        printf("  and rax, rdi\n");
        break;
    case ND_BITOR:
        printf("  or rax, rdi\n");
        break;
    case ND_BITXOR:
        printf("  xor rax, rdi\n");
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

        if (!var->contents) {
            // コンテンツがないということは、これは文字列ではない
            // グローバル変数の場合はゼロ初期化する
            // .zero は、指定したバイト数分の領域を 0 初期化して確保する
            printf("  .zero %d\n", size_of(var->ty));
            continue;
        }

        // 文字列リテラルの場合は一文字ずつ出力
        for (int i = 0; i < var->cont_len; i++) {
            printf("  .byte %d\n", var->contents[i]);
        }
    }
}

// 引数の型に応じて読み取るレジスタを変える
void load_arg(Var *var, int idx) {
    int sz = size_of(var->ty);
    if (sz == 1) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg1[idx]);
    }
    else if (sz == 2) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg2[idx]);
    }
    else if (sz == 4) {
        printf("  mov [rbp-%d], %s\n", var->offset, argreg4[idx]);
    }
    else {
        assert(sz == 8);
        printf("  mov [rbp-%d], %s\n", var->offset, argreg8[idx]);
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
            load_arg(vl->var, i++);
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
