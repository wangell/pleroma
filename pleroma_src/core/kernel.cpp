#include "kernel.h"
#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "ffi.h"
#include "net.h"
#include "fs.h"
#include "io.h"
#include "../type_util.h"

#include <iostream>
#include <string>
#include <vector>

std::map<std::string, AstNode *> kernel_map;

std::map<std::string, Entity*> system_entities;

EntityRefNode* get_entity_ref(Entity* e) {
  return (EntityRefNode*)make_entity_ref(e->address.node_id, e->address.vat_id, e->address.entity_id);
}

void load_system_entity(EvalContext *context, std::string entity_name) {
  auto io_ent = create_entity(context, (EntityDef *)kernel_map[entity_name], false);
  system_entities[entity_name] = io_ent;
}

EntityRefNode* get_system_entity_ref(CType ctype) {
  assert(ctype.basetype == PType::Entity);
  assert(ctype.dtype == DType::Far);

  auto ent = system_entities.find(ctype.entity_name);
  assert(ent != system_entities.end());

  return get_entity_ref(ent->second);
}

AstNode *monad_start_program(EvalContext *context, std::vector<AstNode*> args) {

  auto eref = (EntityRefNode*)args[0];

  eval_message_node(context, eref, CommMode::Sync, "main", {make_number(0)});

  return make_number(0);
}

AstNode *monad_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *monad_hello(EvalContext *context, std::vector<AstNode *> args) {
  load_system_entity(context, "Io");

  auto eref = get_entity_ref(system_entities["Io"]);

  eval_message_node(context, eref, CommMode::Sync, "print", {make_string("hi")});
  return make_number(0);
}

void load_kernel() {

  std::map<std::string, FuncStmt *> functions;

  CType *c = new CType;
  *c = lu8();

  functions["main"] = setup_direct_call(monad_hello, "main", {"i"}, {c}, lu8());
  functions["create"] = setup_direct_call(monad_create, "create", {}, {}, lu8());

  CType *c2 = new CType;
  c2->basetype = PType::BaseEntity;
  c2->dtype = DType::Far;

  functions["startprogram"] = setup_direct_call(monad_start_program, "startprogram", {"eref"}, {c2}, lu8());

  kernel_map["Monad"] = make_actor(nullptr, "Monad", functions, {}, {});

  load_io();
  load_net();
  load_fs();
  //load_amoeba();
}
