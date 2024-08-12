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

Function *function();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// program = function*
Function *program() {
    Function head;
    head.next = NULL;
    Function *cur = &head;

    // プログラムは、シンプルに関数定義が複数並んだもの
    while (!at_eof()) {
        cur->next = function();
        cur = cur->next;
    }

    return head.next;
}

// function = ident "(" ")" "{" stmt* "}"
Function *function() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    char *name = expect_ident();
    expect("(");
    expect(")");
    expect("{");

    Node head;
    head.next = NULL;
    Node *cur = &head;

    // 複数の文を前から順番にリストに追加していく
    while (!consume("}")) {
        // 関数の中身は複数の stmt からなる
        cur->next = stmt();
        cur = cur->next;
    }

    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;
    // head はダミーのノードなので、その次のノードから使う
    fn->node = head.next;
    // パース中に作ったローカル変数一覧をそのまま渡す
    // calloc してあるのでこの関数を抜けても問題ない
    fn->locals = locals;
    return fn;
}

// stmt = expr ";"
//      | "{" stmt* "}"
//      | "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
Node *stmt() {
    // return 文
    if (consume("return")) {
        Node *node = new_unary(ND_RETURN, expr());
        expect(";");
        return node;
    }

    // if 文
    if (consume("if")) {
        Node *node = new_node(ND_IF);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        if (consume("else")) {
            node->els = stmt();
        }
        return node;
    }

    // while 文
    if (consume("while")) {
        Node *node = new_node(ND_WHILE);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    // for 文
    if (consume("for")) {
        Node *node = new_node(ND_FOR);
        expect("(");
        // 初期化部がからっぽの場合はなにも出力しない
        if (!consume(";")) {
            // 初期化部の評価結果は捨てる
            node->init = new_unary(ND_EXPR_STMT, expr());
            expect(";");
        }
        if (!consume(";")) {
            // 条件部の結果はスタックトップに残す必要がある
            node->cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            // インクリメント部の評価結果は捨てる
            node->inc = new_unary(ND_EXPR_STMT, expr());
            expect(")");
        }
        node->then = stmt();
        return node;
    }

    // ブロック
    if (consume("{")) {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        // 中身の複数文を順番にリストに入れていく
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK);
        node->body = head.next;
        return node;
    }

    // 式のみからなる文
    Node *node = new_unary(ND_EXPR_STMT, expr());

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

// func-args = "(" (assign ("," assign)*)? ")"
Node *func_args() {
    // ただの変数か関数呼び出しかを判断するため、既に "(" は消費されている
    // すぐに ")" がきたら引数なしの関数呼び出し
    if (consume(")")) {
        return NULL;
    }

    Node *head = assign();
    Node *cur = head;
    while (consume(",")) {
        cur->next = assign();
        cur = cur->next;
    }
    expect(")");
    return head;
}

// primary = num | ident func-args? | "(" expr ")"
Node *primary() {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node;
    }

    Token *tok = consume_ident();
    if (tok) {
        if (consume("(")) {
            // 関数呼び出しである場合は関数名を控える
            Node *node = new_node(ND_FUNCALL);
            node->funcname = strndup(tok->str, tok->len);
            // 関数呼び出し時の引数をパース
            node->args = func_args();
            return node;
        }

        // 関数呼び出しでない場合は通常の変数
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
