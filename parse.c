#include <string.h>
#include "9cc.h"

VarList *locals;    // ローカル変数のリスト
VarList *globals;   // グローバル変数のリスト
VarList *scope;     // 今のスコープで定義されている変数のリスト

// 今パースしている関数のスコープ内で定義されている変数リストの中から tok を探す
Var *find_var(Token *tok) {
    for (VarList *vl = scope; vl; vl = vl->next) {
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

// 新しい変数のエントリを作って変数リストに足し、新しく作った変数を返す
Var *push_var(char *name, Type *ty, bool is_local) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;

    // ローカル変数かグローバル変数かで追加するリストを変える
    if (is_local) {
        vl->next = locals;
        locals = vl;
    }
    else {
        vl->next = globals;
        globals = vl;
    }

    // 新しい変数を追加したら、scope にも追加する
    VarList *sc = calloc(1, sizeof(VarList));
    sc->var = var;
    sc->next = scope;
    scope = sc;

    return var;
}

char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

Function *function();
Type *basetype();
Type *struct_decl();
Member *struct_member();
void global_var();
Node *declaration();
bool is_typename();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *postfix();
Node *primary();

bool is_functon() {
    // basetype や consume_ident などで先読みするとトークンを消費してしまう
    // 最初にトークンの位置を控えておく
    Token *tok = token;

    basetype();
    // 変数宣言か関数定義かは、識別子のあとに "(" が出てくるか見るまでわからない
    bool isfunc = consume_ident() && consume("(");

    // 読み進めてしまったトークンを戻す
    token = tok;
    return isfunc;
}

// program = (global-var | function)*
Program *program() {
    Function head;
    head.next = NULL;
    Function *cur = &head;
    globals = NULL;

    // プログラムは、グローバル変数の宣言か関数定義が複数並んだもの
    while (!at_eof()) {
        if (is_functon()) {
            cur->next = function();
            cur = cur->next;
        }
        else {
            global_var();
        }
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// basetype = ("char" | "int" | struct-decl) "*"*
// 型宣言を読み取る
Type *basetype() {
    if (!is_typename(token)) {
        error_tok(token, "typename expected");
    }

    Type *ty;

    if (consume("char")) {
        ty = char_type();
    }
    else if (consume("int")) {
        ty = int_type();
    }
    else {
        ty = struct_decl();
    }

    while (consume("*")) {
        ty = pointer_to(ty);
    }
    return ty;
}

// 型の後置修飾語(配列の括弧)をパース
Type *read_type_suffix(Type *base) {
    if (!consume("[")) {
        return base;
    }
    int sz = expect_number();
    expect("]");
    // さらに後ろをパースし、配列型とする
    base = read_type_suffix(base);
    return array_of(base, sz);
}

// struct-decl = "struct" "{" struct-member "}"
Type *struct_decl() {
    expect("struct");
    expect("{");

    Member head;
    head.next = NULL;
    Member *cur = &head;

    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    ty->members = head.next;

    // 構造体の各メンバに対してオフセットを計算する
    // todo: padding?
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        mem->offset = offset;
        offset += size_of(mem->ty);
    }

    return ty;
}

// struct-member = basetype ident ("[" num "]")* ";"
Member *struct_member() {
    Member *mem = calloc(1, sizeof(Member));
    mem->ty = basetype();
    mem->name = expect_ident();
    mem->ty = read_type_suffix(mem->ty);
    expect(";");
    return mem;
}

// 関数引数の宣言を1つ分読み取る
// e.g. "int *x[10]"
VarList *read_func_param() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = push_var(name, ty, true);
    return vl;
}

VarList *read_func_params() {
    // 引数なしなら NULL を返す
    if (consume(")")) {
        return NULL;
    }

    // 最初の1つは必ず存在するので、それを使って先頭要素を作る
    VarList *head = read_func_param();
    VarList *cur = head;

    // 閉じ括弧がくるまで変数をパースしてリストにつなげていく
    while (!consume(")")) {
        expect(",");
        cur->next = read_func_param();
        cur = cur->next;
    }

    return head;
}

// function = ident "(" params? ")" "{" stmt* "}"
// params   = param ("," param)*
// param    = basetype ident
Function *function() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    // 関数の戻り値の型、今は単に無視するだけ
    basetype();
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

// global-var = basetype ident ( "[" num "]" )* "+"
void global_var() {
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration = basetype ident ("[" num "]")* ("=" expr)? ";"
Node *declaration() {
    Token *tok = token;
    Type *ty = basetype();
    char *name = expect_ident();
    ty = read_type_suffix(ty);
    Var *var = push_var(name, ty, true);

    // 初期値のない変数宣言はからっぽの文になる
    if (consume(";")) {
        return new_node(ND_NULL, tok);
    }

    // 初期値がある場合は代入文になる
    expect("=");
    Node *lhs = new_var(var, tok);
    Node *rhs = expr();
    expect(";");
    Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
    return new_unary(ND_EXPR_STMT, node, tok);
}

bool is_typename() {
    return peek("char") || peek("int") || peek("struct");
}

Node *read_expr_stmt() {
    Token *tok = token;
    return new_unary(ND_EXPR_STMT, expr(), tok);
}

// stmt = expr ";"
//      | "{" stmt* "}"
//      | "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "while" "(" expr ")" stmt
//      | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//      | declaration
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

        // ブロックの中だけで有効な変数が定義されるかもしれないので
        // 今の scope を控えておく
        // ブロック内をパースしている間は一時的にリストが伸びることになる
        VarList *sc = scope;
        // 中身の複数文を順番にリストに入れていく
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        // ブロック内で定義されていた変数を忘れるため、scope を戻す
        scope = sc;

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (is_typename()) {
        return declaration();
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
    return postfix();
}

// postfix = primary ( "[" expr "]" | "." ident)*
Node *postfix() {
    Node *node = primary();
    Token *tok;

    for (;;) {
        if (tok = consume("[")) {
            // x[y] は *(x+y) と同じ
            Node *exp = new_binary(ND_ADD, node, expr(), tok);
            expect("]");
            node = new_unary(ND_DEREF, exp, tok);
            continue;
        }

        // 構造体のメンバアクセス時は member_name にメンバ名を入れる
        if (tok = consume(".")) {
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        return node;
    }
}

// stmt-expr = stmt* "}" ")"
Node *stmt_expr(Token *tok) {
    VarList *sc = scope;

    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = stmt();
    Node *cur = node->body;

    // primary のほうで "(" "{" はパースしているので、stmt のパースから始めればいい
    // 複数の statement をパースして body につなげていく
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    expect(")");

    scope = sc;

    // 最後の文は値を返さないといけない
    if (cur->kind != ND_EXPR_STMT) {
        error_tok(cur->tok, "statement expression returning void is not supported");
    }
    // 最後の文の中身の式を取り出して持ち上げる
    // 構造体の代入なので値がすべてコピーされる
    *cur = *cur->lhs;
    return node;
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

// todo: なぜ sizeof の lhs は primary ではなく unary？
// primary = "(" "{" stmt-expr-tail
//         | "(" expr ")"
//         | "sizeof" unary
//         | ident func-args?
//         | str
//         | num
Node *primary() {
    Token *tok;

    if (tok = consume("(")) {
        if (consume("{")) {
            return stmt_expr(tok);
        }

        Node *node = expr();
        expect(")");
        return node;
    }

    if (tok = consume("sizeof")) {
        return new_unary(ND_SIZEOF, unary(), tok);
    }

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
            // 宣言されていない変数の場合はエラー
            error_tok(tok, "undefined variable");
        }
        return new_var(var, tok);
    }

    tok = token;
    if (tok->kind == TK_STR) {
        // expect などでトークンを消費できないので直接進める
        token = token->next;

        // 文字列は char の配列
        Type *ty = array_of(char_type(), tok->cont_len);
        // 変数ではなくラベルを登録する
        // ラベルはコード生成時にラベルとして使われる
        Var *var = push_var(new_label(), ty, false);
        var->contents = tok->contents;
        var->cont_len = tok->cont_len;
        // 文字列リテラルは変数として扱う
        return new_var(var, tok);
    }

    if (tok->kind != TK_NUM) {
        error_tok(tok, "expected expression");
    }
    return new_num(expect_number(), tok);
}
