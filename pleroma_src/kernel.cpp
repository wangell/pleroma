#include "kernel.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include <vector>

std::map<std::string, AstNode *> kernel_map;

// Context may be unnecessary
AstNode* test_ffi (EvalContext* context, std::vector<AstNode*> args) {

  auto blah = (NumberNode*)args[0];

  printf("FFI demo %ld\n", blah->value);

  return make_number(4);
}

FuncStmt *setup_test() {
  std::vector<AstNode *> body;

  body.push_back(make_foreign_func_call(test_ffi, {make_symbol("sys")}));

  return (FuncStmt*)make_function("main", {"sys"}, body, {});
}

FuncStmt *setup_direct_call(AstNode *(*foreign_func)(EvalContext *, std::vector<AstNode *>), std::string name, std::vector<std::string> args, std::vector<PType *> arg_types) {
  std::vector<AstNode *> body;
  std::vector<AstNode *> nu_args;

  for (auto k : args) {
    nu_args.push_back(make_symbol(k));
  }

  body.push_back(make_foreign_func_call(foreign_func, nu_args));

  return (FuncStmt *)make_function(name, args, body, arg_types);
}

void do_test() {
}

void load_kernel() {
  std::map<std::string, FuncStmt *> functions;
  // functions["main"] = setup_test();
  functions["main"] = setup_direct_call(test_ffi, "main", {"sys"}, {});

  kernel_map["Kernel"] = make_actor("Kernel", functions, {});
}
