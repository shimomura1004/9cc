#include <string.h>
#include "9cc.h"

// ローカル変数、グローバル変数、typedef、enum が登録される
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *next;
    char *name;
    int depth;
    Var *var;
    Type *type_def;
    Type *enum_ty;
    int enum_val;
};

// 構造体定義が登録される
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *next;
    char *name;
    int depth;
    Type *ty;
};

typedef struct {
    VarScope *var_scope;
    TagScope *tag_scope;
} Scope;

VarList *globals;       // グローバル変数のリスト
VarList *locals;        // ローカル変数のリスト

VarScope *var_scope;    // 今のスコープで定義されている変数のリスト
TagScope *tag_scope;    // 今のスコープで定義されているタグのリスト
int scope_depth;        // todo: 今のスコープのネストの深さ

Scope *enter_scope() {
    Scope *sc = calloc(1, sizeof(Scope));
    sc->var_scope = var_scope;
    sc->tag_scope = tag_scope;
    ++scope_depth;
    return sc;
}

void leave_scope(Scope *sc) {
    var_scope = sc->var_scope;
    tag_scope = sc->tag_scope;
    --scope_depth;
}

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

Node *new_num(long val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

Node *new_var(Var *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

// スコープに変数を追加する
VarScope *push_scope(char *name) {
    VarScope *sc = calloc(1, sizeof(VarScope));
    sc->name = name;
    sc->next = var_scope;
    sc->depth = scope_depth;
    var_scope = sc;
    return sc;
}

// 新しい変数のエントリを作って変数リストに足し、新しく作った変数を返す
Var *push_var(char *name, Type *ty, bool is_local, Token *tok) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;
    var->is_local = is_local;
    var->tok = tok;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;

    // ローカル変数かグローバル変数かで追加するリストを変える
    // 関数宣言は変数のスコープには入れない
    if (is_local) {
        vl->next = locals;
        locals = vl;
    }
    else if (ty->kind != TY_FUNC) {
        vl->next = globals;
        globals = vl;
    }

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
Type *abstract_declarator(Type *ty);
Type *type_suffix(Type *ty);
Type *type_name();
Type *struct_decl();
Type *enum_specifier();
Member *struct_member();
void global_var();
Node *declaration();
bool is_typename();
Node *stmt();
Node *expr();
Node *assign();
Node *logor();
Node *logand();
Node *bitand();
Node *bitor();
Node *bitxor();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *cast();
Node *unary();
Node *postfix();
Node *primary();

bool is_function() {
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
        if (is_function()) {
            Function *fn = function();
            if (!fn) {
                continue;
            }
            cur->next = fn;
            cur = cur->next;
            continue;
        }

        // グローバル変数の定義であって、現状では型定義のみの宣言はできない
        // 関数定義の中の declaration は型定義のみの宣言に対応している
        global_var();
    }

    Program *prog = calloc(1, sizeof(Program));
    prog->globals = globals;
    prog->fns = head.next;
    return prog;
}

// type-specifier = builtin-type | struct-decl | typedef-name | enum-specifier
// buildin-type = "void"
//              | "_Bool"
//              | "char"
//              | "short" | "short" "int" | "int" "short"
//              | "int"
//              | "long" | "long" "int" | "int" "long"
// "typedef" と "static" は type-specifier 内のどこにでも現れる
// 型宣言を読み取る
Type *type_specifier() {
    if (!is_typename(token)) {
        error_tok(token, "typename expected");
    }

    Type *ty = NULL;

    // 1ビットあけているのは、たとえば int int x が valid にならないようにするため
    // 隙間がない場合は、int を読んだあと、次にまた int を読むと
    // long が1回現れたと解釈されてしまう
    // 1ビットあけることで default に飛ぶようになっている
    enum {
        VOID  = 1 << 1,
        BOOL  = 1 << 3,
        CHAR  = 1 << 5,
        SHORT = 1 << 7,
        INT   = 1 << 9,
        LONG  = 1 << 11,
    };

    int base_type = 0;
    Type *user_type = NULL;

    bool is_typedef = false;
    bool is_static = false;

    for (;;) {
        Token *tok = token;

        if (consume("typedef")) {
            is_typedef = true;
        }
        else if (consume("static")) {
            is_static = true;
        }
        else if (consume("void")) {
            base_type += VOID;
        }
        else if (consume("_Bool")) {
            base_type += BOOL;
        }
        else if (consume("char")) {
            base_type += CHAR;
        }
        else if (consume("short")) {
            base_type += SHORT;
        }
        else if (consume("int")) {
            base_type += INT;
        }
        else if (consume("long")) {
            base_type += LONG;
        }
        else if (peek("struct")) {
            // 既になんらか型のトークン列を読んでいたら抜ける
            // int struct {...} というような場合は int だけ読んで抜ける
            // そのあと識別子がくることが期待されるのでパースエラーになるが…
            if (base_type || user_type) {
                break;
            }
            // struct から始まっているのであれば構造体の宣言としてパース
            user_type = struct_decl();
        }
        else if (peek("enum")) {
            // 構造体と同様に、既になんらかの型のトークン列を読んでいたら抜ける
            if (base_type || user_type) {
                break;
            }
            user_type = enum_specifier();
        }
        else {
            // 型宣言以外のなにかがきた場合
            // 既になんらか型のトークン列を読んでいたら抜ける
            if (base_type || user_type) {
                break;
            }
            // 今のトークンが typedef された型かどうかを確認
            Type *ty = find_typedef(token);
            if (!ty) {
                // 型の名前じゃなかったらパースを終了
                break;
            }
            // 型の名前だったら break せずループを続ける
            token = token->next;
            user_type = ty;
        }

        switch (base_type) {
        case VOID:
            ty = void_type();
            break;
        case BOOL:
            ty = bool_type();
            break;
        case CHAR:
            ty = char_type();
            break;
        case SHORT:
        case SHORT + INT:
            ty = short_type();
            break;
        case INT:
            ty = int_type();
            break;
        case LONG:
        case LONG + INT:
            ty = long_type();
            break;
        case 0:
            // type-specifier がない場合は int になる
            // typedef x; だけだと、x は int のエイリアスになる
            ty = user_type ? user_type : int_type();
            break;
        default:
            error_tok(tok, "invalid type");
        }
    }

    ty->is_typedef = is_typedef;
    ty->is_static = is_static;
    return ty;
}

// declarator = "*"* ( "(" declarator ")" | ident )
// 型の記述と、その型を持つ識別子のセットをパース
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

// abstract-declarator = "*"* ( "(" abstract-declarator ")" )? type-suffix
// 識別子のない型の記述をパース
Type *abstract_declarator(Type *ty) {
    while (consume("*")) {
        ty = pointer_to(ty);
    }

    if (consume("(")) {
        Type *placeholder = calloc(1, sizeof(Type));
        Type *new_ty = abstract_declarator(placeholder);
        expect(")");
        // 後ろの配列宣言のところまでパースしたあと、プレースホルダを差し替え
        *placeholder = *type_suffix(ty);
        return new_ty;
    }

    return type_suffix(ty);
}

// type-suffix = ( "[" num? "]" type-suffix)?
// 型の後置修飾語(配列の括弧)をパース(配列の要素数がない場合もある)
Type *type_suffix(Type *ty) {
    if (!consume("[")) {
        return ty;
    }

    int sz = 0;
    bool is_incomplete = true;
    if (!consume("]")) {
        sz = expect_number();
        is_incomplete = false;
        expect("]");
    }
    // 配列の要素数がない場合は incomplete である

    // さらに後ろをパースし、配列型とする
    ty = type_suffix(ty);
    ty = array_of(ty, sz);
    ty->is_incomplete = is_incomplete;
    return ty;
}

// type-name = type-specifier abstract-declarator type-suffix
// type-declarator ではなく abstract-declarator になっている
// 識別子なしの型宣言部だけを
Type *type_name() {
    Type *ty = type_specifier();
    ty = abstract_declarator(ty);
    return type_suffix(ty);
}

// スコープに型名を追加する
void push_tag_scope(Token *tok, Type *ty) {
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->next = tag_scope;
    sc->name = strndup(tok->str, tok->len);
    sc->depth = scope_depth;
    sc->ty = ty;
    tag_scope = sc;
}

// struct-decl = "struct" ident? ("{" struct-member "}")?
Type *struct_decl() {
    expect("struct");
    Token *tag = consume_ident();

    if (tag && !peek("{")) {
        // "struct" のあとに識別子があり、次のトークンが "{" でない場合は struct tag
        // e.g., struct Position p;

        // その場合は、tag という名前の構造体が定義されているかを調べる
        TagScope *sc = find_tag(tag);

        if (!sc) {
            // まだ定義されていない構造体型を使っている場合は
            // とりあえず登録だけしてしまう
            Type *ty = struct_type();
            push_tag_scope(tag, ty);
            return ty;
        }

        if (sc->ty->kind != TY_STRUCT) {
            // スコープ内に型名は見つかったけど、構造体型ではなかったらエラー
            error_tok(tag, "not a struct tag");
        }

        // 普通に定義済みの構造体型が見つかった場合はそれを返す
        return sc->ty;
    }

    // "struct" のあとに識別子がなかった、
    // もしくは識別子はあったけど次のトークンが "{" の場合は、構造体の定義である
    // e.g., struct { int x; int y; }
    //       struct Position { int x; int y; }
    // ただ "struct *foo" は正しい C の定義で、foo は未定義の構造体型へのポインタ型となる
    if (!consume("{")) {
        // 識別子はなく、次のトークンが "{" でもない、つまりなんの構造体を指しているかわからない
        // 構造体名が指定されないけどポインタとして変数定義されている場合は
        // とりあえず構造体として扱う
        //   struct *foo の場合、struct までをパースして無名の構造体型になる
        //   その後 *foo がパースされて無名の構造体へのポインタ型となる
        return struct_type();
    }

    // 構造体名が書かれている場合は探す
    TagScope *sc = find_tag(tag);
    Type *ty;

    if (sc && sc->depth == scope_depth) {
        // 同じ階層に同じ型名が構造体以外として定義されていたら再定義エラー
        if (sc->ty->kind != TY_STRUCT) {
            error_tok(tag, "not a struct tag");
        }
        // 名前が被っていても構造体だったら問題なし
        // スコープに登録されている型のデータを取り出し、
        // メンバ定義部をパースして型の定義を完成させる
        ty = sc->ty;
    }
    else {
        // 同じ名前の型が定義されていなかった場合は新たに定義される構造体型である
        // とりあえず名前だけ登録してメンバ定義に進む
        ty = struct_type();
        if (tag) {
            push_tag_scope(tag, ty);
        }
    }

    Member head;
    head.next = NULL;
    Member *cur = &head;

    // メンバの定義をパース
    while (!consume("}")) {
        cur->next = struct_member();
        cur = cur->next;
    }

    ty->members = head.next;

    // 構造体の各メンバに対してオフセットを計算する
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next) {
        // アライメントを調整したオフセットを計算
        // C の構造体では、変数のサイズの倍数の境界に合わせて配置する必要がある
        // よってメンバの型のサイズの倍数の位置に合わせる
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += size_of(mem->ty, mem->tok);

        // 構造体自身のアラインメントは、メンバの最大サイズになる
        // 構造体が配列になったときへの対応のため、
        // 最後のメンバの後ろにパディングが入ることもある
        if (ty->align < mem->ty->align) {
            ty->align = mem->ty->align;
        }
    }

    // メンバ定義のパースまで終わったら構造体の定義は完了なので incomplete フラグは消す
    ty->is_incomplete = false;

    return ty;
}

// enum-specifier = "enum" ident
//                | "enum" ident? "{" enum-list? "}"
// enum-list = ident ("=" num)? ("," ident ("=" num)?)* ","?
Type *enum_specifier() {
    expect("enum");
    Type *ty = enum_type();

    // enum の型名が書いてあって、かつその後ろに開きかっこがないのであれば
    // enum の定義ではなく、その enum 型の変数の定義をしている
    // e.g., enum Color {red, blue}; enum Color c;
    Token *tag = consume_ident();
    if (tag && !peek("{")) {
        // スコープに enum の定義があるはず
        TagScope *sc = find_tag(tag);
        if (!sc) {
            error_tok(tag, "unknown enum type");
        }
        if (sc->ty->kind != TY_ENUM) {
            error_tok(tag, "not an enum tag");
        }
        return sc->ty;
    }

    // 開きかっこがある場合は enum の定義
    expect("{");

    // enum-list のパース
    int cnt = 0;
    for (;;) {
        char *name = expect_ident();
        // enum の定数値に値が指定されている場合
        if (consume("=")) {
            cnt = expect_number();
        }

        // enum の定数名をスコープに加える
        VarScope *sc = push_scope(name);
        sc->enum_ty = ty;
        sc->enum_val = cnt++;

        if (consume(",")) {
            if (consume("}")) {
                break;
            }
            continue;
        }

        expect("}");
        break;
    }

    // enum の型名をスコープに加える
    if (tag) {
        push_tag_scope(tag, ty);
    }
    return ty;
}

// struct-member = type-specifier declarator type-suffix ";"
Member *struct_member() {
    Type *ty = type_specifier();
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Member *mem = calloc(1, sizeof(Member));
    mem->name =name;
    mem->ty = ty;
    mem->tok = tok;
    return mem;
}

// 関数引数の宣言を1つ分読み取る
// e.g., "int *x[10]"
VarList *read_func_param() {
    Type *ty = type_specifier();
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    // 関数引数に限り、"型 T の配列" は "型 T へのポインタ" に変換される
    // ちなみに int f(int x[3]) {return x[0];} は文法的に正しい
    if (ty->kind == TY_ARRAY) {
        ty = pointer_to(ty->base);
    }

    Var *var = push_var(name, ty, true, tok);
    push_scope(name)->var = var;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
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

// function = type-specifier declarator "(" params? ")" ( "{" stmt* "}" | ";")
// params   = param ("," param)*
// param    = type-specifier declarator type-suffix
Function *function() {
    // パース中に使う変数の辞書をクリア
    locals = NULL;

    Type *ty = type_specifier();
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);

    // 関数の名前と型の組み合わせをスコープに追加する
    Var *var = push_var(name, func_type(ty), false, tok);
    push_scope(name)->var = var;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = name;

    expect("(");
    fn->params = read_func_params();

    if (consume(";")) {
        return NULL;
    }

    // 関数の本体をパース
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
    Token *tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);
    expect(";");

    Var *var = push_var(name, ty, false, tok);
    push_scope(name)->var = var;
}

// declaration = type-specifier declarator type-suffix ("=" expr)? ";"
//             | type-specifier ";"
Node *declaration() {
    Token *tok;
    Type *ty = type_specifier();

    // 型の定義だけあり変数がない場合は、構造体/列挙型のタグ登録だけを意図している
    // type_specifier によるパースで型が登録され目的を達成しているので NULL ノードにする
    if (tok = consume(";")) {
        return new_node(ND_NULL, tok);
    }

    tok = token;
    char *name = NULL;
    ty = declarator(ty, &name);
    ty = type_suffix(ty);

    if (ty->is_typedef) {
        // type-specifier のパースの結果、typedef であった場合
        // typedef でいきなり変数宣言はできないので ";" がこないといけない
        // typedef int Integer x; は無理
        expect(";");
        ty->is_typedef = false;
        push_scope(name)->type_def = ty;
        return new_node(ND_NULL, tok);
    }

    if (ty->kind == TY_VOID) {
        // void 型の変数は宣言できない
        // todo: まだ戻り値のない関数(void 関数)は扱えない
        error_tok(tok, "variable declared void");
    }

    Var *var;
    if (ty->is_static) {
        // ブロック内で static 変数が宣言される場合
        // ブロック内の static 変数とは、スコープがブロック内だけど関数を抜けても解放されない
        // すなわちグローバル変数と同じ位置に領域を確保する必要がある
        // なのでグローバル変数として登録
        var = push_var(new_label(), ty, false, tok);
    }
    else {
        // static じゃないなら普通にブロックスコープの変数として登録
        var = push_var(name, ty, true, tok);
    }
    // 変数を今のスコープに追加する (グローバルスコープには追加しない)
    push_scope(name)->var = var;

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
    return peek("void") || peek("_Bool") ||
           peek("char") || peek("short") || peek("int") || peek("long") ||
           peek("enum") || peek("struct") || peek("typedef") || peek("static") ||
           find_typedef(token);
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
//      | "for" "(" ( expr? ";" | declaration ) expr? ";" expr? ")" stmt
//      | "break" ";"
//      | "continue" ";"
//      | "goto" ident ";"
//      | ident ":" stmt
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
        Scope *sc = enter_scope();

        // 初期化部がからっぽの場合はなにも出力しない
        if (!consume(";")) {
            if (is_typename()) {
                // 変数宣言が始まった場合
                // declaration には末尾の ";" のパースまで含まれている
                node->init = declaration();
            }
            else {
                // ただの代入文の場合
                // 初期化部の評価結果は捨てる
                node->init = read_expr_stmt();
                expect(";");
            }
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

        // for 文を抜けたあとはスコープを元に戻す
        leave_scope(sc);
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
        Scope *sc = enter_scope();
        // 中身の複数文を順番にリストに入れていく
        while (!consume("}")) {
            cur->next = stmt();
            cur = cur->next;
        }
        // ブロック内で定義されていた変数を忘れるため、scope を戻す
        leave_scope(sc);

        Node *node = new_node(ND_BLOCK, tok);
        node->body = head.next;
        return node;
    }

    if (tok = consume("break")) {
        expect(";");
        return new_node(ND_BREAK, tok);
    }

    if (tok = consume("continue")) {
        expect(";");
        return new_node(ND_CONTINUE, tok);
    }

    if (tok = consume("goto")) {
        Node *node = new_node(ND_GOTO, tok);
        node->label_name = expect_ident();
        expect(";");
        return node;
    }

    if (tok = consume_ident()) {
        if (consume(":")) {
            Node *node = new_unary(ND_LABEL, stmt(), tok);
            node->label_name = strndup(tok->str, tok->len);
            return node;
        }
        // 識別子のあとに ":" がなかった場合(ラベルでなかった場合)は読んでしまったトークンを戻す
        // 戻さないと、識別子で始まる式がパースできなくなる
        // e.g., x = x + 1;
        // 最初の x が consume_ident で消費されてしまう
        token = tok;
    }

    if (is_typename()) {
        return declaration();
    }

    // 式のみからなる文
    Node *node = read_expr_stmt();

    expect(";");
    return node;
}

// expr = assign ( "," assign)*
Node *expr() {
    Node *node = assign();
    Token *tok;
    while (tok = consume(",")) {
        node = new_unary(ND_EXPR_STMT, node, node->tok);
        node = new_binary(ND_COMMA, node, assign(), tok); 
    }
    return node;
}

// assign    = logor (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/="
Node *assign() {
    Node *node = logor();
    Token *tok;

    if (tok = consume("=")) {
        node = new_binary(ND_ASSIGN, node, assign(), tok);
    }
    if (tok = consume("+=")) {
        node = new_binary(ND_A_ADD, node, assign(), tok);
    }
    if (tok = consume("-=")) {
        node = new_binary(ND_A_SUB, node, assign(), tok);
    }
    if (tok = consume("*=")) {
        node = new_binary(ND_A_MUL, node, assign(), tok);
    }
    if (tok = consume("/=")) {
        node = new_binary(ND_A_DIV, node, assign(), tok);
    }

    return node;
}

// logor = logand ("||" logand)*
Node *logor() {
    Node *node = logand();
    Token *tok;
    while (tok = consume("||")) {
        node = new_binary(ND_LOGOR, node, logand(), tok);
    }
    return node;
}

// logand = bitor ("&&" bitor)*
Node *logand() {
    Node *node = bitor();
    Token *tok;
    while (tok = consume("&&")) {
        node = new_binary(ND_LOGAND, node, bitor(), tok);
    }
    return node;
}

// bitor = bitxor ("|" bitxor)*
Node *bitor() {
    Node *node = bitxor();
    Token *tok;
    while (tok = consume("|")) {
        node = new_binary(ND_BITOR, node, bitxor(), tok);
    }
    return node;
}

// bitxor = bitand ("^" bitand)*
Node *bitxor() {
    Node *node = bitand();
    Token *tok;
    while (tok = consume("^")) {
        node = new_binary(ND_BITXOR, node, bitand(), tok);
    }
    return node;
}

// bitand = equality ("&" equality)*
Node *bitand() {
    Node *node = equality();
    Token *tok;
    while (tok = consume("&")) {
        node = new_binary(ND_BITAND, node, equality(), tok);
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

// mul = cast ("*" cast | "/" cast)*
Node *mul() {
    Node *node = cast();
    Token *tok;

    for (;;) {
        if (tok = consume("*")) {
            node = new_binary(ND_MUL, node, cast(), tok);
        }
        else if (tok = consume("/")) {
            node = new_binary(ND_DIV, node, cast(), tok);
        }
        else {
            return node;
        }
    }
}

// cast = "(" type-name ")" cast | unary
Node *cast() {
    Token *tok = token;

    if (consume("(")) {
        if (is_typename()) {
            Type *ty = type_name();
            expect(")");
            Node *node = new_unary(ND_CAST, cast(), tok);
            node->ty = ty;
            return node;
        }
        // キャストでなかったら戻す
        token = tok;
    }

    return unary();
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~")? cast
//       | ("++" | "--") unary
//       | postfix
Node *unary() {
    Token *tok;

    if (consume("+")) {
        return cast();
    }
    if (tok = consume("-")) {
        return new_binary(ND_SUB, new_num(0, tok), cast(), tok);
    }
    if (tok = consume("&")) {
        return new_unary(ND_ADDR, cast(), tok);
    }
    if (tok = consume("*")) {
        return new_unary(ND_DEREF, cast(), tok);
    }
    if (tok = consume("!")) {
        return new_unary(ND_NOT, cast(), tok);
    }
    if (tok = consume("~")) {
        return new_unary(ND_BITNOT, cast(), tok);
    }
    if (tok = consume("++")) {
        // "+" か "++" かはトークナイズのときに確定しているので、
        // パースのときには "+" と "++" の順番を気にする必要はない 
        return new_unary(ND_PRE_INC, unary(), tok);
    }
    if (tok = consume("--")) {
        return new_unary(ND_PRE_DEC, unary(), tok);
    }
    return postfix();
}

// postfix = primary ( "[" expr "]" | "." ident | "->" ident | "++" | "--" )*
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

        if (tok = consume("++")) {
            node = new_unary(ND_POST_INC, node, tok);
            continue;
        }

        if (tok = consume("--")) {
            node = new_unary(ND_POST_DEC, node, tok);
            continue;
        }

        return node;
    }
}

// stmt-expr = stmt* "}" ")"
Node *stmt_expr(Token *tok) {
    Scope *sc = enter_scope();

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

    leave_scope(sc);

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
//         | "sizeof" "(" type-name ")"
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
        // 型名への sizeof には括弧が必須
        if (consume("(")) {
            if (is_typename()) {
                Type *ty = type_name();
                expect(")");
                return new_num(size_of(ty, tok), tok);
            }
            // この時点で tok は sizeof を指しているの
            // next は開き括弧から始まる unary と思われるトークンを指す
            token = tok->next;
        }
        // 型名への sizeof には括弧が必須なので、括弧がない場合は unary
        return new_unary(ND_SIZEOF, unary(), tok);
    }

    if (tok = consume_ident()) {
        if (consume("(")) {
            // 関数呼び出しである場合は関数名を控える
            Node *node = new_node(ND_FUNCALL, tok);
            node->funcname = strndup(tok->str, tok->len);
            // 関数呼び出し時の引数をパース
            node->args = func_args();

            // 関数定義を探す
            VarScope *sc = find_var(tok);
            if (sc) {
                // 関数ではなかったらエラー
                if (!sc->var || sc->var->ty->kind != TY_FUNC) {
                    error_tok(tok, "not a function");
                }
                // 関数呼び出しの場合、関数呼び出しの結果の型は、関数の戻り値の型
                node->ty = sc->var->ty->return_ty;
            }
            else {
                // C では型宣言がないときのデフォルトの型は int
                node->ty = int_type();
            }

            return node;
        }

        // 関数呼び出しでない場合は通常の変数
        VarScope *sc = find_var(tok);
        if (sc) {
            if (sc->var) {
                return new_var(sc->var, tok);
            }
            if (sc->enum_ty) {
                return new_num(sc->enum_val, tok);
            }
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
        Var *var = push_var(new_label(), ty, false, NULL);
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
