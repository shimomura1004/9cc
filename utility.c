#include <stdio.h>
#include "utility.h"

void print_node(Node *node, int depth);

static char *node_names[] = {
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "BITAND",
    "BITOR",
    "BITXOR",
    "SHL",
    "SHR",
    "ASSIGN",
    "TERNARY",
    "PRE_INC",
    "PRE_DEC",
    "POST_INC",
    "POST_DEC",
    "A_ADD",
    "A_SUB",
    "A_MUL",
    "A_DIV",
    "A_SHL",
    "A_SHR",
    "COMMA",
    "MEMBER",
    "ADDR",
    "DEREF",
    "NOT",
    "BITNOT",
    "LOGAND",
    "LOGOR",
    "VAR",
    "EQ",
    "NE",
    "LT",
    "LE",
    "RETURN",
    "IF",
    "WHILE",
    "FOR",
    "SWITCH",
    "CASE",
    "SIZEOF",
    "BLOCK",
    "BREAK",
    "CONTINUE",
    "GOTO",
    "LABEL",
    "FUNCALL",
    "EXPR_STMT",
    "STMT_EXPR",
    "NUM",
    "CAST",
    "NULL",
};

void print_escaped_char(char c) {
    switch (c) {
    case '\a':
        fprintf(stderr, "\\a");
        break;
    case '\b':
        fprintf(stderr, "\\b");
        break;
    case '\t':
        fprintf(stderr, "\\t");
        break;
    case '\n':
        fprintf(stderr, "\\n");
        break;
    case '\v':
        fprintf(stderr, "\\v");
        break;
    case '\f':
        fprintf(stderr, "\\f");
        break;
    case '\r':
        fprintf(stderr, "\\r");
        break;
    case '\e':
        fprintf(stderr, "\\e");
        break;
    case '\0':
        fprintf(stderr, "\\0");
        break;
    default:
        fprintf(stderr, "%c", c);
        break;
    }
}

void print_initializer_as_string(Initializer *init) {
    for (; init; init = init->next) {
        print_escaped_char((char)init->val);
    }
}

void print_initializer_array(Initializer *init) {
    for (; init; init = init->next) {
        fprintf(stderr, "%ld, ", init->val);
    }
}

void print_string_literal(char *str) {
    for (int i = 0; str[i]; i++) {
        print_escaped_char(str[i]);
    }
}

void print_type(Type *ty) {
    if (!ty) {
        fprintf(stderr, "N/A");
        return;
    }

    switch (ty->kind) {
    case TY_VOID:
        fprintf(stderr, "void");
        return;
    case TY_BOOL:
        fprintf(stderr, "bool");
        return;
    case TY_CHAR:
        fprintf(stderr, "char");
        return;
    case TY_SHORT:
        fprintf(stderr, "short");
        return;
    case TY_INT:
        fprintf(stderr, "int");
        return;
    case TY_LONG:
        fprintf(stderr, "long");
        return;
    case TY_ENUM:
        fprintf(stderr, "enum");
        return;
    case TY_ARRAY:
        print_type(ty->base);
        fprintf(stderr, "[]");
        return;
    case TY_PTR:
        print_type(ty->base);
        fprintf(stderr, "*");
        return;
    case TY_STRUCT:
        fprintf(stderr, "struct");
        return;
    case TY_FUNC:
        fprintf(stderr, "func");
        return;
    default:
        fprintf(stderr, "???");
        return;
    }
}

void print_binary_node(Node *node, int depth) {
    fprintf(stderr, "%*s%s : ", depth, " ", node_names[node->kind]);
    print_type(node->ty);
    fprintf(stderr, " = [\n");
    print_node(node->lhs, depth + 2);
    print_node(node->rhs, depth + 2);
    fprintf(stderr, "%*s]\n", depth, " ");
}

void print_unary_node(Node *node, int depth) {
    fprintf(stderr, "%*s%s : ", depth, " ", node_names[node->kind]);
    print_type(node->ty);
    fprintf(stderr, "\n");
    print_node(node->lhs, depth + 2);
}

void print_node(Node *node, int depth) {
    switch (node->kind) {
        case ND_ADD:
        case ND_SUB:
        case ND_MUL:
        case ND_DIV:
        case ND_EQ:
        case ND_LE:
        case ND_NE:
        case ND_LT:
        case ND_ASSIGN:
        case ND_A_ADD:
        case ND_A_SUB:
        case ND_A_MUL:
        case ND_A_DIV:
        case ND_A_SHL:
        case ND_A_SHR:
        case ND_BITAND:
        case ND_BITOR:
        case ND_BITXOR:
        case ND_SHL:
        case ND_SHR:
        case ND_LOGAND:
        case ND_LOGOR:
        case ND_COMMA:
            print_binary_node(node, depth);
            break;
        case ND_ADDR:
        case ND_DEREF:
        case ND_RETURN:
        case ND_PRE_INC:
        case ND_PRE_DEC:
        case ND_POST_INC:
        case ND_POST_DEC:
        case ND_NOT:
        case ND_BITNOT:
        case ND_CAST:
            print_unary_node(node, depth);
            break;
        case ND_VAR:
            if (!node->var->initializer) {
                fprintf(stderr, "%*sVAR %s : ", depth, " ", node->var->name);
                print_type(node->ty);
            }
            else {
                fprintf(stderr, "%*sVAL : ", depth, " ");
                print_type(node->ty);
                fprintf(stderr, " = \"");
                print_initializer_as_string(node->var->initializer);
                fprintf(stderr, "\"");
            }
            fprintf(stderr, "\n");
            break;
        case ND_MEMBER:
            fprintf(stderr, "%*sMEMBER %s : [\n", depth, " ", node->member_name);
            print_node(node->lhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_TERNARY:
            fprintf(stderr, "%*sTERNARY [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth + 2, " ");
            print_node(node->cond, depth + 4);
            fprintf(stderr, "%*sTHEN\n", depth + 2, " ");
            print_node(node->then, depth + 4);
            fprintf(stderr, "%*sELSE\n", depth + 2, " ");
            print_node(node->els, depth + 4);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_IF:
            fprintf(stderr, "%*sIF [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth + 2, " ");
            print_node(node->cond, depth + 4);
            fprintf(stderr, "%*sTHEN\n", depth + 2, " ");
            print_node(node->then, depth + 4);
            if (node->els) {
                fprintf(stderr, "%*sELSE\n", depth + 2, " ");
                print_node(node->els, depth + 4);
            }
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_WHILE:
            fprintf(stderr, "%*sWHILE [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth + 2, " ");
            print_node(node->cond, depth + 4);
            fprintf(stderr, "%*sBODY\n", depth + 2, " ");
            print_node(node->then, depth + 4);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_FOR:
            fprintf(stderr, "%*sFOR [\n", depth, " ");
            if (node->init) {
                fprintf(stderr, "%*sINIT\n", depth + 2, " ");
                print_node(node->init, depth + 4);
            }
            if (node->cond) {
                fprintf(stderr, "%*sCOND\n", depth + 2, " ");
                print_node(node->cond, depth + 4);
            }
            if (node->inc) {
                fprintf(stderr, "%*sINC\n", depth + 2, " ");
                print_node(node->inc, depth + 4);
            }
            fprintf(stderr, "%*sBODY\n", depth + 2, " ");
            print_node(node->then, depth + 4);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_BREAK:
            fprintf(stderr, "%*sBREAK\n", depth, " ");
            break;
        case ND_CONTINUE:
            fprintf(stderr, "%*sCONTINUE\n", depth, " ");
            break;
        case ND_GOTO:
            fprintf(stderr, "%*sGOTO %s\n", depth, " ", node->label_name);
            break;
        case ND_SWITCH:
            fprintf(stderr, "%*sSWITCH [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth + 2, " ");
            print_node(node->cond, depth + 4);
            fprintf(stderr, "%*sBODY\n", depth + 2, " ");
            print_node(node->then, depth + 4);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_CASE:
            fprintf(stderr, "%*sCASE %ld [\n", depth, " ", node->val);
            print_node(node->lhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_BLOCK:
            fprintf(stderr, "%*sBLOCK [\n", depth, " ");
            for (Node *n = node->body; n; n = n->next) {
                print_node(n, depth + 2);
            }
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_LABEL:
            fprintf(stderr, "%*sLABEL %s [\n", depth, " ", node->label_name);
            print_node(node->lhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_STMT_EXPR:
            fprintf(stderr, "%*sSTMT_EXPR [\n", depth, " ");
            for (Node *n = node->body; n; n = n->next) {
                print_node(n, depth + 2);
            }
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_FUNCALL:
            fprintf(stderr, "%*sFUNCALL %s : ", depth, " ", node->funcname);
            print_type(node->ty);
            fprintf(stderr, " [\n");
            for (Node *arg = node->args; arg; arg = arg->next) {
                print_node(arg, depth + 2);
            }
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_EXPR_STMT:
            print_unary_node(node, depth);
            break;
        case ND_NUM:
            fprintf(stderr, "%*sNUM : ", depth, " ");
            print_type(node->ty);
            fprintf(stderr, " = %ld\n", node->val);
            break;
        case ND_NULL:
            fprintf(stderr, "%*sNULL\n", depth, " ");
            break;
        case ND_SIZEOF: // sizeof は AST に残らないので不要
        default:
            fprintf(stderr, "%*s??? (%d)\n", depth, " ", node->kind);
            break;
    }
}

void print_functions(Function *fn) {
    for (; fn; fn = fn->next) {
        fprintf(stderr, "FUN %s (", fn->name);

        VarList *vl = fn->params;
        if (vl) {
            fprintf(stderr, "%s : ", vl->var->name);
            print_type(vl->var->ty);
            vl = vl->next;
        }
        for (; vl; vl = vl->next) {
            fprintf(stderr, ", %s : ", vl->var->name);
            print_type(vl->var->ty);
        }
        fprintf(stderr, ") {\n");
        for (Node *node = fn->node; node; node = node->next) {
            print_node(node, 2);
        }
        fprintf(stderr, "}\n");
    }
}

void print_global_variables(VarList *vl) {
    for (; vl; vl = vl->next) {
        fprintf(stderr, "VAR %s : ",vl->var->name);
        print_type(vl->var->ty);

        if (vl->var->initializer) {
            if (vl->var->ty->kind == TY_ARRAY) {
                if (vl->var->ty->base->kind == TY_CHAR) {
                    fprintf(stderr, " = \"");
                    print_initializer_as_string(vl->var->initializer);
                    fprintf(stderr, "\"");
                }
                else {
                    fprintf(stderr, " = {");
                    print_initializer_array(vl->var->initializer);
                    fprintf(stderr, "}");
                }
            }
            else {
                if (vl->var->initializer->label) {
                    fprintf(stderr, " = %s", vl->var->initializer->label);
                }
                else {
                    fprintf(stderr, " = %ld", vl->var->initializer->val);
                }
            }
        }

        fprintf(stderr, "\n");
    }
}

void print_ast(Program *prog) {
    fprintf(stderr, "--------------------------------\n");
    print_global_variables(prog->globals);
    fprintf(stderr, "--------------------------------\n");
    print_functions(prog->fns);
    fprintf(stderr, "--------------------------------\n");
}

// verror_at と同じ実装
static Token *previous_token;
void print_source_code(Token *tok) {
    if (!tok) {
        return;
    }

    if (previous_token == tok) {
        return;
    }
    previous_token = tok;

    char *line = tok->str;
    while (user_input < line && line[-1] != '\n') {
        line--;
    }

    char *end = tok->str;
    while (*end != '\n') {
        end++;
    }

    int line_num = 1;
    for (char *p = user_input; p < line; p++) {
        if (*p == '\n') {
            line_num++;
        }
    }

    printf("# ");
    int indent = printf("%s:%d: ", filename, line_num);
    printf("%.*s\n", (int)(end - line), line);

    int pos = tok->str - line + indent;
    printf("# %*s^\n", pos, "");
}
