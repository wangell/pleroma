#include "hylic_typesolver.h"
#include "hylic_ast.h"
#include <cassert>
#include <map>
#include <sys/socket.h>
#include <utility>
#include <algorithm>
#include <vector>

struct FuncSig {
  CType return_type;
  bool pure;
  std::vector<CType*> param_types;
};

struct TopTypes {
  std::map<std::string, std::map<std::string, FuncSig>> functions;
  std::map<std::string, TopTypes *> imported;
};

struct TypeScope {
  std::map<std::string, CType*> table;
};

struct TypeContext {
  std::map<std::string, AstNode*> program;
  std::vector<TypeScope> scope_stack;

  EntityDef* entity_def;
  std::string module_name;

  TopTypes *top_types;
  bool pure_func;
};

void push_scope(TypeContext *context) {
  context->scope_stack.push_back(TypeScope());
}

void pop_scope(TypeContext *context) {
  context->scope_stack.pop_back();
}

// Current stack scope
TypeScope &css(TypeContext *context) {
  return context->scope_stack.back();
}

bool is_complex(CType a) {
  return a.basetype == PType::List ||
    a.basetype == PType::Promise ||
    a.basetype == PType::UserType;
}

bool exact_match(CType a, CType b) {
  if (a.basetype != b.basetype || a.dtype != b.dtype) {
    return false;
  }

  if (is_complex(a) || is_complex(b)) {
    return exact_match(*a.subtype, *b.subtype);
  }

  return true;
}

CType *typescope_has(TypeContext *context, std::string sym) {
  for (auto it = context->scope_stack.rbegin(); it != context->scope_stack.rend(); ++it) {
    auto found_it = it->table.find(sym);
    if (found_it != it->table.end()) {
      return found_it->second;
    }
  }

  return nullptr;
}

bool built_in_func(std::string func_name) {
  std::vector<std::string> builtins = {"append", "len"};
  return std::find(builtins.begin(), builtins.end(), func_name) != builtins.end();
}

CType typesolve_sub(TypeContext* context, AstNode *node) {

  switch (node->type) {

  case AstNodeType::ForStmt: {
    auto for_node = (ForStmt *)node;

    push_scope(context);

    auto list_node_type = typesolve_sub(context, for_node->generator);

    // Grab the subtype of the list
    CType p;
    p.basetype = list_node_type.subtype->basetype;
    p.dtype = DType::Local;
    css(context).table[for_node->sym] = &p;

    for (auto blocknode : for_node->body) {
      typesolve_sub(context, blocknode);
    }
    pop_scope(context);

    return CType();
  } break;

  case AstNodeType::WhileStmt: {
    auto while_node = (WhileStmt *)node;

    // FIXME - all of this
    auto while_expr = typesolve_sub(context, while_node->generator);

    for (auto &body_stmt : while_node->body) {
      typesolve_sub(context, body_stmt);
    }

    return node->ctype;
  } break;

  case AstNodeType::MatchNode: {
    auto match_node = (MatchNode *)node;

    // FIXME - all of this
    auto match_expr = typesolve_sub(context, match_node->match_expr);

    for (auto &[case_expr, case_body] : match_node->cases) {
      typesolve_sub(context, case_expr);
      for (auto &k : case_body) {
        typesolve_sub(context, k);
      }
    }

    return node->ctype;
  } break;

  case AstNodeType::OperatorExpr: {
    auto op_expr = (OperatorExpr*)node;

    CType lexpr = typesolve_sub(context, op_expr->term2);
    CType rexpr = typesolve_sub(context, op_expr->term1);

    if (!exact_match(lexpr, rexpr)) {
      throw TypesolverException("", 0, 0, "Operator expression types don't match: " + ctype_to_string(&lexpr) + ", " + ctype_to_string(&rexpr));
    }

    return lexpr;
  } break;

  case AstNodeType::BooleanNode: {
    return node->ctype;
  } break;

  case AstNodeType::BooleanExpr: {
    return node->ctype;
  } break;

  case AstNodeType::ListNode: {
    return node->ctype;
  } break;

  case AstNodeType::CreateEntity: {
    auto ent_node = (CreateEntityNode *)node;
    return ent_node->ctype;
  } break;

  case AstNodeType::SymbolNode: {
    auto sym_node = (SymbolNode *)node;
    auto look = typescope_has(context, sym_node->sym);

    if (context->pure_func && context->entity_def->data.find(sym_node->sym) != context->entity_def->data.end()) {
      throw TypesolverException("nil", 0, 0, "Attempted to access to an entity-level variable inside of a pure function.");
    }

    assert(look != nullptr);
    return *look;
  } break;

  case AstNodeType::ReturnNode: {
    auto ret_node = (ReturnNode*) node;
    return typesolve_sub(context, ret_node->expr);
  } break;

  case AstNodeType::IndexNode: {
    auto index_node = (IndexNode *)node;
    auto ltype = typesolve_sub(context, index_node->list);
    CType st;
    st.basetype = ltype.basetype;
    return st;
  } break;

  case AstNodeType::NumberNode: {
    return node->ctype;
  } break;

  case AstNodeType::StringNode: {
    return node->ctype;
  } break;

  case AstNodeType::EntityRefNode: {
    return node->ctype;
  } break;

  case AstNodeType::PromiseResNode: {
    return node->ctype;
  } break;

  case AstNodeType::ModUseNode: {
    auto mod_use = (ModUseNode*)node;

    if (mod_use->accessor->type == AstNodeType::CreateEntity) {
      return mod_use->accessor->ctype;
    } else {

      auto sub_type = typesolve_sub(context, mod_use->accessor);
      sub_type.entity_name = mod_use->mod_name + "." + sub_type.entity_name;
      return sub_type;
    }
  } break;

  case AstNodeType::MessageNode: {
    auto msg_node = (MessageNode*) node;

    // HACK
    if (built_in_func(msg_node->function_name)) {
      return CType();
    }

    auto ent_ref = (EntityRefNode*)msg_node->entity_ref;

    auto ent_type = typesolve_sub(context, ent_ref);
    auto ent_name = ent_type.entity_name;

    if (ent_ref->node_id == 0 && ent_ref->vat_id == 0 && ent_ref->entity_id == 0) {
      ent_name = context->entity_def->name;
    }

    // FIXME
    if (ent_name == "io.Io") {
      if (msg_node->function_name == "readline") {
        CType c;
        c.basetype = PType::str;
        return c;
      } else {
        CType c;
        c.basetype = PType::None;
        return c;
      }
    }

    if (ent_name == "fs.FS") {
      if (msg_node->function_name == "readfile") {
        CType c;
        c.basetype = PType::str;
        return c;
      }
    }

    auto ent_it = context->top_types->functions.find(ent_name);
    assert(ent_it != context->top_types->functions.end());

    auto func_it = ent_it->second.find(msg_node->function_name);
    FuncSig sig;
    if (func_it == ent_it->second.end()) {
      throw TypesolverException("", 0, 0, "Cannot find called function in entity definition.");
    } else {
      sig = func_it->second;
    }

    if (context->pure_func && !sig.pure) {
      throw TypesolverException("", 0, 0, "Tried to call an impure function from a pure function.");
    }

    // Network == impure
    if (context->pure_func && msg_node->comm_mode == CommMode::Async) {
      throw TypesolverException("", 0, 0, "Tried to send an async message from a pure function.");
    }

    // Check param number
    if (sig.param_types.size() != msg_node->args.size()) {
      throw TypesolverException("", 0, 0, "Number of parameters doesn't match.");
    }

    // Check param type
    for (int i = 0; i < sig.param_types.size(); ++i) {
      auto t1 = sig.param_types[i];
      auto t2 = typesolve_sub(context, msg_node->args[i]);
      if (!exact_match(*t1, t2)) {
        throw TypesolverException("", 0, 0, "Function parameter types don't match: " + ctype_to_string(t1) + ", " + ctype_to_string(&t2));
      }
      i++;
    }

    // Handle entities
    CType ct = sig.return_type;
    CType prom_t = ct;
    // If we're doing async, wrap in a promise
    if (msg_node->comm_mode == CommMode::Async) {
      ct.basetype = PType::Promise;
      ct.subtype = &prom_t;
      ct.dtype = DType::Local;
    }
    return ct;
  } break;

  case AstNodeType::AssignmentStmt: {
    auto assmt_node = (AssignmentStmt *)node;
    SymbolNode *sym;
    if (assmt_node->sym->type == AstNodeType::SymbolNode) {
      sym = (SymbolNode*)make_symbol(((SymbolNode*)assmt_node->sym)->sym);
    } else if (assmt_node->sym->type == AstNodeType::IndexNode){
      IndexNode* ind = (IndexNode*)assmt_node->sym;
      if (ind->list->type == AstNodeType::SymbolNode) {
        sym = (SymbolNode *)make_symbol(((SymbolNode *)ind->list)->sym);
      } else {
        assert(false);
      }
    } else {
      assert(false);
    }

    // TODO: Disallow shadowing
    if (context->pure_func && context->entity_def->data.find(sym->sym) != context->entity_def->data.end()) {
      throw TypesolverException("nil", 0, 0, "Attempted to assign to an entity-level variable inside of a pure function.");
    }

    CType *lexpr;
    if (assmt_node->sym->type == AstNodeType::SymbolNode) {
      lexpr = typescope_has(context, sym->sym);
      if (!lexpr) {
        lexpr = &assmt_node->sym->ctype;
      }
    } else if (assmt_node->sym->type == AstNodeType::IndexNode) {
      CType *temp_type = typescope_has(context, sym->sym);
      CType index_subtype;
      lexpr = temp_type->subtype;
    } else {
      assert(false);
    }

    CType rexpr = typesolve_sub(context, assmt_node->value);

    // If the right expression is an empty list, assign it the value from the left expr
    if (lexpr->basetype == PType::List && rexpr.basetype == PType::List && rexpr.subtype->basetype == PType::NotAssigned) {
      rexpr = *lexpr;
    }

    if (!exact_match(*lexpr, rexpr)) {
      throw TypesolverException("nil", 0, 0, "Attempted to assign a " + ctype_to_string(&rexpr) + " to variable " + sym->sym + " which has type " + ctype_to_string(lexpr));
    }

    css(context).table[sym->sym] = lexpr;

    return *lexpr;
  } break;

  case AstNodeType::FuncStmt: {
    auto func_node = (FuncStmt *)node;
    push_scope(context);

    for (int k = 0; k < func_node->args.size(); ++k) {
      css(context).table[func_node->args[k]] = func_node->param_types[k];
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
        typesolve_sub(context, blocknode);
      }
    }

    if (!has_return) {
      if (func_node->ctype.basetype != PType::None) {
        throw TypesolverException("nil", 0, 0, "Function doesn't return a value, but is not marked void");
      }
    }

    pop_scope(context);
    // No need to return a real type, we already have this in this info TopTypes struct
    return CType();

  } break;

  case AstNodeType::EntityDef: {
    auto ent_node = (EntityDef *)node;

    for (auto &[k, v] : ent_node->functions) {
      push_scope(context);
      context->entity_def = ent_node;
      context->pure_func = v->pure;

      for (auto &k : ent_node->inocaps) {
        css(context).table[k.var_name] = k.ctype;
      }

      for (auto &[k, v] : ent_node->data) {
        // danger?
        css(context).table[k] = &v->ctype;
        //printf("%s : %s\n", k.c_str(), ctype_to_string(&v->ctype).c_str());
      }

      typesolve_sub(context, v);
      pop_scope(context);
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
    tt->imported[k] = record_top_types(context, v);

    for (auto &[k2, v2] : tt->imported[k]->functions) {
      tt->functions[k + "." + k2] = v2;
    }
  }

  for (auto &[k, v] : module->entity_defs) {

    EntityDef* def = (EntityDef*)v;
    //printf("Found entity: %s\n", k.c_str());
    for (auto &[fname, fbod] : def->functions) {
      FuncStmt *b = (FuncStmt*) fbod;
      //printf("Found function: %s with Ctype %s\n", fname.c_str(), ctype_to_string(&fbod->ctype).c_str());
      FuncSig sig;
      sig.return_type = b->ctype;
      sig.param_types = b->param_types;
      sig.pure = b->pure;
      // FIXME
      tt->functions[k][fname] = sig;
    }
  }

  return tt;

}

void typesolve(HylicModule* module) {
  TypeContext context;
  // First scan for all entities and function signatures (including in imports)
  TopTypes *tt = record_top_types(&context, module);
  context.top_types = tt;

  for (auto &[k, v] : module->entity_defs) {
    typesolve_sub(&context, v);
  }
}
