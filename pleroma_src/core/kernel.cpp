#include "kernel.h"
#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "ffi.h"
#include "net.h"
#include "fs.h"
#include "io.h"
#include "../type_util.h"
#include "../system.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

std::map<SystemModule, std::map<std::string, AstNode *>> kernel_map;

std::map<std::string, Entity*> system_entities;

std::map<std::string, HylicModule*> programs;

std::mutex node_mtx;
std::vector<PleromaNode*> nodes;

// Always 1, because we count the Monad
int n_running_programs = 1;

void load_software() {
  programs["helloworld"] = load_file("helloworld", "examples/helloworld.po");
}

void add_new_pnode(PleromaNode* node) {
  node_mtx.lock();
  nodes.push_back(node);
  node_mtx.unlock();
}

// Tries to schedule a vat on the current node set, returns true if it was possible
PleromaNode* try_preschedule(EntityDef* edef) {

  for (auto &k : nodes) {
    bool satisfied = true;
    for (auto &rq : edef->preamble) {
      if (std::find(k->resources.begin(), k->resources.end(), rq) == k->resources.end()) {
        satisfied = false;
      }
    }
    if (satisfied) return k;
  }

  return nullptr;
}

EntityRefNode* get_entity_ref(Entity* e) {
  return (EntityRefNode*)make_entity_ref(e->address.node_id, e->address.vat_id, e->address.entity_id);
}

void load_system_entity(EvalContext *context, std::string entity_name) {
  auto io_def = load_system_module(SystemModule::Io);

  auto io_ent = create_entity(context, (EntityDef*)io_def->entity_defs[entity_name], false);
  io_ent->module_scope = io_ent->entity_def->module;
  assert(io_ent->entity_def->module);
  assert(io_ent->module_scope);
  system_entities[entity_name] = io_ent;
}

EntityRefNode* get_system_entity_ref(CType ctype) {
  assert(ctype.basetype == PType::Entity);
  assert(ctype.dtype == DType::Far);

  auto ent = system_entities.find(ctype.entity_name);
  assert(ent != system_entities.end());

  return get_entity_ref(ent->second);
}

AstNode *monad_new_vat(EvalContext *context, std::vector<AstNode *> args) {

  std::string program_name = ((StringNode *)args[0])->value;
  std::string ent_name = ((StringNode *)args[1])->value;

  EntityDef* edef = (EntityDef*)programs[program_name]->entity_defs[ent_name];

  if (PleromaNode* sched_node = try_preschedule(edef)) {
    printf("Sending create-vat to %d %d %d\n", sched_node->nodeman_addr.node_id, sched_node->nodeman_addr.vat_id, sched_node->nodeman_addr.entity_id);
    //FIXME hardcoded nodeman

    eval_message_node(context, (EntityRefNode *)make_entity_ref(sched_node->nodeman_addr.node_id, sched_node->nodeman_addr.vat_id, sched_node->nodeman_addr.entity_id), CommMode::Async, "create-vat", args);

    //eval(context, make_assignment(make_symbol("nodemanref"), eval_val));
    //auto eref = (EntityRefNode*)context->vat->promises[eval_val->promise_id].results[0];

    // return eval_val;
    // context->vat->promises[eval_val->promise_id].callback =
    // (PromiseResNode*)make_promise_resolution_node("nodemanref",
    // {make_return(make_symbol("nodemanref"))}); return make_nop();
    //  FIXME
    return make_entity_ref(0, 2, 0);
  }

  // Handle failed schedule with an error
  printf("Failed to satisfy preamble!\n");
  assert(false);
}

AstNode *monad_request_far_entity(EvalContext *context, std::vector<AstNode*> args) {

  CType c;
  c.basetype = PType::Entity;
  c.dtype = DType::Far;
  c.entity_name = "Io";

  auto io_ent = get_system_entity_ref(c);

  return io_ent;
}

AstNode *monad_start_program(EvalContext *context, std::vector<AstNode*> args) {

  std::string program_name = ((StringNode*)args[0])->value;;
  std::string ent_name = ((StringNode *)args[1])->value;

  n_running_programs += 1;
  //printf("Incremented programs: %d\n", n_running_programs);
  //printf("Called eref %d %d %d\n", eref->node_id, eref->vat_id, eref->entity_id);

  auto eref = (EntityRefNode*)monad_new_vat(context, args);
  // FIXME: hardcoded NodeMan address
  eval_message_node(context, eref, CommMode::Async, "main", {make_number(0)});

  return make_number(0);
}

AstNode *monad_n_programs(EvalContext *context, std::vector<AstNode *> args) {
  return make_string(std::to_string(n_running_programs));
}

AstNode *monad_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *monad_hello(EvalContext *context, std::vector<AstNode *> args) {
  load_system_entity(context, "Io");

  //eval_message_node(context, eref, CommMode::Sync, "print", {make_string("hi")});
  return make_number(0);
}

AstNode *nodeman_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *nodeman_create_vat(EvalContext *context, std::vector<AstNode *> args) {
  printf("Nodeman received create vat!\n");
  std::string program_name = ((StringNode *)args[0])->value;
  std::string ent_name = ((StringNode *)args[1])->value;

  EntityDef *edef = (EntityDef *)programs[program_name]->entity_defs[ent_name];

  auto io_ent = create_entity(context, edef, true);
  io_ent->module_scope = io_ent->entity_def->module;

  printf("Nodeman created vat with address %d %d %d\n", io_ent->address.node_id, io_ent->address.vat_id, io_ent->address.entity_id);
  return get_entity_ref(io_ent);
}

void load_kernel() {

  std::map<std::string, FuncStmt *> functions;

  CType *c = new CType;
  *c = lu8();

  functions["hello"] = setup_direct_call(monad_hello, "hello", {"i"}, {c}, lu8());
  functions["create"] = setup_direct_call(monad_create, "create", {}, {}, lu8());

  CType *c2 = new CType;
  c2->basetype = PType::BaseEntity;
  c2->dtype = DType::Far;

  CType *c3 = new CType;
  c3->basetype = PType::BaseEntity;
  c3->dtype = DType::Far;

  CType *c4 = new CType;
  c4->basetype = PType::Promise;
  c4->subtype = c2;
  c4->dtype = DType::Local;

  CType *c_str = new CType;
  c_str->basetype = PType::str;
  c_str->dtype = DType::Local;

  functions["start-program"] = setup_direct_call(monad_start_program, "start-program", {"programname", "entname"}, {c_str, c_str}, lu8());
  functions["n-programs"] = setup_direct_call(monad_n_programs, "n-programs", {}, {}, lstr());
  functions["request-far-entity"] = setup_direct_call(monad_request_far_entity, "request-far-entity", {}, {}, *c3);
  functions["new-vat"] = setup_direct_call(monad_new_vat, "new-vat", {"entdef"}, {c_str}, *c4);

  std::map<std::string, FuncStmt *> node_man_functions;

  node_man_functions["create"] = setup_direct_call(nodeman_create, "create", {}, {}, lu8());
  node_man_functions["create-vat"] = setup_direct_call(nodeman_create_vat, "create-vat", {"programname", "entname"}, {c_str, c_str}, *c4);

  kernel_map[SystemModule::Monad] = {
      {"Monad", make_actor(nullptr, "Monad", functions, {}, {}, {}, {})},
      {"NodeMan", make_actor(nullptr, "NodeMan", node_man_functions, {}, {}, {}, {})}
  };

  kernel_map[SystemModule::Io] = load_io();
  kernel_map[SystemModule::Net] = load_net();
  load_fs();

  //load_amoeba();
}
