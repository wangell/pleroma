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

void do_test() {
}

void load_kernel() {
  std::map<std::string, FuncStmt *> functions;
  functions["main"] = setup_test();

  kernel_map["Kernel"] = make_actor("Kernel", functions, {});
}
