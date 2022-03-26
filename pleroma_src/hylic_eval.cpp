#include "hylic_eval.h"
#include "hylic_ast.h"
#include <cassert>

AstNode *eval_block(std::vector<AstNode *> block, Scope *parent_scope, std::vector<std::tuple<std::string, AstNode*>> sub_syms) {
  Scope block_scope;
  block_scope.parent = parent_scope;

  // load symbols into scope
  for (auto &[sym, node] : sub_syms) {
    block_scope.table[sym] = node;
  }

  AstNode* last_val;
  for (auto node : block) {
    last_val = eval(node, &block_scope);
    if (last_val->type == AstNodeType::ReturnNode) {
      auto v = (ReturnNode*)last_val;
      return eval(v->expr, &block_scope);
    }
  }
  return last_val;
}

AstNode *eval_func(AstNode* obj, std::vector<AstNode *> args) {
    FuncStmt *func_def_node = (FuncStmt *)obj;

    std::vector<std::tuple<std::string, AstNode*>> subs;
    for (int i = 0; i < args.size(); ++i) {
      subs.push_back(std::make_tuple(func_def_node->args[i], args[i]));
    }

    return eval_block(func_def_node->body, &global_scope, subs);
}

AstNode *eval(AstNode *obj, Scope *scope) {
  if (obj->type == AstNodeType::AssignmentStmt) {
    auto ass_stmt = (AssignmentStmt *)obj;

    std::string sym;
      sym = ass_stmt->sym;
    auto expr = eval(ass_stmt->value, scope);

    auto find_it = find_symbol_scope(sym, scope);
    if (find_it) {
      find_it->table[sym] = expr;
    } else {
      scope->table[sym] = expr;
    }

    return expr;
  }

  if (obj->type == AstNodeType::OperatorExpr) {
    auto op_expr = (OperatorExpr *)obj;
    auto n1 = ((NumberNode *)eval(op_expr->term1, scope));
    auto n2 = ((NumberNode *)eval(op_expr->term2, scope));

    if (op_expr->op == OperatorExpr::Plus) {
      return make_number(n1->value + n2->value);
    }
  }

  if (obj->type == AstNodeType::ModuleStmt) {
    auto node = (ModuleStmt*)obj;
    //FIXME
    // NOTE just add a "ModuleNode" - when we do a table access, climb the scopes until you find the node, if its a table do a table access, if its a module add the prefix
    //if (node->namespaced) {
    //  prefix_mode = true;
    //  prefix = node->module;
    //} else {
    //  prefix_mode = false;
    //  prefix = "";
    //}

    // FIXME
    //load_file(node->module + ".x");

    return make_nop();
  }

  if (obj->type == AstNodeType::ReturnNode) {
    return obj;
    //auto node = (ReturnNode*)obj;
    //return eval(node->expr, scope);
  }

  if (obj->type == AstNodeType::NumberNode) {
    return obj;
  }

  if (obj->type == AstNodeType::StringNode) {
    return obj;
  }

  if (obj->type == AstNodeType::Nop) {
    return obj;
  }

  if (obj->type == AstNodeType::TableNode) {
    return obj;
  }

  if (obj->type == AstNodeType::WhileStmt) {
    auto node = (WhileStmt*)obj;

    while (((BooleanNode*)eval(node->generator, scope))->value) {
      eval_block(node->body, scope, {});
    }

    return make_nop();
  }

  if (obj->type == AstNodeType::ForStmt) {
    auto node = (ForStmt*)obj;

    auto table = (TableNode*)eval(node->generator, scope);

    for (int k = 0; k < table->table.size(); ++k) {
      // NOTE push k into sub
      std::vector<std::tuple<std::string, AstNode *>> subs;
      subs.push_back(std::make_tuple(node->sym, make_number(k)));
      eval_block(node->body, scope, subs);
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::TableAccess) {
    auto node = (TableAccess*)obj;

    TableNode* table_node;
    if (node->table->type == AstNodeType::SymbolNode) {
      auto symbol_node = (SymbolNode*) node->table;
      table_node = (TableNode *)find_symbol(symbol_node->sym, scope);
    } else if (node->table->type == AstNodeType::TableNode) {
      table_node = (TableNode*)node->table;
    } else if (node->table->type == AstNodeType::TableAccess) {
      table_node = (TableNode*)eval(node->table, scope);
    } else {
      assert(false);
    }

    if (node->breakthrough) {
      auto sym = find_symbol(node->access_sym, scope);
      assert(sym->type == AstNodeType::NumberNode);
      auto sym_n = (NumberNode*) sym;

      auto find_sym = table_node->table.find(std::to_string(sym_n->value));
      assert(find_sym != table_node->table.end());
      return find_sym->second;
    } else {
      auto find_sym = table_node->table.find(node->access_sym);
      assert(find_sym != table_node->table.end());
      return find_sym->second;
    }
  }

  if (obj->type == AstNodeType::BooleanNode) {
    return obj;
  }

  if (obj->type == AstNodeType::BooleanExpr) {
    auto node = (BooleanExpr*) obj;
    if (node->op == BooleanExpr::GreaterThan || node->op == BooleanExpr::LessThan || node->op == BooleanExpr::GreaterThanEqual || node->op == BooleanExpr::LessThanEqual) {
      auto term1 = eval(node->term1, scope);
      auto term2 = eval(node->term2, scope);

      NumberNode* n1 = (NumberNode*)term1;
      NumberNode* n2 = (NumberNode*)term2;

      switch (node->op) {
      case BooleanExpr::GreaterThan:
        return make_boolean(n1->value > n2->value);
        break;
      case BooleanExpr::LessThan:
        return make_boolean(n1->value < n2->value);
        break;
      case BooleanExpr::GreaterThanEqual:
        return make_boolean(n1->value >= n2->value);
        break;
      case BooleanExpr::LessThanEqual:
        return make_boolean(n1->value <= n2->value);
        break;
      }
    }

    if (node->op == BooleanExpr::Equals) {
    }
    return make_boolean(false);
  }

  if (obj->type == AstNodeType::MatchNode) {
    auto node = (MatchNode*) obj;
    // FIXME only handles boolean
    auto mexpr = (BooleanNode*)eval(node->match_expr, scope);

    for (auto match_case : node->cases) {
      // TODO Make it so the order doesn't matter for fallthrough
      if (std::get<0>(match_case)->type == AstNodeType::FallthroughExpr) {
        return eval_block(std::get<1>(match_case), scope, {});
      } else {
        auto mca_eval = (BooleanNode*)eval(std::get<0>(match_case), scope);
        if (mexpr->value == mca_eval->value) {
            return eval_block(std::get<1>(match_case), scope, {});
        }
      }
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncStmt) {
    auto node = (FuncStmt*)obj;
    std::string sym;
      sym = node->name;
    global_scope.table[sym] = node;
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncCall) {
    auto node = (FuncCall*)obj;
    std::vector<AstNode*> args;

    for (auto arg : node->args) {
      args.push_back(eval(arg, scope));
    }

    //return eval_func(sym, args);
  }

  if (obj->type == AstNodeType::SymbolNode) {
    return find_symbol(((SymbolNode*)obj)->sym, scope);
  }

  assert(false);
}

Scope* find_symbol_scope(std::string sym, Scope* scope) {
  Scope *s = scope;

  auto first_find = scope->table.find(sym);
  if (first_find != s->table.end()) {
    return scope;
  }

  while (true) {
    if (s->parent) {
      s = s->parent;

      auto sym_find = s->table.find(sym);
      if (sym_find == s->table.end()) {
        continue;
      }

      return s;
    } else {
      // Global
      auto sym_find = global_scope.table.find(sym);
      if (sym_find == global_scope.table.end()) {
        return nullptr;
      }

      return &global_scope;
    }
  }
}

AstNode* find_symbol(std::string sym, Scope* scope) {
  Scope* s = scope;

  auto first_find = scope->table.find(sym);
  if (first_find != s->table.end()) {
    return first_find->second;
  }

  while (true) {
    if (s->parent) {
      s = s->parent;

      auto sym_find = s->table.find(sym);
      if (sym_find == s->table.end()) {
        continue;
      }

      return sym_find->second;
    } else {
      // Global
      auto sym_find = global_scope.table.find(sym);
      if (sym_find == global_scope.table.end()) {
        // NOTE could return nil here
        printf("Failed to find symbol %s\n", sym.c_str());
        exit(1);
      }

      return sym_find->second;
    }
  }
}

Entity *create_entity(EntityDef *entity_def) {
  Entity* e = new Entity;
  e->entity_def = entity_def;

  return e;
}
