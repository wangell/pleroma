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
  ModUse
};

struct InfixOp {
  InfixOpType type;

  int n_args;
  std::string name;
};

struct ParseContext {
  TokenStream *ts;
  std::map<std::string, TLUserType> tl_symbol_table;
};

struct ParseException : CompileException {
};

struct HylicModule {
  std::map<std::string, HylicModule*> imports;
  std::map<std::string, AstNode*> entity_defs;
};

HylicModule parse(TokenStream *stream);

std::vector<AstNode *> parse_block(ParseContext *context, int expected_indent);
