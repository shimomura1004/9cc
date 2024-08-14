#include "9cc.h"

// int の型情報を malloc して返す
Type *int_type() {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_INT;
    return ty;
}

// ベースの型情報を受取り、ポインタ型としてラップして返す
Type *pointer_to(Type *base) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
    return ty;
}

// ベースとなる型を配列型にラップする
Type *array_of(Type *base, int size) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_ARRAY;
    ty->base = base;
    ty->array_size = size;
    return ty;
}

// 型から変数サイズを計算
int size_of(Type *ty) {
    if (ty->kind == TY_INT || ty->kind == TY_PTR) {
        return 8;
    }
    assert(ty->kind == TY_ARRAY);
    return size_of(ty->base) * ty->array_size;
}

// 子要素を含めノードに型をつける
void visit(Node *node) {
    if (!node) {
        return;
    }

    // Node 構造体の全ての子要素を走査
    visit(node->lhs);
    visit(node->rhs);
    visit(node->cond);
    visit(node->then);
    visit(node->els);
    visit(node->init);
    visit(node->inc);

    for (Node *n = node->body; n; n = n->next) {
        visit(n);
    }
    for (Node *n = node->args; n; n = n->next) {
        visit(n);
    }

    // 型をつける
    switch (node->kind) {
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
        node->ty = int_type();
        return;
    case ND_VAR:
        node->ty = node->var->ty;
        return;
    case ND_ADD:
        // 足し算の右側がポインタ・配列だった場合は左右を入れ替える
        // つまり x + &y; => &y + x
        // base が NULL でないということは、配列かポインタのいずれかである
        if (node->rhs->ty->base) {
            Node *tmp = node->lhs;
            node->lhs = node->rhs;
            node->rhs = tmp;
        }
        // 入れ替えても右側がポインタ・配列の場合、
        // つまりポインタ同士の加算となるためエラーとする
        if (node->rhs->ty->base) {
            error_tok(node->tok, "invalid pointer arithmetic operands");
        }
        // 足し算の式全体の型は、左手側の型と同じ
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        // 足し算と同じだが、左右の入れ替えはできないので簡易化
        if (node->rhs->ty->base) {
            error_tok(node->tok, "invalid pointer arithmetic operands");
        }
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_ADDR:
        if (node->lhs->ty->kind == TY_ARRAY) {
            // 配列変数への & は、配列の中身の型へのポインタ型になる
            // int x[2] のとき int *y = x; int *y = &x; の y は同じ
            // つまり x と &x は同じ値である
            // 一方で型は配列からポインタへ変わる
            node->ty = pointer_to(node->lhs->ty->base);
        }
        else {
            // 左辺が配列でない場合は、
            // ポインタ変数への & は、ポインタをひとつネストする
            // int なら int* になるし、int* なら int** になる
            node->ty = pointer_to(node->lhs->ty);
        }
        return;
    case ND_DEREF:
        // デリファレンスの場合はポインタ型のネストを1つ削除する
        if (!node->lhs->ty->base) {
            error_tok(node->tok, "invalid pointer dereference");
        }
        node->ty = node->lhs->ty->base;
        return;
    case ND_SIZEOF:
        // sizeof の値の計算はコンパイル時に終わり、AST には sizeof は残らない
        node->kind = ND_NUM;
        node->ty = int_type();
        // 型から値を計算
        node->val = size_of(node->lhs->ty);
        // sizeof 演算子の項では値の情報そのものは不要なので削除
        node->lhs = NULL;
        return;
    }
}

void add_type(Function *prog) {
    for (Function *fn = prog; fn; fn = fn->next) {
        for (Node *node = fn->node; node; node = node->next) {
            visit(node);
        }
    }
}