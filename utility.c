#include <stdio.h>
#include "utility.h"

void print_node(Node *node, int depth);

static char *node_names[] = {
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "ASSIGN",
    "ADDR",
    "DEREF",
    "VAR",
    "EQ",
    "NE",
    "LT",
    "LE",
    "RETURN",
    "IF",
    "WHILE",
    "FOR",
    "SIZEOF",
    "BLOCK",
    "FUNCALL",
    "EXPR_STMT",
    "STMT_EXPR",
    "NUM",
    "NULL",
};

void print_string_literal(char *str) {
    for (int i = 0; str[i]; i++) {
        switch (str[i])
        {
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
        default:  
            fputc(str[i], stderr);
            break;
        }
    }
}

void print_type(Type *ty) {
    if (!ty) {
        fprintf(stderr, "N/A");
        return;
    }

    switch (ty->kind) {
    case TY_CHAR:
        fprintf(stderr, "char");
        return;
    case TY_INT:
        fprintf(stderr, "int");
        return;
    case TY_ARRAY:
        print_type(ty->base);
        fprintf(stderr, "[]");
        return;
    case TY_PTR:
        print_type(ty->base);
        fprintf(stderr, "*");
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
            print_binary_node(node, depth);
            break;
        case ND_ADDR:
        case ND_DEREF:
        case ND_RETURN:
            print_unary_node(node, depth);
            break;
        case ND_VAR:
            if (!node->var->contents) {
                fprintf(stderr, "%*sVAR %s : ", depth, " ", node->var->name);
                print_type(node->ty);
            }
            else {
                fprintf(stderr, "%*sVAL : ", depth, " ");
                print_type(node->ty);
                fprintf(stderr, " = \"");
                print_string_literal(node->var->contents);
                fprintf(stderr, "\"");
            }
            fprintf(stderr, "\n");
            break;
        case ND_IF:
            fprintf(stderr, "%*sIF [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth, " ");
            print_node(node->cond, depth + 2);
            fprintf(stderr, "%*sTHEN\n", depth, " ");
            print_node(node->then, depth + 2);
            if (node->els) {
                fprintf(stderr, "%*sELSE\n", depth, " ");
                print_node(node->els, depth + 2);
            }
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_WHILE:
            fprintf(stderr, "%*sWHILE [\n", depth, " ");
            fprintf(stderr, "%*sCOND\n", depth, " ");
            print_node(node->cond, depth + 2);
            fprintf(stderr, "%*sBODY\n", depth, " ");
            print_node(node->then, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_FOR:
            fprintf(stderr, "%*sFOR [\n", depth, " ");
            if (node->init) {
                fprintf(stderr, "%*sINIT\n", depth, " ");
                print_node(node->init, depth + 2);
            }
            if (node->cond) {
                fprintf(stderr, "%*sCOND\n", depth, " ");
                print_node(node->cond, depth + 2);
            }
            if (node->inc) {
                fprintf(stderr, "%*sINC\n", depth, " ");
                print_node(node->inc, depth + 2);
            }
            fprintf(stderr, "%*sBODY\n", depth, " ");
            print_node(node->then, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_BLOCK:
            fprintf(stderr, "%*sBLOCK [\n", depth, " ");
            for (Node *n = node->body; n; n = n->next) {
                print_node(n, depth + 2);
            }
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
            fprintf(stderr, " = %d\n", node->val);
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
