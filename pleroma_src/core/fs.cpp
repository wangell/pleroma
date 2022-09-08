#include "fs.h"

#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "ffi.h"

#include <cstring>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

extern std::map<std::string, AstNode *> kernel_map;

AstNode *fs_readfile(EvalContext *context, std::vector<AstNode *> args) {

  StringNode* fname = (StringNode*) args[0];

  std::ifstream t(fname->value);
  std::stringstream buffer;
  buffer << t.rdbuf();

  return make_string(buffer.str());
}

AstNode *fs_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

void load_fs() {
  std::map<std::string, FuncStmt *> functions;

  CType test_type;
  test_type.basetype = PType::u8;

  CType blah;
  blah.basetype = PType::Entity;

  functions["create"] = setup_direct_call(fs_create, "create", {}, {}, test_type);
  functions["create"]->ctype.basetype = PType::u8;

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;
  functions["readfile"] = setup_direct_call(fs_readfile, "readfile", {"fname"}, {str_type}, *str_type);

  kernel_map["FS"] = make_actor(nullptr, "FS", functions, {}, {}, {}, {});
}
