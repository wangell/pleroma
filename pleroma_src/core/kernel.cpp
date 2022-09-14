#include "kernel.h"
#include "../general_util.h"
#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "../system.h"
#include "../type_util.h"
#include "amoeba.h"
#include "zeno.h"
#include "ds.h"
#include "ffi.h"
#include "fs.h"
#include "io.h"
#include "net.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>
#include <vector>

std::map<SystemModule, std::map<std::string, AstNode *>> kernel_map;

std::map<std::string, std::map<std::string, Entity*>> system_entities;

std::map<std::string, HylicModule*> sys_mods;

std::map<std::string, HylicModule*> programs;

std::mutex node_mtx;
std::vector<PleromaNode*> nodes;

// Always 1, because we count the Monad
int n_running_programs = 1;

void load_software() {
  programs["helloworld"] = load_file("helloworld", "examples/helloworld.plm");
}

void add_new_pnode(PleromaNode* node) {
  node_mtx.lock();
  nodes.push_back(node);
  node_mtx.unlock();
}

void monad_log(std::string log_str) {
  dbp(log_debug, "\033[1;92m(Monad)\033[0m %s", log_str.c_str());
}

void nodeman_log(std::string log_str) {
  dbp(log_debug, "\033[1;36m(NodeMan)\033[0m %s", log_str.c_str());
}

// Tries to schedule a vat on the current node set, returns true if it was possible
PleromaNode* try_preschedule(EntityDef* edef) {

  PleromaNode* node = nullptr;
  node_mtx.lock();
  for (auto &k : nodes) {
    bool satisfied = true;
    for (auto &rq : edef->preamble) {
      if (std::find(k->resources.begin(), k->resources.end(), rq) == k->resources.end()) {
        satisfied = false;
      }
    }
    if (satisfied) {
      node = k;
    }
  }
  node_mtx.unlock();

  return node;
}

EntityRefNode* get_entity_ref(Entity* e) {
  return (EntityRefNode*)make_entity_ref(e->address.node_id, e->address.vat_id, e->address.entity_id);
}

void load_system_entity(EvalContext *context, std::string sys_name, std::string entity_name) {
  monad_log("Initializing " + sys_name + "[" + entity_name + "]...");

  auto io_def = sys_mods[sys_name]->entity_defs[entity_name];

  auto io_ent = create_entity(context, (EntityDef*)io_def, true);
  io_ent->module_scope = io_ent->entity_def->module;
  assert(io_ent->entity_def->module);
  assert(io_ent->module_scope);
  system_entities[sys_name][entity_name] = io_ent;
}

EntityRefNode* get_system_entity_ref(std::string sys_name, std::string ent_name) {
  auto sys = system_entities.find(sys_name);

  assert(sys != system_entities.end());

  auto ent = sys->second.find(ent_name);

  assert(ent != sys->second.end());

  return get_entity_ref(ent->second);
}

AstNode *monad_new_vat(EvalContext *context, std::vector<AstNode *> args) {
  std::string program_name = ((StringNode *)args[0])->value;
  std::string ent_name = ((StringNode *)args[1])->value;

  monad_log("Received new vat request (" + program_name + " / " + ent_name + ")");

  EntityDef* edef = (EntityDef*)programs[program_name]->entity_defs[ent_name];

  if (PleromaNode* sched_node = try_preschedule(edef)) {
    printf("Sending create-vat to %d %d %d\n", sched_node->nodeman_addr.node_id, sched_node->nodeman_addr.vat_id, sched_node->nodeman_addr.entity_id);
    //FIXME hardcoded nodeman

    auto prom = eval_message_node(context, (EntityRefNode *)make_entity_ref(sched_node->nodeman_addr.node_id, sched_node->nodeman_addr.vat_id, sched_node->nodeman_addr.entity_id), CommMode::Async, "create-vat", args);

    //eval(context, make_assignment(make_symbol("nodemanref"), eval_val));
    //auto eref = (EntityRefNode*)context->vat->promises[eval_val->promise_id].results[0];

    // return eval_val;
    // context->vat->promises[eval_val->promise_id].callback =
    // (PromiseResNode*)make_promise_resolution_node("nodemanref",
    // {make_return(make_symbol("nodemanref"))}); return make_nop();
    // IMPORTANT FIXME
    //return make_entity_ref(0, 2, 0);
    //printf("inside call %s\n", ast_type_to_string(prom->type).c_str());
    return prom;
  }

  // Handle failed schedule with an error
  monad_log("Failed to satisfy vat constraints, didn't allocate new vat");
  assert(false);
}

AstNode *monad_request_far_entity(EvalContext *context, std::vector<AstNode*> args) {

  CType c;
  c.basetype = PType::Entity;
  c.dtype = DType::Far;
  c.entity_name = ((EntityRefNode*)args[0])->ctype.entity_name;


  std::vector<std::string> splimp = split_import(c.entity_name);

  auto io_ent = get_system_entity_ref(splimp[0], splimp[1]);
  monad_log("Got far request for " + c.entity_name + ", resolved to " + entity_ref_str(io_ent));

  return io_ent;
}

AstNode *monad_start_program(EvalContext *context, std::vector<AstNode*> args) {

  std::string program_name = ((StringNode*)args[0])->value;;
  std::string ent_name = ((StringNode *)args[1])->value;

  monad_log("Starting program: " + program_name + " / " + ent_name);

  n_running_programs += 1;
  //printf("Incremented programs: %d\n", n_running_programs);
  //printf("Called eref %d %d %d\n", eref->node_id, eref->vat_id, eref->entity_id);

  auto eref = monad_new_vat(context, args);
  // FIXME: hardcoded NodeMan address

  auto finaly = eval_message_node(context, eref, CommMode::Async, "main", {make_number(0)});
  return finaly;
}

AstNode *monad_n_programs(EvalContext *context, std::vector<AstNode *> args) {
  return make_string(std::to_string(n_running_programs));
}

AstNode *monad_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *monad_hello(EvalContext *context, std::vector<AstNode *> args) {
  monad_log("Hello.");

  system_entities["monad"]["Monad"] = cfs(context).entity;

  sys_mods["io"] = load_system_module(SystemModule::Io);
  sys_mods["amoeba"] = load_system_module(SystemModule::Amoeba);
  sys_mods["net"] = load_system_module(SystemModule::Net);
  sys_mods["zeno"] = load_system_module(SystemModule::Zeno);

  load_system_entity(context, "io", "Io");
  load_system_entity(context, "amoeba", "Amoeba");
  load_system_entity(context, "net", "HttpLb");
  load_system_entity(context, "zeno", "ZenoMaster");

  //eval_message_node(context, eref, CommMode::Sync, "print", {make_string("hi")});
  return make_number(0);
}

AstNode *nodeman_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *nodeman_create_vat(EvalContext *context, std::vector<AstNode *> args) {
  std::string program_name = ((StringNode *)args[0])->value;
  std::string ent_name = ((StringNode *)args[1])->value;

  nodeman_log("Received create vat request (" + program_name + " / " + ent_name + ")");

  EntityDef *edef = (EntityDef *)programs[program_name]->entity_defs[ent_name];

  auto io_ent = create_entity(context, edef, true);
  io_ent->module_scope = io_ent->entity_def->module;

  nodeman_log("Created new vat (" + program_name + " / " + ent_name + ") @ (" + std::to_string(io_ent->address.node_id) + ", " + std::to_string(io_ent->address.vat_id) + ", " + std::to_string(io_ent->address.entity_id) + ")");
  return get_entity_ref(io_ent);
}

void load_kernel() {

  std::map<std::string, FuncStmt *> functions;

  CType *c = new CType;
  *c = lu8();

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

  CType *c_none = new CType;
  c_none->basetype = PType::None;
  c_none->dtype = DType::Local;

  functions["hello"] = setup_direct_call(monad_hello, "hello", {"i"}, {c}, lu8());
  functions["create"] = setup_direct_call(monad_create, "create", {}, {}, *c_none);

  functions["start-program"] = setup_direct_call(monad_start_program, "start-program", {"programname", "entname"}, {c_str, c_str}, lu8());
  functions["n-programs"] = setup_direct_call(monad_n_programs, "n-programs", {}, {}, lstr());
  functions["request-far-entity"] = setup_direct_call(monad_request_far_entity, "request-far-entity", {"ent"}, {c2}, *c3);
  functions["new-vat"] = setup_direct_call(monad_new_vat, "new-vat", {"programname", "entname"}, {c_str, c_str}, *c4);

  std::map<std::string, FuncStmt *> node_man_functions;

  node_man_functions["create"] = setup_direct_call(nodeman_create, "create", {}, {}, *c_none);
  node_man_functions["create-vat"] = setup_direct_call(nodeman_create_vat, "create-vat", {"programname", "entname"}, {c_str, c_str}, *c4);

  std::map<std::string, FuncStmt *> clogger_functions;

  clogger_functions["create"] = setup_direct_call(nodeman_create, "create", {}, {}, *c_none);
  clogger_functions["log"] = setup_direct_call(nodeman_create_vat, "create-vat", {"programname", "entname"}, {c_str, c_str}, *c4);

  kernel_map[SystemModule::Monad] = {
      {"Monad", make_actor(nullptr, "Monad", functions, {}, {}, {}, {})},
      {"NodeMan", make_actor(nullptr, "NodeMan", node_man_functions, {}, {}, {}, {})},
      {"Clogger", make_actor(nullptr, "Clogger", node_man_functions, {}, {}, {}, {})}
  };

  kernel_map[SystemModule::Io] = load_io();
  kernel_map[SystemModule::Net] = load_net();
  kernel_map[SystemModule::Amoeba] = load_amoeba();
  kernel_map[SystemModule::Ds] = load_ds();
  kernel_map[SystemModule::Zeno] = load_zeno();
  load_fs();

}
