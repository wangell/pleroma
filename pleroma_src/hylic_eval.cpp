#include "hylic_eval.h"
#include "hylic_ast.h"
#include "pleroma.h"
#include <cassert>

AstNode *eval_block(EvalContext *context, std::vector<AstNode *> block,
                    std::vector<std::tuple<std::string, AstNode *>> sub_syms) {
  Scope block_scope;
  block_scope.parent = context->scope;

  EvalContext new_context;
  new_context.entity = context->entity;
  new_context.vat = context->vat;
  new_context.scope = &block_scope;

  // load symbols into scope
  for (auto &[sym, node] : sub_syms) {
    block_scope.table[sym] = node;
  }

  AstNode *last_val;
  for (auto node : block) {
    last_val = eval(&new_context, node);
    if (last_val->type == AstNodeType::ReturnNode) {
      auto v = (ReturnNode *)last_val;
      return eval(&new_context, v->expr);
    }
  }
  return last_val;
}

Entity *resolve_local_entity(EvalContext *context, EntityRefNode *entity_ref) {
  // printf("Resolving local entity: %d %d %d\n", entity_ref->entity_id,
  // entity_ref->vat_id, entity_ref->node_id);
  if (entity_ref->entity_id == 0 && entity_ref->vat_id == 0 &&
      entity_ref->node_id == 0) {
    return context->entity;
  } else {
    auto found_ent = context->vat->entities.find(entity_ref->entity_id);
    assert(found_ent != context->vat->entities.end());

    return found_ent->second;
  }
}

AstNode *eval_promise_local(EvalContext *context, Entity *entity,
                            PromiseResult *resolve_node) {

  std::vector<std::tuple<std::string, AstNode *>> subs;
  for (int i = 0; i < resolve_node->results.size(); ++i) {
    subs.push_back(
        std::make_tuple(resolve_node->callback->sym, resolve_node->results[i]));
  }

  return eval_block(context, resolve_node->callback->body, subs);
}

AstNode *eval_func_local(EvalContext *context, Entity *entity,
                         std::string function_name,
                         std::vector<AstNode *> args) {
  // printf("Eval: eval_func_local %s %s\n", entity->entity_def->name.c_str(),
  // function_name.c_str());
  auto func = entity->entity_def->functions.find(function_name);

  assert(func != entity->entity_def->functions.end());

  FuncStmt *func_def_node = (FuncStmt *)func->second;
  assert(func_def_node->args.size() == args.size());

  std::vector<std::tuple<std::string, AstNode *>> subs;
  for (int i = 0; i < func_def_node->args.size(); ++i) {
    subs.push_back(std::make_tuple(func_def_node->args[i], args[i]));
  }
  return eval_block(context, func_def_node->body, subs);
}

AstNode *eval_message_node(EvalContext *context, EntityRefNode *entity_ref,
                           MessageDistance distance, CommMode comm_mode,
                           std::string function_name,
                           std::vector<AstNode *> args) {

  // 1. Determine what type of Entity we have - local, far, alien
  // 2. Determine if the call will be sync/async and if we care about the result
  // 3. Insert a row into the Promise stack if we need to.  Yield if we are
  // doing async, otherwise wait for return
  Entity *target_entity = context->entity;

  target_entity = resolve_local_entity(context, entity_ref);
  assert(target_entity != nullptr);

  // printf("Eval: message node %s %s\n",
  // target_entity->entity_def->name.c_str(), function_name.c_str());

  if (distance == MessageDistance::Local) {

    if (comm_mode == CommMode::Sync) {

      return eval_func_local(context, target_entity, function_name, args);
    } else {

      Msg m;
      m.entity_id = target_entity->address.entity_id;
      m.vat_id = target_entity->address.vat_id;
      m.node_id = target_entity->address.node_id;
      m.src_node_id = 0;
      m.src_entity_id = 0;
      m.src_vat_id = 0;
      m.function_name = function_name;

      m.response = false;

      for (auto varg : args) {
        m.values.push_back((ValueNode *)varg);
      }

      int pid = context->vat->promise_id_base;
      m.promise_id = pid;

      context->vat->out_messages.push(m);

      context->vat->promises[pid] = PromiseResult();
      context->vat->promise_id_base++;

      return make_promise_node(pid);
    }

  } else if (distance == MessageDistance::Far) {
    Msg m;
    m.entity_id = target_entity->address.entity_id;
    m.vat_id = target_entity->address.vat_id;
    m.node_id = target_entity->address.node_id;

    m.src_node_id = 0;
    m.src_entity_id = 0;
    m.src_vat_id = 0;

    m.function_name = function_name;
    m.response = false;
    context->vat->out_messages.push(m);

    return make_nop();

  } else {
    // Alien
    assert(false);
  }

  assert(false);
}

AstNode *eval(EvalContext *context, AstNode *obj) {
  if (obj->type == AstNodeType::AssignmentStmt) {
    auto ass_stmt = (AssignmentStmt *)obj;

    SymbolNode* sym;
    sym = ass_stmt->sym;
    auto expr = eval(context, ass_stmt->value);

    auto find_it = find_symbol_scope(sym->sym, context->scope);
    if (find_it) {
      find_it->table[sym->sym] = expr;
    } else {
      context->scope->table[sym->sym] = expr;
    }

    return expr;
  }

  if (obj->type == AstNodeType::OperatorExpr) {
    auto op_expr = (OperatorExpr *)obj;
    auto n1 = ((NumberNode *)eval(context, op_expr->term1));
    auto n2 = ((NumberNode *)eval(context, op_expr->term2));

    if (op_expr->op == OperatorExpr::Plus) {
      return make_number(n1->value + n2->value);
    }
  }

  if (obj->type == AstNodeType::ModuleStmt) {
    auto node = (ModuleStmt *)obj;
    // FIXME
    // NOTE just add a "ModuleNode" - when we do a table access, climb the
    // scopes until you find the node, if its a table do a table access, if its
    // a module add the prefix
    // if (node->namespaced) {
    //  prefix_mode = true;
    //  prefix = node->module;
    //} else {
    //  prefix_mode = false;
    //  prefix = "";
    //}

    // FIXME
    // load_file(node->module + ".x");

    return make_nop();
  }

  if (obj->type == AstNodeType::ReturnNode) {
    return obj;
    // auto node = (ReturnNode*)obj;
    // return eval(node->expr, scope);
  }

  if (obj->type == AstNodeType::NumberNode) {
    return obj;
  }

  if (obj->type == AstNodeType::StringNode) {
    return obj;
  }

  if (obj->type == AstNodeType::ListNode) {
    return obj;
  }

  if (obj->type == AstNodeType::Nop) {
    return obj;
  }

  if (obj->type == AstNodeType::TableNode) {
    return obj;
  }

  if (obj->type == AstNodeType::WhileStmt) {
    auto node = (WhileStmt *)obj;

    while (((BooleanNode *)eval(context, node->generator))->value) {
      eval_block(context, node->body, {});
    }

    return make_nop();
  }

  if (obj->type == AstNodeType::ForStmt) {
    auto node = (ForStmt *)obj;

    auto table = (ListNode *)eval(context, node->generator);

    for (int k = 0; k < table->list.size(); ++k) {
      // NOTE push k into sub
      std::vector<std::tuple<std::string, AstNode *>> subs;
      subs.push_back(std::make_tuple(node->sym, make_number(k)));
      eval_block(context, node->body, subs);
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::NamespaceAccess) {
    auto node = (NamespaceAccess *)obj;

    // Lookup the symbol
    auto ref_node =
        eval(context, find_symbol(node->namespace_table, context->scope));

    if (ref_node->type == AstNodeType::EntityRefNode) {

      auto ent_node = (EntityRefNode *)ref_node;
      auto mess_node = (MessageNode *)node->accessor;

      EvalContext new_context;
      new_context.vat = context->vat;
      new_context.entity =
          resolve_local_entity(context, (EntityRefNode *)ref_node);
      Scope new_scope;
      new_scope.table["a"] = make_number(4);
      new_scope.table["self"] = new_context.entity->entity_def;
      new_context.scope = &new_scope;

      // printf("Searching namespace %s\n", node->namespace_table.c_str());

      return eval_message_node(context, ent_node, mess_node->message_distance,
                               mess_node->comm_mode, mess_node->function_name,
                               mess_node->args);

    } else {
      assert(false);
    }

    assert(false);
  }

  if (obj->type == AstNodeType::BooleanNode) {
    return obj;
  }

  if (obj->type == AstNodeType::BooleanExpr) {
    auto node = (BooleanExpr *)obj;
    if (node->op == BooleanExpr::GreaterThan ||
        node->op == BooleanExpr::LessThan ||
        node->op == BooleanExpr::GreaterThanEqual ||
        node->op == BooleanExpr::LessThanEqual) {
      auto term1 = eval(context, node->term1);
      auto term2 = eval(context, node->term2);

      NumberNode *n1 = (NumberNode *)term1;
      NumberNode *n2 = (NumberNode *)term2;

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
      case BooleanExpr::Equals:
        assert(false);
        break;
      }
    }

    if (node->op == BooleanExpr::Equals) {
    }
    return make_boolean(false);
  }

  if (obj->type == AstNodeType::MatchNode) {
    auto node = (MatchNode *)obj;
    // FIXME only handles boolean
    auto mexpr = (BooleanNode *)eval(context, node->match_expr);

    for (auto match_case : node->cases) {
      // TODO Make it so the order doesn't matter for fallthrough
      if (std::get<0>(match_case)->type == AstNodeType::FallthroughExpr) {
        return eval_block(context, std::get<1>(match_case), {});
      } else {
        auto mca_eval = (BooleanNode *)eval(context, std::get<0>(match_case));
        if (mexpr->value == mca_eval->value) {
          return eval_block(context, std::get<1>(match_case), {});
        }
      }
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncStmt) {
    auto node = (FuncStmt *)obj;
    std::string sym;
    sym = node->name;
    global_scope.table[sym] = node;
    return make_nop();
  }

  if (obj->type == AstNodeType::MessageNode) {
    auto node = (MessageNode *)obj;
    std::vector<AstNode *> args;

    for (auto arg : node->args) {
      args.push_back(eval(context, arg));
    }

    if (node->function_name == "print") {
      if (args[0]->type == AstNodeType::StringNode) {
        printf("%s\n", ((StringNode *)args[0])->value.c_str());
      }

      if (args[0]->type == AstNodeType::NumberNode) {
        printf("%ld\n", ((NumberNode *)args[0])->value);
      }

      if (args[0]->type == AstNodeType::ListNode) {
        printf("[");
        for (auto k : ((ListNode *)args[0])->list) {
          if (k->type == AstNodeType::StringNode) {
            printf("%s", ((StringNode *)k)->value.c_str());
          }

          if (k->type == AstNodeType::NumberNode) {
            printf("%ld", ((NumberNode *)k)->value);
          }

          printf(",");
        }
        printf("]\n");
      }

      return make_nop();
    } else {
      return eval_message_node(
          context,
          (EntityRefNode *)find_symbol(node->entity_ref_name, context->scope),
          node->message_distance, node->comm_mode, node->function_name, args);
    }
  }

  if (obj->type == AstNodeType::SymbolNode) {
    return find_symbol(((SymbolNode *)obj)->sym, context->scope);
  }

  if (obj->type == AstNodeType::CreateEntity) {
    auto node = (CreateEntityNode *)obj;
    Entity* ent = create_entity(context, (EntityDef *)find_symbol(node->entity_def_name, context->scope));

    return make_entity_ref(ent->address.node_id, ent->address.vat_id, ent->address.entity_id);
  }

  if (obj->type == AstNodeType::EntityRefNode) {
    return obj;
  }

  if (obj->type == AstNodeType::PromiseResNode) {
    auto node = (PromiseResNode *)obj;

    auto prom_sym = find_symbol(node->sym, context->scope);
    assert(prom_sym->type == AstNodeType::PromiseNode);
    auto prom = (PromiseNode *)prom_sym;

    assert(context->vat->promises.find(prom->promise_id) !=
           context->vat->promises.end());

    // If available, run now, else stuff the promise into the Promise stack -
    // will be resolved + run by VM
    if (context->vat->promises[prom->promise_id].resolved) {
      assert(false);
      // return eval(context, context->vat->promises[prom->promise_id].result);
    } else {
      context->vat->promises[prom->promise_id].callback = node;
      return obj;
    }
  }

  if (obj->type == AstNodeType::PromiseNode) {
    return obj;
  }

  if (obj->type == AstNodeType::ForeignFunc) {
    auto ffc = (ForeignFuncCall*)obj;

    std::vector<AstNode*> args;
    for (auto k : ffc->args) {
      args.push_back(eval(context, k));
    }

    return ffc->foreign_func(context, args);
  }

  printf("Failing to evaluate node type %s\n",
         ast_type_to_string(obj->type).c_str());
  assert(false);
}

Scope *find_symbol_scope(std::string sym, Scope *scope) {
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

AstNode *find_symbol(std::string sym, Scope *scope) {
  Scope *s = scope;

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

Entity *create_entity(EvalContext *context, EntityDef *entity_def) {
  Entity *e = new Entity;
  e->entity_def = entity_def;

  e->address.entity_id = context->vat->entity_id_base;
  e->address.vat_id = context->vat->id;
  e->address.node_id = context->node->node_id;

  context->vat->entity_id_base++;

  context->vat->entities[e->address.entity_id] = e;

  return e;
}

void print_value_node(ValueNode *value_node) {
  switch (value_node->value_type) {
  case ValueType::Number:
    printf("%ld", ((NumberNode *)value_node)->value);
    break;
  case ValueType::String:
    printf("%s", ((StringNode *)value_node)->value.c_str());
    break;
  case ValueType::Boolean: {
    auto bval = (BooleanNode *)value_node;
    if (bval->value) {
      printf("#t");
    } else {
      printf("#f");
    }
    break;
  }
  case ValueType::Character:
    assert(false);
    break;
  case ValueType::User:
    assert(false);
    break;
  }
}

void print_msg(Msg *m) {
  if (m->response) {
    printf("MsgResponse => %s\n", m->function_name.c_str());
  } else {
    printf("Msg => %s\n", m->function_name.c_str());
  }

  printf("\tTarget: Node: %d, Vat: %d, Entity: %d\n", m->node_id, m->vat_id,
         m->entity_id);
  printf("\tSource: Node: %d, Vat: %d, Entity: %d\n", m->src_node_id,
         m->src_vat_id, m->src_entity_id);
  printf("\tOther: Promise: %d\n", m->promise_id);
  printf("\tPayload (%zu): ", m->values.size());
  for (auto k : m->values) {
    print_value_node(k);
  }
  printf("\n\n");
}

void start_stack(EvalContext* context, Scope *scope, Vat* vat, Entity *entity) {
  scope->table = entity->file_scope;
  scope->table["self"] = make_entity_ref(0, 0, 0);

  context->vat = vat;
  context->entity = entity;
  context->scope = scope;
}

EvalContext push_stack_frame() {
}
