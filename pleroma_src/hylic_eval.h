#pragma once

#include "common.h"
#include "hylic_ast.h"
#include "hylic_parse.h"
#include <mutex>
#include <queue>
#include <string>
#include <vector>

struct EntityAddress {
  int node_id = 0;
  int vat_id = 0;
  int entity_id = 0;
};

struct Entity {
  EntityDef *entity_def;
  EntityAddress address;

  std::map<std::string, AstNode *> data;
  HylicModule* module_scope;
  std::map<std::string, AstNode *> _kdata;
};

struct Msg {
  int entity_id = 0;
  int vat_id = 0;
  int node_id = 0;

  int promise_id = 0;

  bool response = false;

  int src_entity_id = 0;
  int src_vat_id = 0;
  int src_node_id = 0;

  std::string function_name;

  std::vector<ValueNode *> values;
};

struct PromiseResult {
  bool resolved = false;
  std::vector<ValueNode *> results;
  PromiseResNode *callback;

  // Return info
  bool return_msg = false;
  Msg msg;
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
  std::map<std::string, AstNode *> table;
};

struct PleromaNode {
  std::string node_name;

  u32 node_id = 0;

  int vat_id_base = 0;

  std::vector<std::string> resources;

  EntityAddress nodeman_addr;
};

struct StackFrame {
  HylicModule *module;
  Entity *entity;
  std::vector<Scope> scope_stack;
};

struct EvalContext {
  PleromaNode *node;
  Vat *vat;
  std::vector<StackFrame> stack;
};

AstNode *eval(EvalContext *context, AstNode *obj);
std::map<std::string, AstNode *> *find_symbol_table(EvalContext *context, std::string sym);
AstNode *find_symbol(EvalContext *context, std::string sym);
Entity *create_entity(EvalContext *context, EntityDef *entity_def, bool new_vat);
AstNode *eval_func_local(EvalContext *context, Entity *entity, std::string function_name, std::vector<AstNode *> args);
AstNode *eval_promise_local(EvalContext *context, Entity *entity, PromiseResult *resolve_node);
AstNode *promise_new_vat(EvalContext *context, EntityDef *entity_def);
void print_value_node(ValueNode * value_node);
void print_msg(Msg * m);

void start_context(EvalContext * context, PleromaNode * node, Vat * vat,
                   HylicModule * module, Entity * entity);
void push_stack_frame(EvalContext * context, Entity * e,
                      HylicModule * module);
void pop_stack_frame(EvalContext * context);

void pop_scope(EvalContext * context);
void push_scope(EvalContext * context);

AstNode *eval_message_node(EvalContext * context, AstNode * entity_ref, CommMode comm_mode, std::string function_name, std::vector<AstNode *> args);

StackFrame &cfs(EvalContext * context);
Scope &css(EvalContext * context);
