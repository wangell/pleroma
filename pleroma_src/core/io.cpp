#include "io.h"
#include "ffi.h"
#include "../type_util.h"

#include <iostream>
#include <string>
#include <vector>

extern std::map<std::string, AstNode *> kernel_map;

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

void load_io() {
  std::map<std::string, FuncStmt *> io_functions;

  CType *strl = new CType;
  strl->basetype = PType::str;
  strl->dtype = DType::Local;

  io_functions["print"] = setup_direct_call(io_print, "print", {"val"}, {strl}, lu8());
  CType str_type;
  str_type.basetype = PType::str;
  str_type.dtype = DType::Local;
  io_functions["readline"] = setup_direct_call(io_readline, "readline", {}, {}, str_type);

  io_functions["print"]->ctype.basetype = PType::u8;

  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;
  io_functions["create"] = setup_direct_call(io_create, "create", {}, {}, none_type);

  //io_functions["readline"]->ctype.basetype = PType::str;

  kernel_map["Io"] = make_actor(nullptr, "Io", io_functions, {}, {});
}
