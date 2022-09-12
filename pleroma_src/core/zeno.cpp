#include "io.h"
#include "ffi.h"
#include "../type_util.h"

#include <iostream>
#include <string>
#include <vector>

AstNode *zeno_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

std::map<std::string, AstNode*> load_zeno() {
  std::map<std::string, FuncStmt *> functions;

  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;

  functions["create"] = setup_direct_call(zeno_create, "create", {}, {}, none_type);

  return {
    {"Zeno", make_actor(nullptr, "Zeno", functions, {}, {}, {}, {})}
  };
}
