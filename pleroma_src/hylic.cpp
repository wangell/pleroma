// Hylic = the language

#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <stack>
#include <string>
#include <tuple>
#include <vector>

#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_compex.h"
#include "hylic_eval.h"
#include "hylic_parse.h"
#include "hylic_tokenizer.h"
#include "hylic_typesolver.h"

EntityRefNode *monad_ref;

Scope global_scope;

const char *node_type_to_string(AstNodeType t) {
  switch (t) {
  case AstNodeType::AssignmentStmt:
    return "AssignmentStmt";
    break;
  }

  return "Unimplemented";
}

void print(AstNode *s) {
  if (s->type == AstNodeType::NumberNode) {
    auto node = (NumberNode *)s;
  }
}

void parse_match_blocks() {}

HylicModule *load_file(std::string path) {
  printf("Loading %s...\n", path.c_str());

  HylicModule *program;
    TokenStream *stream = tokenize_file(path);

    program = parse(stream);

    typesolve(program);

  return program;
}
