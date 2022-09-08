#include "hylic_eval.h"
#include "blockingconcurrentqueue.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "other.h"
#include "pleroma.h"
#include <cassert>
#include <utility>
#include "core/kernel.h"
#include "system.h"
#include "general_util.h"

extern moodycamel::BlockingConcurrentQueue<Vat *> queue;

AstNode *eval_block(EvalContext *context, std::vector<AstNode *> block,
                    std::vector<std::tuple<std::string, AstNode *>> sub_syms) {

  push_scope(context);

  // load symbols into scope
  for (auto &[sym, node] : sub_syms) {
    css(context).table[sym] = node;
  }

  AstNode *last_val;
  for (auto node : block) {
    last_val = eval(context, node);
    if (last_val->type == AstNodeType::ReturnNode) {
      auto v = (ReturnNode *)last_val;
      return eval(context, v->expr);
    }
  }

  pop_scope(context);

  return make_nop();
}

Entity *resolve_local_entity(EvalContext *context, EntityRefNode *entity_ref) {
  //printf("Resolving local entity: %d %d %d\n", entity_ref->entity_id, entity_ref->vat_id, entity_ref->node_id);
  // FIXME - self fix
  if (entity_ref->entity_id == -1 && entity_ref->vat_id == -1 && entity_ref->node_id == -1) {
    return context->stack.back().entity;
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
    subs.push_back(std::make_tuple(resolve_node->callback->sym, resolve_node->results[i]));
  }

  return eval_block(context, resolve_node->callback->body, subs);
}

AstNode *eval_func_local(EvalContext *context, Entity *entity, std::string function_name, std::vector<AstNode *> args) {

  auto func = entity->entity_def->functions.find(function_name);
  if(func == entity->entity_def->functions.end()) {
    std::string msg = "Attempted to call function '" + function_name + "' on entity '" + entity->entity_def->name + "': function not found.";
    throw PleromaException(msg.c_str());
  }

  FuncStmt *func_def_node = (FuncStmt *)func->second;
  if (func_def_node->args.size() != args.size()) {
    throw PleromaException(std::string("Runtime error: Amount of arguments in function " + entity->entity_def->name + "::" + function_name +  " doesn't match in eval_func_local. Expected " + std::to_string(func_def_node->args.size()) + ", but got " + std::to_string(args.size())).c_str());
  }

  std::vector<std::tuple<std::string, AstNode *>> subs;
  for (int i = 0; i < func_def_node->args.size(); ++i) {
    subs.push_back(std::make_tuple(func_def_node->args[i], eval(context, args[i])));
  }

  push_stack_frame(context, entity, entity->entity_def->module);

  auto res = eval_block(context, func_def_node->body, subs);

  pop_stack_frame(context);

  return res;
}

AstNode *eval_message_node(EvalContext *context, EntityRefNode *entity_ref,
                           CommMode comm_mode, std::string function_name,
                           std::vector<AstNode *> args) {

  // 1. Determine what type of Entity we have - local, far, alien
  // 2. Determine if the call will be sync/async and if we care about the result
  // 3. Insert a row into the Promise stack if we need to.  Yield if we are
  // doing async, otherwise wait for return

  // printf("Eval: message node %s %s\n",
  // target_entity->entity_def->name.c_str(), function_name.c_str());

  //printf("Eval msg node : %d %d %d\n", entity_ref->node_id, entity_ref->vat_id, entity_ref->entity_id);

  if (comm_mode == CommMode::Sync) {
    Entity *target_entity;
    target_entity = resolve_local_entity(context, entity_ref);
    printf("%d %d %d\n", target_entity->address.node_id, target_entity->address.vat_id, target_entity->address.entity_id);
    printf("%s %s\n", target_entity->entity_def->name.c_str(), function_name.c_str());
    assert(target_entity != nullptr);
    return eval_func_local(context, target_entity, function_name, args);
  } else {

    Msg m;

    // We shouldn't have this, replace with self ref
    if (entity_ref->entity_id == -1 && entity_ref->vat_id == -1 && entity_ref->node_id == -1) {
      m.entity_id = cfs(context).entity->address.entity_id;
      m.vat_id = cfs(context).entity->address.vat_id;
      m.node_id = cfs(context).entity->address.node_id;
    } else {
      m.entity_id = entity_ref->entity_id;
      m.vat_id = entity_ref->vat_id;
      m.node_id = entity_ref->node_id;
    }
    m.src_node_id = cfs(context).entity->address.node_id;
    m.src_vat_id = cfs(context).entity->address.vat_id;
    m.src_entity_id = cfs(context).entity->address.entity_id;
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

  // TODO handle alien

  assert(false);
}

AstNode *eval(EvalContext *context, AstNode *obj) {
  if (obj->type == AstNodeType::AssignmentStmt) {
    auto ass_stmt = (AssignmentStmt *)obj;

    AstNode* expr;
    SymbolNode *sym;
    if (ass_stmt->sym->type == AstNodeType::SymbolNode) {
      sym = ((SymbolNode*)ass_stmt->sym);
      expr = eval(context, ass_stmt->value);

      std::map<std::string, AstNode *> *find_it = find_symbol_table(context, sym->sym);
      if (find_it) {
        (*find_it)[sym->sym] = expr;
      } else {
        css(context).table[sym->sym] = expr;
      }
    } else if (ass_stmt->sym->type == AstNodeType::IndexNode) {
      IndexNode* ind_node = (IndexNode*) ass_stmt->sym;
      if (ind_node->list->type == AstNodeType::SymbolNode) {
        sym = ((SymbolNode *)ind_node->list);
        expr = eval(context, ass_stmt->value);

        auto find_list = (ListNode*)find_symbol(context, sym->sym);
        int index_value = ((NumberNode*)eval(context, ind_node->accessor))->value;

        find_list->list[index_value] = expr;
        expr = find_list->list[index_value];

      }
    } else {
      throw PleromaException("Invalid assignment.");
    }

    return expr;
  }

  if (obj->type == AstNodeType::OperatorExpr) {
    auto op_expr = (OperatorExpr *)obj;

    auto n1 = eval(context, op_expr->term1);
    auto n2 = eval(context, op_expr->term2);

    if (n1->type == AstNodeType::NumberNode && n2->type == AstNodeType::NumberNode) {
      if (op_expr->op == OperatorExpr::Plus) {
        return make_number(((NumberNode*)n1)->value + ((NumberNode*)n2)->value);
      }
    } else if (n1->type == AstNodeType::StringNode && n2->type == AstNodeType::StringNode) {
        if (op_expr->op == OperatorExpr::Plus) {
          return make_string(((StringNode *)n2)->value + ((StringNode *)n1)->value);
        }
    } else{
      assert(false);
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

  if (obj->type == AstNodeType::SelfNode) {
    auto eadd = cfs(context).entity->address;
    return make_entity_ref(eadd.node_id, eadd.vat_id, eadd.entity_id);
  }

  if (obj->type == AstNodeType::CommentNode) {
    return obj;
  }

  if (obj->type == AstNodeType::NumberNode) {
    return obj;
  }

  if (obj->type == AstNodeType::StringNode) {
    return obj;
  }

  if (obj->type == AstNodeType::ListNode) {
    auto table = (ListNode *)obj;

    // FIXME: not sure where we should do this
    //for (int k = 0; k < table->list.size(); ++k) {
    //  table->list[k] = eval(context, table->list[k]);
    //}

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
      subs.push_back(std::make_tuple(node->sym, eval(context, table->list[k])));
      eval_block(context, node->body, subs);
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::BooleanNode) {
    return obj;
  }

  if (obj->type == AstNodeType::IndexNode) {
    IndexNode* ind_node = (IndexNode*)obj;
    assert(obj->type == AstNodeType::IndexNode);
    ListNode* list_node = (ListNode*)eval(context, ind_node->list);
    assert(list_node->type == AstNodeType::ListNode);

    auto index = ((NumberNode*)eval(context, ind_node->accessor))->value;

    if (index >= list_node->list.size()) {
      throw PleromaException("Attempted to access array out of bounds.");
    }
    return list_node->list[index];
  }

  if (obj->type == AstNodeType::BooleanExpr) {
    auto node = (BooleanExpr *)obj;
    auto term1 = eval(context, node->term1);
    auto term2 = eval(context, node->term2);
    if (node->op == BooleanExpr::GreaterThan ||
        node->op == BooleanExpr::LessThan ||
        node->op == BooleanExpr::GreaterThanEqual ||
        node->op == BooleanExpr::LessThanEqual) {

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
      }
    }

    if (node->op == BooleanExpr::Equals) {
      if (term1->type == AstNodeType::NumberNode && term2->type == AstNodeType::NumberNode) {
        NumberNode *n1 = (NumberNode *)term1;
        NumberNode *n2 = (NumberNode *)term2;
        return make_boolean(n1->value == n2->value);
      } else if (term1->type == AstNodeType::StringNode && term2->type == AstNodeType::StringNode) {
        StringNode *n1 = (StringNode *)term1;
        StringNode *n2 = (StringNode *)term2;
        return make_boolean(n1->value == n2->value);
      } else {
        assert(false);
      }
    }

    return make_boolean(false);
  }

  if (obj->type == AstNodeType::EntityDef) {
    return obj;
  }

  if (obj->type == AstNodeType::MatchNode) {
    auto node = (MatchNode *)obj;
    // FIXME only handles boolean
    auto mexpr = eval(context, node->match_expr);

    for (auto match_case : node->cases) {
      // TODO Make it so the order doesn't matter for fallthrough
      if (std::get<0>(match_case)->type == AstNodeType::FallthroughExpr) {
        return eval_block(context, std::get<1>(match_case), {});
      } else {
        auto mca_eval = eval(context, std::get<0>(match_case));

        if (mexpr->type == AstNodeType::StringNode) {
          auto sexpr = (StringNode*) mexpr;
          auto sexpr_match = (StringNode *)mca_eval;
          if (sexpr->value == sexpr_match->value) {
            return eval_block(context, std::get<1>(match_case), {});
          }
        } else if (mexpr->type == AstNodeType::NumberNode) {
          auto sexpr = (NumberNode *)mexpr;
          auto sexpr_match = (NumberNode *)mca_eval;
          if (sexpr->value == sexpr_match->value) {
            return eval_block(context, std::get<1>(match_case), {});
          }
        } else if (mexpr->type == AstNodeType::BooleanNode) {
          auto sexpr = (BooleanNode *)mexpr;
          auto sexpr_match = (BooleanNode *)mca_eval;
          if (sexpr->value == sexpr_match->value) {
            return eval_block(context, std::get<1>(match_case), {});
          }
        } else {
          assert(false);
        }
      }
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncStmt) {
    return make_nop();
  }

  if (obj->type == AstNodeType::MessageNode) {
    auto node = (MessageNode *)obj;
    std::vector<AstNode *> args;

    for (auto arg : node->args) {
      args.push_back(eval(context, arg));
    }

    // HACK
    if (node->function_name == "append") {
      auto list_node = (ListNode*) args[1];
      auto val = args[0];
      list_node->list.push_back(val);
      return make_nop();
    }

    if (node->function_name == "len") {
      auto list_node = (ListNode *)args[0];
      return make_number(list_node->list.size());
    }

    // Are we calling this on our self?
    // EntityRefNode* ref = nullptr;;
    // if (node->entity_ref != nullptr) {
    //  ref = (EntityRefNode *)eval(context, node->entity_ref);
    //}

    return eval_message_node(context, (EntityRefNode *)eval(context, node->entity_ref), node->comm_mode, node->function_name, args);
  }

  if (obj->type == AstNodeType::SymbolNode) {
    return find_symbol(context, ((SymbolNode *)obj)->sym);
  }

  if (obj->type == AstNodeType::CreateEntity) {
    auto node = (CreateEntityNode *)obj;

    auto creation_ast = cfs(context).module->entity_defs.find(node->entity_def_name);

    assert(creation_ast != cfs(context).module->entity_defs.end());
    if (node->new_vat) {
      return promise_new_vat(context, (EntityDef *)creation_ast->second);
    } else {
      Entity *ent = create_entity(context, (EntityDef *)creation_ast->second,
                                  node->new_vat);
      return make_entity_ref(ent->address.node_id, ent->address.vat_id,
                             ent->address.entity_id);
    }
  }

  if (obj->type == AstNodeType::EntityRefNode) {
    return obj;
  }

  if (obj->type == AstNodeType::PromiseResNode) {
    auto node = (PromiseResNode *)obj;

    auto prom_sym = find_symbol(context, node->sym);
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
    auto ffc = (ForeignFuncCall *)obj;

    std::vector<AstNode *> args;
    for (auto k : ffc->args) {
      args.push_back(eval(context, k));
    }

    return ffc->foreign_func(context, args);
  }

  if (obj->type == AstNodeType::ModUseNode) {
    auto node = (ModUseNode *)obj;

    auto find_mod = cfs(context).module->imports.find(node->mod_name);

    assert(find_mod != cfs(context).module->imports.end());

    push_stack_frame(context, cfs(context).entity, find_mod->second);

    auto res = eval(context, node->accessor);
    pop_stack_frame(context);

    return res;
  }

  printf("Failing to evaluate node type %s\n", ast_type_to_string(obj->type).c_str());
  assert(false);
}

std::map<std::string, AstNode *> *find_symbol_table(EvalContext *context, std::string sym) {
for (auto x = cfs(context).scope_stack.rbegin(); x != cfs(context).scope_stack.rend(); x++) {
    auto found_it = x->table.find(sym);
    if (found_it != x->table.end()) return &x->table;
  }

 if (cfs(context).entity->data.find(sym) != cfs(context).entity->data.end()) {
   return &cfs(context).entity->data;
  }

  return nullptr;
}

AstNode *find_symbol(EvalContext *context, std::string sym) {
  // Search through lexical scopes
  for (auto x = cfs(context).scope_stack.rbegin(); x != cfs(context).scope_stack.rend(); x++) {
    auto found_it = x->table.find(sym);
    if (found_it != x->table.end()) return found_it->second;
  }

  // Search entity data
  if (cfs(context).entity->data.find(sym) != cfs(context).entity->data.end()) {
    return cfs(context).entity->data.find(sym)->second;
  }

  // Search file scope
  if (cfs(context).module->entity_defs.find(sym) !=
      cfs(context).module->entity_defs.end()) {
    return cfs(context).entity->module_scope->entity_defs.find(sym)->second;
  }

  for (auto &[k, v]: cfs(context).entity->data) {
    printf("Data %s %s\n", sym.c_str(), k.c_str());
  }

  throw PleromaException((std::string("Failed to find symbol: ") + sym).c_str());
}

AstNode *promise_new_vat(EvalContext *context, EntityDef *entity_def) {
  return eval_message_node(context, monad_ref, CommMode::Async, "new-vat", {entity_def});
}

Entity *create_entity(EvalContext *context, EntityDef *entity_def,
                          bool new_vat) {
  Entity *e = new Entity;
  Vat *vat;

  vat = context->vat;

  e->entity_def = entity_def;

  e->address.entity_id = vat->entity_id_base;
  e->address.vat_id = vat->id;
  e->address.node_id = context->node->node_id;
  e->module_scope = entity_def->module;

  //printf("Creating entity: %d %d %d\n", e->address.entity_id, e->address.vat_id, e->address.node_id);

  vat->entity_id_base++;

  vat->entities[e->address.entity_id] = e;


  for (auto &[k, v] : entity_def->data) {
    e->data[k] = v;
  }

  for (auto &k : entity_def->inocaps) {

    // If far - run get_far_inocap() otherwise if local, just find the symbol and run create
    // Hack for now
    if (k.ctype->entity_name == "monad►Monad") {
      e->data[k.var_name] = monad_ref;
      //} else if (k.ctype->dtype == DType::Local) {
    } else {
      //auto old_vat = context->vat;
      //context->vat = vat;

      std::map<std::string, std::string> fqn_map;

      for (auto &[k, v] : entity_def->module->imports) {
        std::vector<std::string> split_name = split_import(k);
        std::string base_name = split_name[split_name.size() - 1];
        fqn_map[base_name] = k;
      }

      std::vector<std::string> split_name = split_import(k.ctype->subtype->entity_name);

      std::string lib_name = split_name[split_name.size() - 2];
      std::string base_name = split_name[split_name.size() - 1];

      // Old-method
      //Entity* io_ent = create_entity(context, (EntityDef *)entity_def->module->imports[fqn_map[lib_name]]->entity_defs[base_name], false);
      //e->data[k.var_name] = make_entity_ref(io_ent->address.node_id, io_ent->address.vat_id, io_ent->address.entity_id);
      push_stack_frame(context, e, e->module_scope);
      assert(monad_ref);
      //if (!monad_ref) {
      //  monad_ref = (EntityRefNode*)make_entity_ref(0, 0, 0);
      //}
      e->data[k.var_name] = eval_message_node(context, monad_ref, CommMode::Async, "request-far-entity", {});
      pop_stack_frame(context);

      // FIXME: see above
      //context->vat = old_vat;
    }
  }

  // FIXME: see above
  eval_func_local(context, e, "create", {});

  if (new_vat) {
    queue.enqueue(vat);
  }

  //printf("%s (%d, %d, %d)\n", entity_def->name.c_str(), e->address.entity_id, e->address.vat_id, e->address.node_id);

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
    printf("val %d\n", k->value_type);
    //print_value_node(k);
  }
  printf("\n\n");
}

void start_context(EvalContext *context, PleromaNode *node, Vat *vat, HylicModule *module, Entity *entity) {
  context->node = node;
  context->vat = vat;

  push_stack_frame(context, nullptr, module);

  cfs(context).module = module;
  cfs(context).entity = entity;

  if (entity) {
    css(context).table = entity->module_scope->entity_defs;
    css(context).table["self"] = make_entity_ref(-1, -1, -1);
  }
}

void push_scope(EvalContext *context) {
  cfs(context).scope_stack.push_back(Scope());
}

void pop_scope(EvalContext *context) {
  cfs(context).scope_stack.pop_back();
}

void push_stack_frame(EvalContext *context, Entity* e, HylicModule* module) {
  context->stack.push_back(StackFrame());
  context->stack.back().entity = e;
  context->stack.back().module = module;

  push_scope(context);
}

void pop_stack_frame(EvalContext *context) {
  context->stack.pop_back();
}

// Current stack frame
StackFrame& cfs(EvalContext *context) {
  return context->stack.back();
}

// Current stack scope
Scope &css(EvalContext *context) {
  return context->stack.back().scope_stack.back();
}
