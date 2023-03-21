#include "io.h"
#include "ffi.h"
#include "../type_util.h"

#include <iostream>
#include <string>
#include <vector>

AstNode *io_print(EvalContext *context, std::vector<AstNode *> args) {

  auto pval = args[0];

  if (pval->type == AstNodeType::StringNode) {
    printf("%s\n", ((StringNode *)pval)->value.c_str());
  }

  if (pval->type == AstNodeType::NumberNode) {
    printf("%ld\n", ((NumberNode *)pval)->value);
  }

  if (pval->type == AstNodeType::ListNode) {
    printf("%ld\n", ((NumberNode *)pval)->value);
  }

  if (pval->type == AstNodeType::EntityRefNode) {
    auto eref = (EntityRefNode *)pval;
    printf("EntityRef(%d, %d, %d)\n", eref->node_id, eref->vat_id,
           eref->entity_id);
  }

  return make_number(0);
}

AstNode *io_readline(EvalContext *context, std::vector<AstNode *> args) {

  std::string user_input;

  std::getline(std::cin, user_input);

  return make_string(user_input);
}

AstNode *io_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

std::map<std::string, AstNode*> load_io() {
  std::map<std::string, FuncStmt *> io_functions = {
    {"print", setup_direct_call(io_print, "print", {"val"}, {lstr()}, *lu8())},
    {"readline", setup_direct_call(io_readline, "readline", {}, {}, *lstr())},
    {"create", setup_direct_call(io_create, "create", {}, {}, *void_t())}
  };

  return {
    {"Io", make_actor(nullptr, "Io", io_functions, {}, {}, {}, {})}
  };
}
