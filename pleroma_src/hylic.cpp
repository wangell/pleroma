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
#include "hylic_eval.h"
#include "hylic_parse.h"
#include "hylic_tokenizer.h"
#include "hylic_typesolver.h"

TokenStream tokenstream;

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

char peek(FILE *f) {
  char q = fgetc(f);
  ungetc(q, f);
  return q;
}

bool is_delimiter(char c) { return (c == '\n' || c == EOF); }

void eat_whitespace(FILE *f) {
  char c;

  while (isspace(c = fgetc(f)))
    ;
  ungetc(c, f);
}

std::string get_symbol(FILE *f) {
  char c;
  std::string s;
  while ((c = fgetc(f)) == '_' || isdigit(c) || isalpha(c)) {
    s.push_back(c);
  }

  ungetc(c, f);

  return s;
}

void expect(char c, FILE *f) { assert(fgetc(f) == c); }

void parse_match_blocks() {}

std::map<std::string, AstNode*> load_file(std::string path) {
  printf("Loading %s...\n", path.c_str());
  FILE *f = fopen(path.c_str(), "r");
  tokenize_file(f);

  //auto program = resolve_thunks(parse(tokenstream));
  auto program = parse(tokenstream);

  typesolve(program);

  return program;
}
