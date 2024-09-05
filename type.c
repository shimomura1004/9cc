#include <string.h>
#include "9cc.h"

// 数値 n を、次の align の倍数に切り上げる
int align_to(int n, int align) {
    // e.g. align_to(10, 8) = (10 + 8 - 1) & ~(8 - 1)
    //                      = 17 & ~(7)
    //                      = 0b0001_0001 & ~0b0000_0111
    //                      = 0b0001_0001 &  0b1111_1000
    //                      = 0b0001_0000
    //                      = 16
    return (n + align - 1) & ~(align - 1);
}

// 型情報を malloc して返す
Type *new_type(TypeKind kind, int align) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = kind;
    ty->align = align;
    return ty;
}

Type *void_type() {
    return new_type(TY_VOID, 1);
}

Type *bool_type() {
    return new_type(TY_BOOL, 1);
}

Type *char_type() {
    return new_type(TY_CHAR, 1);
}

Type *short_type() {
    return new_type(TY_SHORT, 2);
}

Type *int_type() {
    return new_type(TY_INT, 4);
}

Type *long_type() {
    return new_type(TY_LONG, 8);
}

Type *enum_type() {
    return new_type(TY_ENUM, 4);
}

Type *func_type(Type *return_ty) {
    // todo: おそらく align は使わないから1にしている、関数ポインタとは別物
    Type *ty = new_type(TY_FUNC, 1);
    ty->return_ty = return_ty;
    return ty;
}

// ベースの型情報を受取り、ポインタ型としてラップして返す
Type *pointer_to(Type *base) {
    Type *ty = new_type(TY_PTR, 8);
    ty->base = base;
    return ty;
}

// ベースとなる型を配列型にラップする
Type *array_of(Type *base, int size) {
    Type *ty = new_type(TY_ARRAY, base->align);
    ty->base = base;
    ty->array_size = size;
    return ty;
}

// 型から変数サイズを計算
int size_of(Type *ty) {
    assert(ty->kind != TY_VOID);

    switch (ty->kind) {
    case TY_BOOL:
    case TY_CHAR:
        return 1;
    case TY_SHORT:
        return 2;
    case TY_INT:
    case TY_ENUM:
        return 4;
    case TY_LONG:
    case TY_PTR:
        return 8;
    case TY_ARRAY:
        return size_of(ty->base) * ty->array_size;
    default:
        assert(ty->kind == TY_STRUCT);
        Member *mem = ty->members;
        while (mem->next) {
            mem = mem->next;
        }
        // 最後のメンバのオフセットに、最後のメンバのサイズを足す
        int end = mem->offset + size_of(mem->ty);
        // 構造体自体の align には、メンバの中で最大の値が入っている
        // 最後のメンバのあとにもパディングが入る可能性がある
        return align_to(end, ty->align);
    }
}

// 構造体型から指定された名前のメンバを探す
Member *find_member(Type *ty, char *name) {
    assert(ty->kind == TY_STRUCT);
    for (Member *mem = ty->members; mem; mem = mem->next) {
        if (!strcmp(mem->name, name)) {
            return mem;
        }
    }
    return NULL;
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
        node->ty = int_type();
        return;
    case ND_NUM:
        // int(4バイト)に収まらない場合は long にする
        if (node->val == (int)node->val) {
            node->ty = int_type();
        }
        else {
            node->ty = long_type();
        }
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
    case ND_MEMBER: {
        // 構造体のメンバアクセスのノードの型をつける
        // 構造体以外のノードにメンバアクセスしようとしていたらエラー
        if (node->lhs->ty->kind != TY_STRUCT) {
            error_tok(node->tok, "not a struct");
        }
        // アクセスするメンバの型がこのノードの型になる
        node->member = find_member(node->lhs->ty, node->member_name);
        node->ty = node->member->ty;
        return;
    }
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
        if (node->ty->kind == TY_VOID) {
            error_tok(node->tok, "dereferencing a void pointer");
        }
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
    case ND_STMT_EXPR:
        // 最後の文の型がブロック全体の型になる
        Node *last = node->body;
        while (last->next) {
            last = last->next;
        }
        node->ty = last->ty;
        return;
    }
}

void add_type(Program *prog) {
    for (Function *fn = prog->fns; fn; fn = fn->next) {
        for (Node *node = fn->node; node; node = node->next) {
            visit(node);
        }
    }
}