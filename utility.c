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
    "BLOCK",
    "FUNCALL",
    "EXPR_STMT",
    "NUM",
};

void print_type(Type *ty) {
    if (!ty) {
        fprintf(stderr, "N/A");
        return;
    }

    if (ty->kind == TY_INT) {
        fprintf(stderr, "int");
    }
    else {
        fprintf(stderr, "*");
        print_type(ty->base);
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
            fprintf(stderr, "%*sVAR : ", depth, " ");
            print_type(node->ty);
            fprintf(stderr, " = '%s\n", node->var->name);
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
        case ND_FUNCALL:
            fprintf(stderr, "%*sFUNCALL(%s) [\n", depth, " ", node->funcname);
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
        default:
            fprintf(stderr, "%*s??? (%d)\n", depth, " ", node->kind);
            break;
    }
}

void print_ast(Function *prog) {
    fprintf(stderr, "--------------------------------\n");

    for (Function *fn = prog; fn; fn = fn->next) {
        fprintf(stderr, "Function %s (", fn->name);

        VarList *vl = fn->params;
        if (vl) {
            fprintf(stderr, "%s", vl->var->name);
            vl = vl->next;
        }
        for (; vl; vl = vl->next) {
            fprintf(stderr, ", %s", vl->var->name);
        }
        fprintf(stderr, ") {\n");
        for (Node *node = fn->node; node; node = node->next) {
            print_node(node, 2);
        }
        fprintf(stderr, "}\n");
    }

    fprintf(stderr, "--------------------------------\n");
}
