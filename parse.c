#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "9cc.h"

Var *locals;

Var *find_var(Token *tok) {
    for (Var *var = locals; var; var = var->next) {
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

Node *new_node(NodeKind kind) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

Node *new_num(int val) {
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

Node *new_var(Var *var) {
    Node *node = new_node(ND_VAR);
    node->var = var;
    return node;
}

// 新しい変数のエントリを作り locals に足す
Var *push_var(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->next = locals;
    var->name = name;
    locals = var;
    return var;
}

Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// program = stmt*
Program *program() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    Node head;
    head.next = NULL;
    Node *cur = &head;

    // 複数の文を前から順番にリンクさせていく
    while (!at_eof()) {
        // program は複数の stmt からなる
        cur->next = stmt();
        cur = cur->next;
    }

    Program *prog = calloc(1, sizeof(Program));
    // head はダミーのノードなので、その次のノードから使う
    prog->node = head.next;
    // パース中に作ったローカル変数一覧をそのまま渡す
    // calloc してあるのでこの関数を抜けても問題ない
    prog->locals = locals;
    return prog;
}

// stmt = expr ";" | "return" expr ";"
Node *stmt() {
    Node *node;

    if (consume("return")) {
        node = new_unary(ND_RETURN, expr());
    }
    else {
        node = expr();
    }

    expect(";");
    return node;
}

// expr = assign
Node *expr() {
    return assign();
}

// assign = equality ("=" assign)?
Node *assign() {
    Node *node = equality();
    if (consume("=")) {
        node = new_binary(ND_ASSIGN, node, assign());
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
    Node *node = relational();

    for (;;) {
        if (consume("==")) {
            node = new_binary(ND_EQ, node, relational());
        }
        else if (consume("!=")) {
            node = new_binary(ND_NE, node, relational());
        }
        else {
            return node;
        }
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
    Node *node = add();

    for (;;) {
        if (consume("<")) {
            node = new_binary(ND_LT, node, add());
        }
        else if (consume("<=")) {
            node = new_binary(ND_LE, node, add());
        }
        else if (consume(">")) {
            node = new_binary(ND_LT, add(), node);
        }
        else if (consume(">=")) {
            node = new_binary(ND_LE, add(), node);
        }
        else {
            return node;
        }
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
    Node *node = mul();

    for (;;) {
        if (consume("+")) {
            node = new_binary(ND_ADD, node, mul());
        }
        else if (consume("-")) {
            node = new_binary(ND_SUB, node, mul());
        }
        else {
            return node;
        }
    }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
    Node *node = unary();

    for (;;) {
        if (consume("*")) {
            node = new_binary(ND_MUL, node, unary());
        }
        else if (consume("/")) {
            node = new_binary(ND_DIV, node, unary());
        }
        else {
            return node;
        }
    }
}

// unary = ("+" | "-")? primary
Node *unary() {
    if (consume("+")) {
        return unary();
    }
    if (consume("-")) {
        return new_binary(ND_SUB, new_num(0), unary());
    }
    return primary();

}

// primary = num | ident | "(" expr ")"
Node *primary() {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok = consume_ident();
    if (tok) {
        Var *var = find_var(tok);
        if (!var) {
            // 新たな変数であれば locals に追加しておく
            // strndup は malloc で取得した領域に文字列をコピーする
            // NULL 終端となっているので変数名の長さは覚えなくていい
            var = push_var(strndup(tok->str, tok->len));
        }
        return new_var(var);
    }

    return new_num(expect_number());
}
