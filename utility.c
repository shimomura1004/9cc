#include <stdio.h>
#include "utility.h"

void print_node(Node *node, int depth) {
    switch (node->kind) {
        case ND_ADD:
            fprintf(stderr, "%*sADD [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_SUB:
            fprintf(stderr, "%*sSUB [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_MUL:
            fprintf(stderr, "%*sMUL [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_DIV:
            fprintf(stderr, "%*sDIV [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_ASSIGN:
            fprintf(stderr, "%*sASSIGN [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_ADDR:
            fprintf(stderr, "%*sADDR\n", depth, " ");
            print_node(node->lhs, depth + 2);
            break;
        case ND_DEREF:
            fprintf(stderr, "%*sDEREF\n", depth, " ");
            print_node(node->lhs, depth + 2);
            break;
        case ND_VAR:
            fprintf(stderr, "%*sVAR(%s)\n", depth, " ", node->var->name);
            break;
        case ND_EQ:
            fprintf(stderr, "%*sEQ [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_LE:
            fprintf(stderr, "%*sLE [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_NE:
            fprintf(stderr, "%*sNE [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_LT:
            fprintf(stderr, "%*sLT [\n", depth, " ");
            print_node(node->lhs, depth + 2);
            print_node(node->rhs, depth + 2);
            fprintf(stderr, "%*s]\n", depth, " ");
            break;
        case ND_RETURN:
            fprintf(stderr, "%*sRETURN\n", depth, " ");
            print_node(node->lhs, depth + 2);
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
            fprintf(stderr, "%*sEXPR_STMT\n", depth, " ");
            print_node(node->lhs, depth + 2);
            break;
        case ND_NUM:
            fprintf(stderr, "%*sNUM(%d)\n", depth, " ", node->val);
            break;
        default:
            fprintf(stderr, "%*s??? (%d)\n", depth, " ", node->kind);
            break;
    }
}

void print_ast(Function *prog) {
    fprintf(stderr, "----------------\n");

    for (Function *fn = prog; fn; fn = fn->next) {
        fprintf(stderr, "Function %s(", fn->name);

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

    fprintf(stderr, "----------------\n");
}
