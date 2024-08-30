#include <string.h>
#include "9cc.h"

// ローカル変数、グローバル変数、typedef が登録される
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    Var *var;
    Type *type_def;
};

// 構造体定義が登録される
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    Type *ty;
};

VarList *globals;       // グローバル変数のリスト
VarList *locals;        // ローカル変数のリスト

VarScope *var_scope;    // 今のスコープで定義されている変数のリスト
TagScope *tag_scope;    // 今のスコープで定義されているタグのリスト

// 今パースしている関数のスコープ内で定義されている変数と typedef のリストの中から
// tok を探す
VarScope *find_var(Token *tok) {
    for (VarScope *sc = var_scope; sc; sc = sc->next) {
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len)) {
            return sc;
        }
    }

    return NULL;
}

// 今パースしている関数のスコープ内で定義されているタグリストの中から tok を探す
TagScope *find_tag(Token *tok) {
    for (TagScope *sc = tag_scope; sc; sc = sc->next) {
        if (strlen(sc->name) == tok->len && !memcmp(tok->str, sc->name, tok->len)) {
            return sc;
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

VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    var_scope = sc;
    return sc;
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
    VarScope *sc = push_scope(name);
    sc->var = var;

    return var;
}

Type *find_typedef(Token *tok) {
    if (tok->kind == TK_IDENT) {
        VarScope *sc = find_var(token);
        if (sc) {
            return sc->type_def;
        }
    }
    return NULL;
}

char *new_label() {
    static int cnt = 0;
    char buf[20];
    sprintf(buf, ".L.data.%d", cnt++);
    return strndup(buf, 20);
}

Function *function();
Type *type_specifier();
Type *declarator(Type *ty, char **name);
Type *type_suffix(Type *ty);
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
    // type_specifier や consume_ident などで先読みするとトークンを消費してしまう
    // 最初にトークンの位置を控えておく
    Token *tok = token;

    Type *ty = type_specifier();
    char *name = NULL;
    declarator(ty, &name);
    // 変数宣言か関数定義かは、識別子のあとに "(" が出てくるか見るまでわからない
    bool isfunc = name && consume("(");

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

// type-specifier = builtin-type | struct-decl | typedef-name
// buildin-type = "char" | "short" | "int" | "long"
// 型宣言を読み取る
Type *type_specifier() {
    if (!is_typename(token)) {
        error_tok(token, "typename expected");
    }

    if (consume("char")) {
        return char_type();
    }
    else if (consume("short")) {
        return short_type();
    }
    else if (consume("int")) {
        return int_type();
    }
    else if (consume("long")) {
        return long_type();
    }
    else if (consume("struct")) {
        return struct_decl();
    }
    return find_var(consume_ident())->type_def;
}

// declarator = "*"* ( "(" declarator ")" | ident )
Type *declarator(Type *ty, char **name) {
    // 1. int *x -> pointer_to(int)
    // 2. int *x[3] -> array_of(pointer_to(int))
    // 3. int (*x)[3]
    //      -> *x を declarator で再帰パース、ただしプレースホルダあり
    //      -> pointer_to(placeholder)
    //      -> その後 [3] を type_suffix でパースしてプレースホルダにいれる
    //      -> placeholder = array_of(int)
    //      -> pointer_to(array_of(int))

    while (consume("*")) {
        ty = pointer_to(ty);
    }

    if (consume("(")) {
        // ネストした型定義を深さ優先でパースするため、いったんプレースホルダを作る
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = declarator(placeholder, name);
        expect(")");
        // プレースホルダに値としてコピー
        *placeholder = *type_suffix(ty);
        return new_ty;
    }

    *name = expect_ident();
    return type_suffix(ty);
}

// type-suffix = ( "[" num "]" type-suffix)?
// 型の後置修飾語(配列の括弧)をパース
Type *type_suffix(Type *ty) {
    if (!consume("[")) {
        return ty;
    }
    int sz = expect_number();
    expect("]");
    // さらに後ろをパースし、配列型とする
    ty = type_suffix(ty);
    return array_of(ty, sz);
}

void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident
//             | "struct" "{" struct-member "}"
Type *struct_decl() {
    // "struct" のあとに識別子があり次のトークンが "{" でなければ struct tag
    // e.g., struct Position p;
    Token *tag = consume_ident();
    if (tag && !peek("{")) {
        TagScope *sc = find_tag(tag);
        if (!sc) {
            error_tok(tag, "unknown struct type");
        }
        return sc->ty;
    }

    // "struct" のあとに識別子がなかった、もしくは識別子の次のトークンが
    //  "{" の場合は、構造体の定義
    // e.g., struct { int x; int y; }
    //      struct Position { int x; int y; }
    expect("{");

    Member head;
    head.next = NULL;
    Member *cur = &head;

    // メンバの定義をパース
    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    ty->members = head.next;

    // 構造体の各メンバに対してオフセットを計算する
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        // アライメントを調整したオフセットを計算
        // C の構造体では、変数のサイズの倍数の境界に合わせて配置する必要がある
        // よってメンバの型のサイズの倍数の位置に合わせる
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += size_of(mem->ty);

        // 構造体自身のアラインメントは、メンバの最大サイズになる
        // 構造体が配列になったときへの対応のため、
        // 最後のメンバの後ろにパディングが入ることもある
        if (ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }

    // 構造体名(タグ)が宣言されていた場合はタグリストに登録する
    // struct Position { int x; int y; }
    // のとき、{int x; int y;} という構造体型を Position というタグで登録する
    if (tag) {
        push_tag_scope(tag, ty);
    }
    return ty;
}

// struct-member = type-specifier declarator type-suffix ";"
Member *struct_member() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name =name;
    mem->ty = ty;
    return mem;
}

// 関数引数の宣言を1つ分読み取る
// e.g., "int *x[10]"
VarList *read_func_param() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

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

// function = type-specifier declarator "(" params? ")" "{" stmt* "}"
// params   = param ("," param)*
// param    = type-specifier declarator type-suffix
Function *function() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    Type *ty = type_specifier();
    char *name = NULL;
    declarator(ty, &name);

    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;

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

// global-var = type-specifier declarator type-suffix ";"
void global_var() {
    Type *ty = type_specifier();
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");
    push_var(name, ty, false);
}

// declaration = type-specifier declarator type-suffix ("=" expr)? ";"
//             | type-specifier ";"
Node *declaration() {
    Token *tok = token;
    Type *ty = type_specifier();

    // 型の定義だけあり変数がない場合は、構造体のタグ登録だけを意図している
    // type_specifier によるパースで型が登録され目的を達成しているので NULL ノードにする
    if (consume(";")) {
        return new_node(ND_NULL, tok);
    }

    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
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
    return peek("char") || peek("short") || peek("int") || peek("long") ||
           peek("struct") || find_typedef(token);
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
//      | "typedef" type-specifier declarator type-suffix ";"
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
        VarScope *sc1 = var_scope;
        TagScope *sc2 = tag_scope;
        // 中身の複数文を順番にリストに入れていく
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        // ブロック内で定義されていた変数を忘れるため、scope を戻す
        var_scope = sc1;
        tag_scope = sc2;

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (tok = consume("typedef")) {
        Type *ty = type_specifier();
        char *name = NULL;
        ty = declarator(ty, &name);
        ty = type_suffix(ty);
        expect(";");
        VarScope *sc = push_scope(name);
        sc->type_def = ty;
        // 型に別名をつける
        // 動作に影響はないのでパース結果としては空ノードにする
        return new_node(ND_NULL, tok);
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

// postfix = primary ( "[" expr "]" | "." ident | "->" ident)*
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

        // アロー演算子は、ポインタを deref した上でメンバアクセスする
        // e.g., pos->x == (*pos).x
        if (tok = consume("->")) {
            node = new_unary(ND_DEREF, node, tok);
            node = new_unary(ND_MEMBER, node, tok);
            node->member_name = expect_ident();
            continue;
        }

        return node;
    }
}

// stmt-expr = stmt* "}" ")"
Node *stmt_expr(Token *tok) {
    VarScope *sc1 = var_scope;
    TagScope *sc2 = tag_scope;

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

    var_scope = sc1;
    tag_scope = sc2;

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
        VarScope *sc = find_var(tok);
        if (sc && sc->var) {
            return new_var(sc->var, tok);
        }
        // 識別子が見つからないか、見つかっても typedef された値だったらエラー
        error_tok(tok, "undefined variable");
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
