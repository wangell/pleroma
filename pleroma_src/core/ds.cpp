#include "io.h"
#include "ffi.h"
#include "../type_util.h"

#include <iostream>
#include <string>
#include <vector>

AstNode *hashmap_readline(EvalContext *context, std::vector<AstNode *> args) {

  return make_string("blah");
}

AstNode *hashmap_create(EvalContext *context, std::vector<AstNode *> args) {
  printf("created hashmap\n");
  return make_number(0);
}

AstNode *hashmap_get(EvalContext *context, std::vector<AstNode *> args) {

  std::string sarg = ((StringNode *)args[0])->value;
  printf("Got val: %s\n", sarg.c_str());

  std::string retval = ((StringNode*)cfs(context).entity->_kdata[sarg])->value;

  printf("ret val: %s\n", retval.c_str());

  return make_string(retval);
}

AstNode *hashmap_set(EvalContext *context, std::vector<AstNode *> args) {

  std::string sarg = ((StringNode*) args[1])->value;
  printf("set val %s\n", sarg.c_str());

  cfs(context).entity->_kdata[sarg] = args[0];

  return make_nop();
}

std::map<std::string, AstNode*> load_ds() {
  std::map<std::string, FuncStmt *> hashmap_functions;

  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;

  hashmap_functions["create"] = setup_direct_call(hashmap_create, "create", {}, {}, none_type);
  hashmap_functions["set"] = setup_direct_call(hashmap_set, "set", {"key", "val"}, {str_type, str_type}, none_type);
  hashmap_functions["get"] = setup_direct_call(hashmap_get, "get", {"key"}, {str_type}, *str_type);

  //io_functions["readline"]->ctype.basetype = PType::str;

  return {
    {"Hashmap", make_actor(nullptr, "Hashmap", hashmap_functions, {}, {}, {}, {})}
  };
}
