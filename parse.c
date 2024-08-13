#include <string.h>
#include "9cc.h"

VarList *locals;

// 今パースしている関数内で定義されている変数リストの中から tok を探す
Var *find_var(Token *tok) {
    for (VarList *vl = locals; vl; vl = vl->next) {
        Var *var = vl->var;
        if (strlen(var->name) == tok->len && !memcmp(tok->str, var->name, tok->len)) {
            return var;
        }
    }
    return NULL;
}

Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

Node *new_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Node *new_var(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

// 新しい変数のエントリを作り locals に足し、新しく作った変数を返す
Var *push_var(char *name) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
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

VarList *read_func_params() {
    // 引数なしなら NULL を返す
    if (consume(")")) {
        return NULL;
    }

    // 最初の1つは必ず存在するので、それを使って先頭要素を作る
    VarList *head = calloc(1, sizeof(VarList));
    head->var = push_var(expect_ident());
    VarList *cur = head;

    // 閉じ括弧がくるまで変数をパースしてリストにつなげていく
    while (!consume(")")) {
        expect(",");
        cur->next = calloc(1, sizeof(VarList));
        cur->next->var = push_var(expect_ident());
        cur = cur->next;
    }

    return head;
}

Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

// function = ident "(" params? ")" "{" stmt* "}"
// params   = ident ("," ident)*
Function *function() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = expect_ident();

    expect("(");
    fn->params = read_func_params();
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
    Token *tok;

    // return 文
    if (tok = consume("return")) {
        Node *node = new_unary(ND_RETURN, expr(), tok);
        expect(";");
        return node;
    }

    // if 文
    if (tok = consume("if")) {
        Node *node = new_node(ND_IF, tok);
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
    if (tok = consume("while")) {
        Node *node = new_node(ND_WHILE, tok);
        expect("(");
        node->cond = expr();
        expect(")");
        node->then = stmt();
        return node;
    }

    // for 文
    if (tok = consume("for")) {
        Node *node = new_node(ND_FOR, tok);
        expect("(");
        // 初期化部がからっぽの場合はなにも出力しない
        if (!consume(";")) {
            // 初期化部の評価結果は捨てる
            node->init = read_expr_stmt();
            expect(";");
        }
        if (!consume(";")) {
            // 条件部の結果はスタックトップに残す必要がある
            node->cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            // インクリメント部の評価結果は捨てる
            node->inc = read_expr_stmt();
            expect(")");
        }
        node->then = stmt();
        return node;
    }

    // ブロック
    if (tok = consume("{")) {
        Node head;
        head.next = NULL;
        Node *cur = &head;

        // 中身の複数文を順番にリストに入れていく
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    // 式のみからなる文
    Node *node = read_expr_stmt();

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
    Token *tok;

    if (tok = consume("=")) {
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
    Node *node = relational();
    Token *tok;

    for (;;) {
        if (tok = consume("==")) {
            node = new_binary(ND_EQ, node, relational(), tok);
        }
        else if (consume("!=")) {
            node = new_binary(ND_NE, node, relational(), tok);
        }
        else {
            return node;
        }
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
Node *relational() {
    Node *node = add();
    Token *tok;

    for (;;) {
        if (tok = consume("<")) {
            node = new_binary(ND_LT, node, add(), tok);
        }
        else if (tok = consume("<=")) {
            node = new_binary(ND_LE, node, add(), tok);
        }
        else if (tok = consume(">")) {
            node = new_binary(ND_LT, add(), node, tok);
        }
        else if (tok = consume(">=")) {
            node = new_binary(ND_LE, add(), node, tok);
        }
        else {
            return node;
        }
    }
}

// add = mul ("+" mul | "-" mul)*
Node *add() {
    Node *node = mul();
    Token *tok;

    for (;;) {
        if (tok = consume("+")) {
            node = new_binary(ND_ADD, node, mul(), tok);
        }
        else if (tok = consume("-")) {
            node = new_binary(ND_SUB, node, mul(), tok);
        }
        else {
            return node;
        }
    }
}

// mul = unary ("*" unary | "/" unary)*
Node *mul() {
    Node *node = unary();
    Token *tok;

    for (;;) {
        if (tok = consume("*")) {
            node = new_binary(ND_MUL, node, unary(), tok);
        }
        else if (tok = consume("/")) {
            node = new_binary(ND_DIV, node, unary(), tok);
        }
        else {
            return node;
        }
    }
}

// unary = ("+" | "-" | "*" | "&" )? primary
//       | primary
Node *unary() {
    Token *tok;

    if (consume("+")) {
        return unary();
    }
    if (tok = consume("-")) {
        return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
    }
    if (tok = consume("&")) {
        return new_unary(ND_ADDR, unary(), tok);
    }
    if (consume("*")) {
        return new_unary(ND_DEREF, unary(), tok);
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

    Token *tok;
    if (tok = consume_ident()) {
        if (consume("(")) {
            // 関数呼び出しである場合は関数名を控える
            Node *node = new_node(ND_FUNCALL, tok);
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
        return new_var(var, tok);
    }

    tok = token;
    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected expression");
    }
    return new_num(expect_number(), tok);
}
