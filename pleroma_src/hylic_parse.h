#pragma once

#include <map>
#include <string>
#include "hylic_ast.h"
#include "hylic_compex.h"
#include "hylic_tokenizer.h"
#include <cassert>
#include <stack>

enum class TLUserType {
  Entity,
  Data
};

enum class InfixOpType {
  Plus,
  Minus,
  Divide,
  Multiply,

  MessageSync,
  MessageAsync,
  NewVat,
  Namespace,
  ModUse,
  CreateEntity,

  // Boolean ops
  LessThan,
  LessThanEqual,
  GreaterThan,
  GreaterThanEqual,
  Equals,

  Range,

  Index
};

struct InfixOp {
  InfixOpType type;

  int n_args;
  std::string name;
};

struct ParseContext {
  TokenStream *ts;
  HylicModule* module;
  std::string current_mod_path;
};

class ParserException : public CompileException {
public:
  ParserException(std::string file, u32 line_n, u32 char_n, std::string msg)
      : CompileException(file, line_n, char_n, msg) {}
};

HylicModule *parse(std::string abs_mod_path, TokenStream *stream);

std::vector<AstNode *> parse_block(ParseContext *context, int expected_indent);
