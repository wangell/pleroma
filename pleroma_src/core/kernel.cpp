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

AstNode *monad_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *monad_hello(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

void load_kernel() {

  std::map<std::string, FuncStmt *> functions;

  functions["hello"] = setup_direct_call(monad_hello, "hello", {}, {}, lu8());
  functions["create"] = setup_direct_call(monad_create, "create", {}, {}, lu8());

  kernel_map["Monad"] = make_actor(nullptr, "Monad", functions, {}, {});

  load_io();
  load_net();
  load_fs();
  //load_amoeba();
}
