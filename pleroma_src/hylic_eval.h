#pragma once

#include <vector>
#include "hylic_ast.h"
#include "common.h"
#include <mutex>
#include <queue>
#include <string>

struct EntityAddress {
  int node_id = 0;
  int vat_id = 0;
  int entity_id = 0;
};

struct Entity {
  EntityDef *entity_def;
  EntityAddress address;

  std::map<std::string, AstNode *> data;
  std::map<std::string, AstNode*> file_scope;
};

struct Msg {
  int entity_id = 0;
  int vat_id = 0;
  int node_id = 0;

  int promise_id = 0;

  bool response = false;

  int src_entity_id = 0;
  int src_vat_id = 0 ;
  int src_node_id = 0;

  std::string function_name;

  std::vector<ValueNode*> values;
};

struct PromiseResult {
  bool resolved = false;
  std::vector<ValueNode*> results;
  PromiseResNode* callback;
};

struct Vat {
  int id = 0;
  int run_n = 0;
  int entity_id_base = 0;

  int promise_id_base = 0;

  std::mutex message_mtx;
  std::queue<Msg> messages;
  std::queue<Msg> out_messages;

  std::map<int, PromiseResult> promises;

  std::map<int, Entity *> entities;
};

struct Scope {
  Scope *parent = nullptr;
  std::map<std::string, AstNode *> table;
  bool readonly = false;
};

extern Scope global_scope;

struct PleromaNode {
  u32 node_id = 0;
};

struct EvalContext {
  PleromaNode* node;
  Vat* vat;
  Entity* entity;
  Scope *scope;
};

AstNode *eval(EvalContext* context, AstNode *obj);
std::map<std::string, AstNode *> * find_symbol_table(EvalContext *context, Scope *scope, std::string sym);
  AstNode *find_symbol(EvalContext * context, std::string sym);
  Entity *create_entity(EvalContext * context, EntityDef * entity_def);
  AstNode *eval_func_local(EvalContext * context, Entity * entity,
                           std::string function_name,
                           std::vector<AstNode *> args);
  AstNode *eval_promise_local(EvalContext * context, Entity * entity,
                              PromiseResult * resolve_node);
  void print_value_node(ValueNode * value_node);
  void print_msg(Msg * m);

  void start_stack(EvalContext * context, Scope * scope, Vat * vat,
                   Entity * entity);
  EvalContext push_stack_frame();

  AstNode *eval_message_node(EvalContext * context, EntityRefNode * entity_ref,
                             MessageDistance distance, CommMode comm_mode,
                             std::string function_name,
                             std::vector<AstNode *> args);
