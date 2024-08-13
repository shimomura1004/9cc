#include "9cc.h"

Type *int_type() {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_INT;
    return ty;
}

Type *pointer_to(Type *base) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
    return ty;
}

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
    case ND_VAR:
    case ND_FUNCALL:
    case ND_NUM:
        node->ty = int_type();
        return;
    case ND_ADD:
        // 足し算の右側がポインタだった場合は左右を入れ替える
        // つまり x + &y; => &y + x
        if (node->rhs->ty->kind == TY_PTR) {
            Node *tmp = node->lhs;
            node->lhs = node->rhs;
            node->rhs = tmp;
        }
        // 入れ替えても右側がポインタ型の場合、つまりポインタ同士の加算の場合はエラー
        if (node->rhs->ty->kind == TY_PTR) {
            error_tok(node->tok, "invalid pointer arithmetic operands");
        }
        // 足し算の式全体の型は、左手側の型と同じ
        node->ty = node->lhs->ty;
        return;
    case ND_SUB:
        // 足し算と同じだが、左右の入れ替えはできないので簡易化
        if (node->rhs->ty->kind == TY_PTR) {
            error_tok(node->tok, "invalid pointer arithmetic operands");
        }
        node->ty = node->lhs->ty;
        return;
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_ADDR:
        // 参照型の場合はポインタ型のネストを1つ追加する
        node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        // デリファレンスの場合はポインタ型のネストを1つ削除する
        if (node->lhs->ty->kind == TY_PTR) {
            node->ty = node->lhs->ty->base;
        }
        else {
            // todo: ポインタ型でないものをデリファレンスするのはいいのか？
            node->ty = int_type();
        }
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