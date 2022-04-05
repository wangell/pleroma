#include "hylic_typesolver.h"
#include "hylic_ast.h"
#include <cassert>
#include <map>

struct TypeContext {
  std::map<std::string, PType> typestore;
};

void exact_match(PType a, PType b) {
  if (a != b) {
    printf("A incompatible with type B: %d, %d\n", a, b);
    exit(1);
  }
}

PType typesolve_sub(TypeContext* context, AstNode *node) {

  switch (node->type) {

  case AstNodeType::StringNode: {
    return node->ptype;
  } break;

  case AstNodeType::NumberNode: {
    return node->ptype;
  } break;

  case AstNodeType::ReturnNode: {
    auto ret_node = (ReturnNode*)node;
    return typesolve_sub(context, ret_node->expr);
  } break;

  case AstNodeType::FuncStmt: {
    auto func_node = (FuncStmt *)node;
    // Function node has multiple types: return type, and type for each param
    // PType rexpr = typesolve_sub(assmt_node->value);

    // Params
    for (auto param : func_node->param_types) {
    }

    bool has_return = false;
    for (auto blocknode : func_node->body) {
      if (blocknode->type == AstNodeType::ReturnNode) {
        has_return = true;
        printf("%d\n", func_node->ptype);
        assert(typesolve_sub(context, blocknode) == func_node->ptype);
      } else {
        typesolve_sub(context, blocknode);
      }
    }

    if (!has_return) {
      assert(func_node->ptype == PType::None);
    }

    return PType();

  } break;

  case AstNodeType::OperatorExpr: {
    auto op_expr = (OperatorExpr *)node;

    auto lexpr = typesolve_sub(context, op_expr->term1);
    auto rexpr = typesolve_sub(context, op_expr->term2);

    if (lexpr == PType::str) {
      if (op_expr->op != OperatorExpr::Op::Plus) {
        printf("Can only add strings\n");
        exit(1);
      }
      exact_match(lexpr, rexpr);
    } else if (lexpr == PType::u8) {
      exact_match(lexpr, rexpr);
    } else {
      printf("Only numbers and strings can be operated on.\n");
      exit(1);
    }

    // FIXME -> should compute a proper return expr
    return lexpr;
  } break;

  case AstNodeType::AssignmentStmt: {
    auto assmt_node = (AssignmentStmt *)node;
    PType lexpr = assmt_node->sym->ptype;
    PType rexpr = typesolve_sub(context, assmt_node->value);

    exact_match(lexpr, rexpr);

    return rexpr;
  } break;

  case AstNodeType::EntityDef: {
    auto ent_node = (EntityDef *)node;
    for (auto &[k, v] : ent_node->functions) {
      typesolve_sub(context, v);
      // all_valid = all_valid && typesolve_sub(v);
    }
    return PType();
  } break;
  }

  // We should never reach here
  printf("Didn't handle type %d\n", node->type);
  assert(false);
}

void typesolve(std::map<std::string, AstNode *> program) {
  TypeContext context;

  bool all_valid = true;
  for (auto &[k, v] : program) {
    // all_valid = all_valid && typesolve_sub(v);
    //typesolve_sub(&context, v);
  }

  assert(all_valid);
}
