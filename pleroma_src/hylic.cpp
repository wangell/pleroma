// Hylic = the language

#include <cstdio>
#include <cstdlib>
#include <map>
#include <cassert>
#include <tuple>
#include <vector>
#include <cctype>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <string>
#include <list>
#include <stack>

#include "hylic.h"
#include "hylic_tokenizer.h"
#include "hylic_ast.h"
#include "hylic_eval.h"

TokenStream tokenstream;

Scope global_scope;

Token *check(TokenType t) {
  if (tokenstream.current == tokenstream.tokens.end()) {
    return nullptr;
  }
  auto g = tokenstream.get();
  tokenstream.current--;
  if (g->type == t) {
    return g;
  } else {
    return nullptr;
  }
}

std::vector<Token*> accept_all(std::vector<TokenType> toks) {
  assert(toks.size() > 0);

  // FIXME need to check for toks.length until end
  if (tokenstream.current == tokenstream.tokens.end()) {
    return {};
  }

  std::vector<Token*> return_toks;

  for (int k = 0; k < toks.size(); ++k) {
    auto g = tokenstream.get();
    if (toks[k] == g->type) {
      return_toks.push_back(g);
    } else {
      break;
    }
  }

  if (toks.size() != return_toks.size()) {
    for (int k = 0; k < return_toks.size(); ++k) {
      tokenstream.current--;
    }
    return {};
  }

  return return_toks;
}

Token* accept(TokenType t) {
  if (tokenstream.current == tokenstream.tokens.end()) {
    return nullptr;
  }
  auto g = tokenstream.get();
  if (g->type == t) {

    if (g->type == TokenType::Newline) tokenstream.line_number++;

    return g;
  } else {
    tokenstream.current--;
    return nullptr;
  }
}

const char* token_type_to_string(TokenType t) {
  switch (t) {
  case TokenType::Newline:
    return "Newline";
    break;
  case TokenType::Tab:
    return "Tab";
    break;
  case TokenType::Symbol:
    return "Symbol";
    break;
  case TokenType::LeftParen:
    return "Left Paren";
    break;
  case TokenType::RightParen:
    return "Right Paren";
    break;
  case TokenType::Colon:
    return "Colon";
    break;
  }

  return "Unimplemented";
}

const char* node_type_to_string(AstNodeType t) {
  switch (t) {
  case AstNodeType::AssignmentStmt:
    return "AssignmentStmt";
    break;
  }

  return "Unimplemented";
}

void expect(TokenType t) {
  if (tokenstream.current == tokenstream.tokens.end()) {
    printf("Reached end of tokenstream\n");
    exit(1);
  }

  auto curr = tokenstream.get();
  if (curr->type != t) {
    printf("Expected token type: %s but got %s(%d), at line %d\n", token_type_to_string(t), token_type_to_string(curr->type), curr->type, tokenstream.line_number);
    exit(1);
  }

  tokenstream.line_number++;
}

void print(AstNode* s) {
  if (s->type == AstNodeType::NumberNode) {
    auto node = (NumberNode*)s;
  }
}


char peek(FILE* f) {
  char q = fgetc(f);
  ungetc(q, f);
  return q;
}


bool is_delimiter(char c) {
  return (c == '\n' || c == EOF);
}

void eat_whitespace(FILE *f) {
  char c;

  while (isspace(c = fgetc(f)));
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

void expect(char c, FILE *f) {
  assert(fgetc(f) == c);
}

void parse_match_blocks() {
}

bool check_type(std::string type) {
  // check type against symbol table

  if (type == "u8") return true;

  return false;
}

PType *parse_type() {
  PType *var_type = new PType;

  Token *dist_type = accept(TokenType::Symbol);

  // let blah : (loc|far) [Promise] base_type
  // No such thing as a far Promise

  if (dist_type->lexeme == "loc") {
    var_type->distance = PType::Local;
  } else if (dist_type->lexeme == "far") {
    var_type->distance = PType::Far;
  }

  Token *promise_or_var = accept(TokenType::Symbol);

  if (promise_or_var->lexeme == "Promise") {
    // handle promise
    assert(false);
  } else {
    var_type->type = promise_or_var->lexeme;
    assert(check_type(var_type->type));
}

  return var_type;
}

AstNode* parse_expr() {
  if (accept(TokenType::LeftParen)) {
    auto body = parse_expr();
    expect(TokenType::RightParen);
    return body;
  } else if (accept(TokenType::Return)) {
    return make_return(parse_expr());
  } else if (check(TokenType::String)) {
    auto s = accept(TokenType::String);
    return make_string(s->lexeme);
  } else if (check(TokenType::Symbol)) {
    auto tt = accept(TokenType::Symbol);
    auto expr1 = make_symbol(tt->lexeme);

    if (check(TokenType::Dot)) {
      AstNode* taccess = nullptr;
      while (accept(TokenType::Dot)) {
        bool bt = false;;

        if (accept(TokenType::Breakthrough)) {
          bt = true;
        }

        Token *exp;
        if (check(TokenType::Symbol)) {
          exp = accept(TokenType::Symbol);
        }
        if (check(TokenType::Number)) {
          exp = accept(TokenType::Number);
        }
        if (taccess) {
          taccess = make_table_access(taccess, exp->lexeme, bt);
        } else {
          taccess = make_table_access(expr1, exp->lexeme, bt);
        }
      }
      return taccess;
    }

    if (accept(TokenType::LeftParen)) {

      std::vector<AstNode*> args;
      // While next token does not equal right paren
      while (!check(TokenType::RightParen)) {
        auto expr = parse_expr();
        args.push_back(expr);

        if (!check(TokenType::RightParen)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightParen);

      return make_function_call(expr1, args);
    }
    if (accept(TokenType::Plus)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Plus, expr1, expr2);
    } else if (accept(TokenType::Minus)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Minus, expr1, expr2);
    } else if (accept(TokenType::Star)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Times, expr1, expr2);
    } else if (accept(TokenType::Slash)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Divide, expr1, expr2);
    } else if (accept(TokenType::LessThan)) {
      auto expr2 = parse_expr();
      return make_boolean_expr(BooleanExpr::LessThan, expr1, expr2);
    } else if (accept(TokenType::LessThanEqual)) {
      auto expr2 = parse_expr();
      return make_boolean_expr(BooleanExpr::LessThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThanEqual)) {
      auto expr2 = parse_expr();
      return make_boolean_expr(BooleanExpr::GreaterThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThan)) {
      auto expr2 = parse_expr();
      return make_boolean_expr(BooleanExpr::GreaterThan, expr1, expr2);
    }

    return expr1;
  } else if (check(TokenType::Number)) {
    auto expr1 = make_number(strtol(accept(TokenType::Number)->lexeme.c_str(), nullptr, 10));
    if (accept(TokenType::Plus)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Plus, expr1, expr2);
    }
    if (accept(TokenType::Minus)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Minus, expr1, expr2);
    }
    if (accept(TokenType::Star)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Times, expr1, expr2);
    }

    if (accept(TokenType::Slash)) {
      auto expr2 = parse_expr();
      return make_operator_expr(OperatorExpr::Divide, expr1, expr2);
    }

  if (accept(TokenType::LessThan)) {
    auto expr2 = parse_expr();
    return make_boolean_expr(BooleanExpr::LessThan, expr1, expr2);
  } else if (accept(TokenType::LessThanEqual)) {
    auto expr2 = parse_expr();
    return make_boolean_expr(BooleanExpr::LessThanEqual, expr1, expr2);
  } else if (accept(TokenType::GreaterThanEqual)) {
    auto expr2 = parse_expr();
    return make_boolean_expr(BooleanExpr::GreaterThanEqual, expr1, expr2);
  } else if (accept(TokenType::GreaterThan)) {
    auto expr2 = parse_expr();
    return make_boolean_expr(BooleanExpr::GreaterThan, expr1, expr2);
  }

    return expr1;

  } else if (check(TokenType::True) || check(TokenType::False)) {
    if (accept(TokenType::True)) {
      return make_boolean(true);
    } else if (accept(TokenType::False)) {
      return make_boolean(false);
    }
  } else if (accept(TokenType::LeftBrace)) {
    // While next token does not equal right paren
    std::map<std::string, AstNode*> table_vals;
    while (!check(TokenType::RightBrace)) {

      std::string label;

      Token* t;
      if ((t = accept(TokenType::Symbol))) {
        label = t->lexeme;
      }

      if ((t = accept(TokenType::Number))) {
        label = t->lexeme;
      }

      expect(TokenType::Colon);

      auto expr = parse_expr();

      table_vals[label] = expr;

      if (!check(TokenType::RightBrace)) {
        expect(TokenType::Comma);
      }
    }

    expect(TokenType::RightBrace);

    return make_table(table_vals);
  } else {
    printf("Couldn't parse expression at line %d\n", tokenstream.line_number);
    exit(1);
  }
}



AstNode* parse_stmt(int expected_indent = 0) {

  // NOTE create function to accept row of symbols
  Token *t;
  if ((t = accept(TokenType::Symbol))) {

    // Creating a variable
    if (t->lexeme == "let") {
      Token *new_variable = accept(TokenType::Symbol);

      expect(TokenType::Colon);

      // We can either take a nothing, a loc, or a far
      PType *var_type = parse_type();

      expect(TokenType::Equals);

      AstNode* expr = parse_expr();

      return make_assignment(new_variable->lexeme, expr);
    }

    if (accept(TokenType::Equals)) {
      auto expr = parse_expr();
      expect(TokenType::Newline);
      return make_assignment(t->lexeme, expr);
    } else if (accept(TokenType::LeftParen)) {
      std::vector<AstNode *> args;
      // While next token does not equal right paren
      while (!check(TokenType::RightParen)) {
        auto expr = parse_expr();
        args.push_back(expr);

        if (!check(TokenType::RightParen)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightParen);
      auto sym = make_symbol(t->lexeme);

      return make_function_call(sym , args);
    } else if (accept(TokenType::For)) {
      auto for_generator = parse_expr();
      expect(TokenType::Newline);

      std::vector<AstNode*> body;
      while (true) {
        int current_indent = 0;

        while (accept(TokenType::Tab)) current_indent++;

        if (current_indent == expected_indent + 1) {
          body.push_back(parse_stmt());
          expect(TokenType::Newline);
        } else {
          break;
        }
      }

      return make_for(t->lexeme, for_generator, body);
    } else {
      return make_symbol(t->lexeme);
    }

  } else if (accept(TokenType::While)) {
    printf("Parsing while...\n");
    auto while_expr = parse_expr();
    expect(TokenType::Newline);
    std::vector<AstNode *> body;
    while (true) {
      int current_indent = 0;

      while (accept(TokenType::Tab)) current_indent++;

      if (current_indent == expected_indent + 1) {
        body.push_back(parse_stmt(expected_indent + 1));
        expect(TokenType::Newline);
      } else {
        break;
      }
    }
    return make_while(while_expr, body);
  } else if (accept(TokenType::Return)) {
    auto expr = parse_expr();
    // FIXME
    //expect(TokenType::Newline);
    return make_return(expr);
  } else if (accept(TokenType::Newline)) {
    // return nop node?
    return nullptr;
  } else if (accept(TokenType::Match)) {
    auto match_expr = parse_expr();
    expect(TokenType::Newline);

    std::vector<std::tuple<AstNode *, std::vector<AstNode*>>> match_cases;
    AstNode *match_case = nullptr;
    std::vector<AstNode *> case_body;
    while (true) {
      int current_indent = 0;

      while (accept(TokenType::Tab)) current_indent++;

      if (current_indent == expected_indent + 1) {
        if (match_case) {
          match_cases.push_back(std::make_tuple(match_case, case_body));
        }
        if (accept(TokenType::Fallthrough)) {
          match_case = make_fallthrough();
        } else {
          match_case = parse_expr();
        }
        case_body.clear();
        expect(TokenType::Newline);
      } else if (current_indent > expected_indent + 1) {
        // FIXME is this the right indnet level?
        case_body.push_back(parse_stmt(expected_indent + 2));
        expect(TokenType::Newline);
      } else if (current_indent < expected_indent + 1) {
        // We're done
        match_cases.push_back(std::make_tuple(match_case, case_body));
        tokenstream.go_back(current_indent);
        break;
      }
    }

    return make_match(match_expr, match_cases);
  } else {
    // try parse expr
    return parse_expr();
  }

  assert(false);
}

std::vector<AstNode*> parse_block(int expected_indent = 0) {
  std::vector<AstNode*> block;
  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab)) current_indent++;

    if (current_indent != expected_indent) break;

    printf("parsings statemnet\n");

    auto stmt = parse_stmt(expected_indent);
    block.push_back(stmt);
  }
  return block;
}

AstNode* parse_function() {
  printf("Parsing function\n");
  bool pure;
  if (accept(TokenType::Function)) {
    pure = false;
  } else if (accept(TokenType::Pure)) {
    pure = true;
  }
  auto func_name = accept(TokenType::Symbol);

  std::vector<std::string> args;

  Token* arg;
  expect(TokenType::LeftParen);
  while((arg = accept(TokenType::Symbol))) {

    expect(TokenType::Colon);
    auto type_spec = accept(TokenType::Symbol);

    args.push_back(arg->lexeme);
  }
  expect(TokenType::RightParen);

  // Return type
  expect(TokenType::Minus);
  expect(TokenType::GreaterThan);

  expect(TokenType::Symbol);

  expect(TokenType::Newline);

  printf("getting block\n");
  auto body = parse_block(2);
  printf("got block\n");

  return make_function(func_name->lexeme, args, body);
}

AstNode *parse_actor() {
  std::map<std::string, FuncStmt*> functions;
  std::map<std::string, AstNode *> data;

  printf("Parsing actor\n");
  Token* actor_name;
  if (!(actor_name = accept(TokenType::Symbol))) {
  }

  expect(TokenType::Newline);

  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab)) {
      current_indent++;
    }

    if (current_indent != 1) break;

    printf("func\n");
    FuncStmt* func = (FuncStmt*)parse_function();
    functions[func->name] = func;
    printf("done parsing\n");
  }

  return make_actor(actor_name->lexeme, functions, data);
}

std::map<std::string, AstNode*> parse(TokenStream stream) {

  std::map<std::string, AstNode*> symbol_map;

  if (accept(TokenType::Import)) {
    // ModuleStmt
    std::string mod_name;

    bool namespaced = true;
    if (accept(TokenType::Star)) {
      namespaced = false;
    }

    Token *sym = tokenstream.get();
    expect(TokenType::Newline);
    make_module_stmt(sym->lexeme, namespaced);
  } else if (accept(TokenType::Actor)) {
    EntityDef *actor = (EntityDef*)parse_actor();
    symbol_map[actor->name] = (AstNode*)actor;
  } else if (accept(TokenType::Newline)) {
    // Skip
    make_nop();
  } else {
    assert(false);
  }

  return symbol_map;
}

void parse_file(std::string path) {
  printf("Loading %s...\n", path.c_str());
  // printf("> ");
  FILE *f = fopen(path.c_str(), "r");
  tokenize_file(f);

  std::vector<EntityDef*> nodes;

  //parse(ts);
}

bool typecheck(std::map<std::string, AstNode*> program) {
  return true;
}

std::map<std::string, AstNode*> load_file(std::string path) {
  printf("Loading %s...\n", path.c_str());
  FILE *f = fopen(path.c_str(), "r");
  tokenize_file(f);

  auto program = parse(tokenstream);

  if (!typecheck(program)) {
    printf("Typecheck failed\n");
  }
  printf("Load success!\n");

  return program;
}
