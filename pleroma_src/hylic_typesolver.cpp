#include "hylic_typesolver.h"
#include "hylic_ast.h"
#include <cassert>
#include <map>
#include <vector>

struct FuncSig {
  CType return_type;
  std::vector<CType*> param_types;
};

struct TopTypes {
  std::map<std::string, FuncSig> functions;
  std::map<std::string, TopTypes *> imported;
};

struct TypeStackFrame {
  HylicModule *module;
  std::vector<Scope> scope_stack;
};

struct TypeContext {
  Scope* scope;
  std::map<std::string, AstNode*> program;
  std::vector<TypeStackFrame> stack;

  TopTypes *top_types;
};

bool is_complex(CType a) {
  return a.basetype == PType::List ||
    a.basetype == PType::Promise ||
    a.basetype == PType::UserType;
}

bool exact_match(CType a, CType b) {
  if (a.basetype != b.basetype) {
    return false;
  }

  if (is_complex(a) || is_complex(b)) {
    return exact_match(*a.subtype, *b.subtype);
  }

  return true;
}

CType typesolve_sub(TypeContext* context, AstNode *node) {

  switch (node->type) {

  case AstNodeType::ReturnNode: {
    auto ret_node = (ReturnNode*) node;
    return typesolve_sub(context, ret_node->expr);
  } break;

  case AstNodeType::NumberNode: {
    return node->ctype;
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
        auto return_type = typesolve_sub(context, blocknode);
        if (!exact_match(typesolve_sub(context, blocknode), func_node->ctype)) {
          printf("Expected: %s, got %s\n", ctype_to_string(&func_node->ctype).c_str(), ctype_to_string(&return_type).c_str());
          throw TypesolverException("nil", 0, 0, "Function return type differs from a body return value.");
        }
      } else {
        //typesolve_sub(context, blocknode);
      }
    }

    if (!has_return) {
      if (func_node->ctype.basetype != PType::None) {
        throw TypesolverException("nil", 0, 0, "Function is marked void, but returns a value in the body.");
      }
    }

    return CType();

  } break;

  case AstNodeType::EntityDef: {
    auto ent_node = (EntityDef *)node;
    for (auto &[k, v] : ent_node->data) {
      // context->typestore[k] = v;
    }

    for (auto &[k, v] : ent_node->functions) {
      typesolve_sub(context, v);
    }
    return CType();
  } break;

  }
    // We should never reach here
    printf("Didn't handle type %s\n", ast_type_to_string(node->type).c_str());
    assert(false);
}

void print_typesolver_exceptions(std::vector<TypesolverException*> exceptions) {
  for (auto e : exceptions) {
    //printf("%s\n", e->msg.c_str());
  }
}

TopTypes *record_top_types(TypeContext* context, HylicModule* module) {

  TopTypes *tt = new TopTypes;

  for (auto &[k, v] : module->imports) {
    //printf("Found import: %s\n", k.c_str());
    tt->imported[k] = record_top_types(context, v);
  }

  for (auto &[k, v] : module->entity_defs) {

    EntityDef* def = (EntityDef*)v;
    //printf("Found entity: %s\n", k.c_str());
    for (auto &[fname, fbod] : def->functions) {
      FuncStmt *b = (FuncStmt*) fbod;
      //printf("Found function: %s with Ctype %s\n", fname.c_str(), ctype_to_string(&fbod->ctype).c_str());
      FuncSig sig;
      sig.return_type = v->ctype;
      sig.param_types = b->param_types;
      tt->functions[k] = sig;
    }
  }

  return tt;

}

void typesolve(HylicModule* module) {
  TypeContext context;
  // First scan for all entities and function signatures (including in imports)
  TopTypes *tt = record_top_types(&context, module);

  for (auto &[k, v] : module->entity_defs) {
    typesolve_sub(&context, v);
  }
}
