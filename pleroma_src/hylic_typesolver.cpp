#include "hylic_typesolver.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include <cassert>
#include <map>

struct TypeContext {
  std::map<std::string, CType> typestore;
  std::map<std::string, AstNode*> program;
};

bool is_complex(CType a) {
  return a.basetype == PType::List ||
    a.basetype == PType::Promise ||
    a.basetype == PType::UserType;
}

void exact_match(CType a, CType b) {
  if (a.basetype != b.basetype) {
    // printf("A incompatible with type B: %d, %d\n", a, b);
    printf("A incompatible with type B: %d, %d\n", a.basetype, b.basetype);
    assert(false);
  }

  if (is_complex(a) || is_complex(b)) {
    exact_match(*a.subtype, *b.subtype);
  }
}

CType typesolve_sub(TypeContext* context, AstNode *node) {

  switch (node->type) {

  case AstNodeType::ForeignFunc: {
    return node->ctype;
  } break;

  case AstNodeType::ListNode: {
    return node->ctype;
  } break;

  case AstNodeType::StringNode: {
    return node->ctype;
  } break;

  case AstNodeType::NumberNode: {
    return node->ctype;
  } break;

  case AstNodeType::MessageNode: {
    auto mess_node = (MessageNode *)node;

    // Find the entity definition
    //printf("%s\n", mess_node->entity_ref_name.c_str());
    //assert(context->typestore.find(mess_node->entity_ref_name) != context->typestore.end());

    //CType entity_node_type = context->typestore[mess_node->entity_ref_name];
    //assert(entity_node_type.basetype == PType::Entity);
    //assert(context->program.find(entity_node_type.subtype->entity_name) != context->program.end());

    //EntityDef* entity_def = (EntityDef*)context->program[entity_node_type.subtype->entity_name];

    ////context[mess_node->entity_ref_name
    //// TODO Check that the parameters passed to are solved
    //// Look up the return value and return that values
    //return entity_def->ctype;
    return node->ctype;
  } break;

  case AstNodeType::NamespaceAccess: {
    auto ns_node = (NamespaceAccess *)node;
    return typesolve_sub(context, ns_node->accessor);
  } break;

  case AstNodeType::ReturnNode: {
    auto ret_node = (ReturnNode*)node;
    return typesolve_sub(context, ret_node->expr);
  } break;

  case AstNodeType::CreateEntity: {
    auto ce_node = (CreateEntityNode *)node;
    return ce_node->ctype;
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
        //printf("%d\n", func_node->ctype);
        exact_match(typesolve_sub(context, blocknode), func_node->ctype);
      } else {
        typesolve_sub(context, blocknode);
      }
    }

    if (!has_return) {
      assert(func_node->ctype.basetype == PType::None);
    }

    return CType();

  } break;

  case AstNodeType::OperatorExpr: {
    auto op_expr = (OperatorExpr *)node;

    auto lexpr = typesolve_sub(context, op_expr->term1);
    auto rexpr = typesolve_sub(context, op_expr->term2);

    if (lexpr.basetype == PType::str) {
      if (op_expr->op != OperatorExpr::Op::Plus) {
        printf("Can only add strings\n");
        exit(1);
      }
      exact_match(lexpr, rexpr);
    } else if (lexpr.basetype == PType::u8) {
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
    CType lexpr = assmt_node->sym->ctype;
    CType rexpr = typesolve_sub(context, assmt_node->value);

    exact_match(lexpr, rexpr);

    context->typestore[assmt_node->sym->sym] = lexpr;

    return rexpr;
  } break;

  case AstNodeType::EntityDef: {
    auto ent_node = (EntityDef *)node;
    for (auto &[k, v] : ent_node->functions) {
      typesolve_sub(context, v);
      // all_valid = all_valid && typesolve_sub(v);
    }
    return CType();
  } break;
  }

  // We should never reach here
  printf("Didn't handle type %s\n", ast_type_to_string(node->type).c_str());
  assert(false);
}

void typesolve(std::map<std::string, AstNode *> program) {
  TypeContext context;

  bool all_valid = true;
  for (auto &[k, v] : program) {
    //all_valid = all_valid && typesolve_sub(v);
    //typesolve_sub(&context, v);
  }

  assert(all_valid);
}
