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

std::stack<TokenStream> tokenstream_stack;

Scope global_scope;

AstNode* static_nop;
AstNode *static_true;
AstNode *static_false;

Token *check(TokenType t) {
  if (tokenstream_stack.top().current == tokenstream_stack.top().tokens.end()) {
    return nullptr;
  }
  auto g = tokenstream_stack.top().get();
  tokenstream_stack.top().current--;
  if (g->type == t) {
    return g;
  } else {
    return nullptr;
  }
}

std::vector<Token*> accept_all(std::vector<TokenType> toks) {
  assert(toks.size() > 0);

  // FIXME need to check for toks.length until end
  if (tokenstream_stack.top().current == tokenstream_stack.top().tokens.end()) {
    return {};
  }

  std::vector<Token*> return_toks;

  for (int k = 0; k < toks.size(); ++k) {
    auto g = tokenstream_stack.top().get();
    if (toks[k] == g->type) {
      return_toks.push_back(g);
    } else {
      break;
    }
  }

  if (toks.size() != return_toks.size()) {
    for (int k = 0; k < return_toks.size(); ++k) {
      tokenstream_stack.top().current--;
    }
    return {};
  }

  return return_toks;
}

Token* accept(TokenType t) {
  if (tokenstream_stack.top().current == tokenstream_stack.top().tokens.end()) {
    return nullptr;
  }
  auto g = tokenstream_stack.top().get();
  if (g->type == t) {

    if (g->type == TokenType::Newline) tokenstream_stack.top().line_number++;

    return g;
  } else {
    tokenstream_stack.top().current--;
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
  if (tokenstream_stack.top().current == tokenstream_stack.top().tokens.end()) {
    printf("Reached end of tokenstream\n");
    exit(1);
  }

  auto curr = tokenstream_stack.top().get();
  if (curr->type != t) {
    printf("Expected token type: %s but got %s, at line %d\n", token_type_to_string(t), token_type_to_string(curr->type), tokenstream_stack.top().line_number);
    exit(1);
  }

  tokenstream_stack.top().line_number++;
}

void print(AstNode* s) {
  if (s->type == AstNodeType::NumberNode) {
    auto node = (NumberNode*)s;
  }
}

Scope* find_symbol_scope(std::string sym, Scope* scope) {
  Scope *s = scope;

  auto first_find = scope->table.find(sym);
  if (first_find != s->table.end()) {
    return scope;
  }

  while (true) {
    if (s->parent) {
      s = s->parent;

      auto sym_find = s->table.find(sym);
      if (sym_find == s->table.end()) {
        continue;
      }

      return s;
    } else {
      // Global
      auto sym_find = global_scope.table.find(sym);
      if (sym_find == global_scope.table.end()) {
        return nullptr;
      }

      return &global_scope;
    }
  }
}

AstNode* find_symbol(std::string sym, Scope* scope) {
  Scope* s = scope;

  auto first_find = scope->table.find(sym);
  if (first_find != s->table.end()) {
    return first_find->second;
  }

  while (true) {
    if (s->parent) {
      s = s->parent;

      auto sym_find = s->table.find(sym);
      if (sym_find == s->table.end()) {
        continue;
      }

      return sym_find->second;
    } else {
      // Global
      auto sym_find = global_scope.table.find(sym);
      if (sym_find == global_scope.table.end()) {
        // NOTE could return nil here
        printf("Failed to find symbol %s\n", sym.c_str());
        exit(1);
      }

      return sym_find->second;
    }
  }
}

char peek(FILE* f) {
  char q = fgetc(f);
  ungetc(q, f);
  return q;
}

AstNode* make_for(std::string sym, AstNode* gen, std::vector<AstNode*> body) {
  ForStmt *node = new ForStmt;
  node->type = AstNodeType::ForStmt;
  node->generator = gen;
  node->body = body;
  node->sym = sym;
  return node;
}

AstNode* make_return(AstNode* a) {
  ReturnNode *r = new ReturnNode;
  r->type = AstNodeType::ReturnNode;
  r->expr = a;
  return r;
}

AstNode *make_operator_expr(OperatorExpr::Op op, AstNode* expr1, AstNode* expr2) {
  OperatorExpr *exp = new OperatorExpr;
  exp->type = AstNodeType::OperatorExpr;
  exp->op = op;
  exp->term1 = expr1;
  exp->term2 = expr2;
  return exp;
}

AstNode *make_fallthrough() {
  FallthroughExpr *exp = new FallthroughExpr;
  exp->type = AstNodeType::FallthroughExpr;
  return exp;
}

AstNode *make_boolean_expr(BooleanExpr::Op op, AstNode *expr1, AstNode *expr2) {
  BooleanExpr *exp = new BooleanExpr;
  exp->type = AstNodeType::BooleanExpr;
  exp->op = op;
  exp->term1 = expr1;
  exp->term2 = expr2;
  return exp;
}

AstNode *make_assignment(std::string sym, AstNode* expr) {
  AssignmentStmt *assignment_stmt = new AssignmentStmt;
  assignment_stmt->type = AstNodeType::AssignmentStmt;
  assignment_stmt->sym = sym;
  assignment_stmt->value = expr;
  return assignment_stmt;
}

AstNode* make_match(AstNode* match_expr, std::vector<std::tuple<AstNode*, std::vector<AstNode*>>> cases) {
  MatchNode* node = new MatchNode;
  node->type = AstNodeType::MatchNode;
  node->match_expr = match_expr;
  node->cases = cases;
  return node;
}

AstNode* make_table_access(AstNode* table, std::string sym, bool breakthrough) {
  TableAccess* node = new TableAccess;
  node->type = AstNodeType::TableAccess;
  node->table = table;
  node->access_sym = sym;
  node->breakthrough = breakthrough;
  return node;
}

AstNode* make_while(AstNode* generator, std::vector<AstNode*> body) {
  WhileStmt *while_stmt = new WhileStmt;

  while_stmt->type = AstNodeType::WhileStmt;
  while_stmt->generator = generator;
  while_stmt->body = body;

  return while_stmt;
}

AstNode* make_module_stmt(std::string s, bool namespaced) {
  ModuleStmt* mod_stmt = new ModuleStmt;
  mod_stmt->type = AstNodeType::ModuleStmt;
  mod_stmt->module = s;
  mod_stmt->namespaced = namespaced;
  return mod_stmt;
}

AstNode* make_table(std::map<std::string, AstNode*> vals) {
  TableNode* table = new TableNode;
  table->type = AstNodeType::TableNode;
  table->table = vals;
  return table;
}

AstNode *make_number(int64_t v) {
  NumberNode *symbol_node = new NumberNode;
  symbol_node->type = AstNodeType::NumberNode;
  //symbol_node->value = strtol(s.c_str(), nullptr, 10);
  symbol_node->value = v;
  return symbol_node;
}

AstNode* make_string(std::string s) {
  StringNode *node = new StringNode;
  node->type = AstNodeType::StringNode;
  node->value = s;
  return node;
}

AstNode *make_boolean(bool b) {
  if (!static_true || !static_false) {
    BooleanNode *true_node = new BooleanNode;
    true_node->type = AstNodeType::BooleanNode;
    true_node->value = true;
    static_true = true_node;

    BooleanNode *false_node = new BooleanNode;
    false_node->type = AstNodeType::BooleanNode;
    false_node->value = false;
    static_false = false_node;
  }

  if (b) {
    return static_true;
  } else {
    return static_false;
  }
}

AstNode *make_symbol(std::string s) {
  SymbolNode *symbol_node = new SymbolNode;
  symbol_node->type = AstNodeType::SymbolNode;
  symbol_node->sym = s;
  return symbol_node;
}

AstNode *make_actor(std::string s, std::map<std::string, FuncStmt*> functions, std::map<std::string, AstNode*> data) {
  ActorDef *actor_def = new ActorDef;
  actor_def->type = AstNodeType::ActorDef;
  actor_def->name = s;
  actor_def->functions = functions;
  actor_def->data = data;
  return actor_def;
}

AstNode *make_function(std::string s, std::vector<std::string> args, std::vector<AstNode*> body) {
  FuncStmt *func_stmt = new FuncStmt;
  func_stmt->type = AstNodeType::FuncStmt;
  func_stmt->name = s;
  func_stmt->args = args;
  func_stmt->body = body;
  return func_stmt;
}

AstNode* make_nop() {
  if (!static_nop) {
    auto nop = new Nop;
    nop->type = AstNodeType::Nop;
    static_nop = nop;
  }
  return static_nop;
}

AstNode *make_function_call(AstNode* sym, std::vector<AstNode*> args) {
  FuncCall *func_call = new FuncCall;
  func_call->type = AstNodeType::FuncCall;
  func_call->sym = (SymbolNode*)sym;
  func_call->args = args;
  return func_call;
}

AstNode * eval(AstNode * obj, Scope *scope);

AstNode *eval_block(std::vector<AstNode *> block, Scope *parent_scope, std::vector<std::tuple<std::string, AstNode*>> sub_syms) {
  Scope block_scope;
  block_scope.parent = parent_scope;

  // load symbols into scope
  for (auto &[sym, node] : sub_syms) {
    block_scope.table[sym] = node;
  }

  AstNode* last_val;
  for (auto node : block) {
    last_val = eval(node, &block_scope);
    if (last_val->type == AstNodeType::ReturnNode) {
      auto v = (ReturnNode*)last_val;
      return eval(v->expr, &block_scope);
    }
  }
  return last_val;
}

AstNode *eval_func(std::string sym, std::vector<AstNode *> args) {
  if (sym == "print") {
    //FIXME implement better std inteorp
    if (args[0]->type == AstNodeType::StringNode) {
      auto nb = (StringNode *)args[0];

      printf("%s\n", nb->value.c_str());
    }

    if (args[0]->type == AstNodeType::BooleanNode) {
      auto nb = (BooleanNode*) args[0];
      if (nb->value) {
        printf("#t\n");
      } else {
        printf("#f\n");
      }
    }

    if (args[0]->type == AstNodeType::NumberNode) {
      printf("%ld\n", ((NumberNode *)args[0])->value);
    }
    return make_nop();
  } else {
    assert(global_scope.table.find(sym) != global_scope.table.end());
    AstNode* obj = global_scope.table[sym];
    FuncStmt *func_def_node = (FuncStmt *)obj;

    std::vector<std::tuple<std::string, AstNode*>> subs;
    for (int i = 0; i < args.size(); ++i) {
      subs.push_back(std::make_tuple(func_def_node->args[i], args[i]));
    }

    return eval_block(func_def_node->body, &global_scope, subs);
  }
}

AstNode *eval(AstNode *obj, Scope *scope = &global_scope) {
  if (obj->type == AstNodeType::AssignmentStmt) {
    auto ass_stmt = (AssignmentStmt *)obj;

    std::string sym;
      sym = ass_stmt->sym;
    auto expr = eval(ass_stmt->value, scope);

    auto find_it = find_symbol_scope(sym, scope);
    if (find_it) {
      find_it->table[sym] = expr;
    } else {
      scope->table[sym] = expr;
    }

    return expr;
  }

  if (obj->type == AstNodeType::OperatorExpr) {
    auto op_expr = (OperatorExpr *)obj;
    auto n1 = ((NumberNode *)eval(op_expr->term1, scope));
    auto n2 = ((NumberNode *)eval(op_expr->term2, scope));

    if (op_expr->op == OperatorExpr::Plus) {
      return make_number(n1->value + n2->value);
    }
  }

  if (obj->type == AstNodeType::ModuleStmt) {
    auto node = (ModuleStmt*)obj;
    //FIXME
    // NOTE just add a "ModuleNode" - when we do a table access, climb the scopes until you find the node, if its a table do a table access, if its a module add the prefix
    //if (node->namespaced) {
    //  prefix_mode = true;
    //  prefix = node->module;
    //} else {
    //  prefix_mode = false;
    //  prefix = "";
    //}

    load_file(node->module + ".x");

    return make_nop();
  }

  if (obj->type == AstNodeType::ReturnNode) {
    return obj;
    //auto node = (ReturnNode*)obj;
    //return eval(node->expr, scope);
  }

  if (obj->type == AstNodeType::NumberNode) {
    return obj;
  }

  if (obj->type == AstNodeType::StringNode) {
    return obj;
  }

  if (obj->type == AstNodeType::Nop) {
    return obj;
  }

  if (obj->type == AstNodeType::TableNode) {
    return obj;
  }

  if (obj->type == AstNodeType::WhileStmt) {
    auto node = (WhileStmt*)obj;

    while (((BooleanNode*)eval(node->generator, scope))->value) {
      eval_block(node->body, scope, {});
    }

    return make_nop();
  }

  if (obj->type == AstNodeType::ForStmt) {
    auto node = (ForStmt*)obj;

    auto table = (TableNode*)eval(node->generator, scope);

    for (int k = 0; k < table->table.size(); ++k) {
      // NOTE push k into sub
      std::vector<std::tuple<std::string, AstNode *>> subs;
      subs.push_back(std::make_tuple(node->sym, make_number(k)));
      eval_block(node->body, scope, subs);
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::TableAccess) {
    auto node = (TableAccess*)obj;

    TableNode* table_node;
    if (node->table->type == AstNodeType::SymbolNode) {
      auto symbol_node = (SymbolNode*) node->table;
      table_node = (TableNode *)find_symbol(symbol_node->sym, scope);
    } else if (node->table->type == AstNodeType::TableNode) {
      table_node = (TableNode*)node->table;
    } else if (node->table->type == AstNodeType::TableAccess) {
      table_node = (TableNode*)eval(node->table, scope);
    } else {
      assert(false);
    }

    if (node->breakthrough) {
      auto sym = find_symbol(node->access_sym, scope);
      assert(sym->type == AstNodeType::NumberNode);
      auto sym_n = (NumberNode*) sym;

      auto find_sym = table_node->table.find(std::to_string(sym_n->value));
      assert(find_sym != table_node->table.end());
      return find_sym->second;
    } else {
      auto find_sym = table_node->table.find(node->access_sym);
      assert(find_sym != table_node->table.end());
      return find_sym->second;
    }
  }

  if (obj->type == AstNodeType::BooleanNode) {
    return obj;
  }

  if (obj->type == AstNodeType::BooleanExpr) {
    auto node = (BooleanExpr*) obj;
    if (node->op == BooleanExpr::GreaterThan || node->op == BooleanExpr::LessThan || node->op == BooleanExpr::GreaterThanEqual || node->op == BooleanExpr::LessThanEqual) {
      auto term1 = eval(node->term1, scope);
      auto term2 = eval(node->term2, scope);

      NumberNode* n1 = (NumberNode*)term1;
      NumberNode* n2 = (NumberNode*)term2;

      switch (node->op) {
      case BooleanExpr::GreaterThan:
        return make_boolean(n1->value > n2->value);
        break;
      case BooleanExpr::LessThan:
        return make_boolean(n1->value < n2->value);
        break;
      case BooleanExpr::GreaterThanEqual:
        return make_boolean(n1->value >= n2->value);
        break;
      case BooleanExpr::LessThanEqual:
        return make_boolean(n1->value <= n2->value);
        break;
      }
    }

    if (node->op == BooleanExpr::Equals) {
    }
    return make_boolean(false);
  }

  if (obj->type == AstNodeType::MatchNode) {
    auto node = (MatchNode*) obj;
    // FIXME only handles boolean
    auto mexpr = (BooleanNode*)eval(node->match_expr, scope);

    for (auto match_case : node->cases) {
      // TODO Make it so the order doesn't matter for fallthrough
      if (std::get<0>(match_case)->type == AstNodeType::FallthroughExpr) {
        return eval_block(std::get<1>(match_case), scope, {});
      } else {
        auto mca_eval = (BooleanNode*)eval(std::get<0>(match_case), scope);
        if (mexpr->value == mca_eval->value) {
            return eval_block(std::get<1>(match_case), scope, {});
        }
      }
    }
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncStmt) {
    auto node = (FuncStmt*)obj;
    std::string sym;
      sym = node->name;
    global_scope.table[sym] = node;
    return make_nop();
  }

  if (obj->type == AstNodeType::FuncCall) {
    auto node = (FuncCall*)obj;
    std::vector<AstNode*> args;

    for (auto arg : node->args) {
      args.push_back(eval(arg, scope));
    }

    return eval_func(node->sym->sym, args);
  }

  if (obj->type == AstNodeType::SymbolNode) {
    return find_symbol(((SymbolNode*)obj)->sym, scope);
  }

  assert(false);
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
    printf("Couldn't parse expression at line %d\n", tokenstream_stack.top().line_number);
    exit(1);
  }
}

AstNode* parse_stmt(int expected_indent = 0) {

  // NOTE create function to accept row of symbols
  Token *t;
  if ((t = accept(TokenType::Symbol))) {

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
        tokenstream_stack.top().go_back(current_indent);
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

    auto stmt = parse_stmt(expected_indent);
    block.push_back(stmt);
  }
  return block;
}

AstNode* parse_function() {
  printf("Parsing function\n");
  // Handle pure
  accept(TokenType::Function);
  auto func_name = accept(TokenType::Symbol);

  std::vector<std::string> args;

  Token* arg;
  expect(TokenType::LeftParen);
  while((arg = accept(TokenType::Symbol))) {
    args.push_back(arg->lexeme);
  }
  expect(TokenType::RightParen);
  expect(TokenType::Newline);

  auto body = parse_block(2);

  return make_function(func_name->lexeme, args, body);
}

AstNode *parse_actor() {
  std::map<std::string, FuncStmt*> functions;
  std::map<std::string, AstNode *> data;

  printf("Parsing actor\n");
  auto actor_name = accept(TokenType::Symbol);

  expect(TokenType::Newline);

  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab)) {
      current_indent++;
    }

    if (current_indent != 1) break;

    FuncStmt* func = (FuncStmt*)parse_function();
    functions[func->name] = func;
    printf("done parsing\n");
  }

  return make_actor(actor_name->lexeme, functions, data);
}

void parse(TokenStream stream) {
  if (accept(TokenType::Import)) {
    // ModuleStmt
    std::string mod_name;

    bool namespaced = true;
    if (accept(TokenType::Star)) {
      namespaced = false;
    }

    Token *sym = tokenstream_stack.top().get();
    expect(TokenType::Newline);
    make_module_stmt(sym->lexeme, namespaced);
    } else if (accept(TokenType::Actor)) {
      parse_actor();
  } else if (accept(TokenType::Newline)) {
    // Skip
    make_nop();
  } else {
    assert(false);
  }
}

void parse_file(std::string path) {
  printf("Loading %s...\n", path.c_str());
  // printf("> ");
  FILE *f = fopen(path.c_str(), "r");
  TokenStream ts;
  ts = tokenize_file(f);

  std::vector<ActorDef*> nodes;

  parse(ts);
}

void load_file(std::string path) {
  printf("Loading %s...\n", path.c_str());
  //printf("> ");
  FILE *f = fopen(path.c_str(), "r");
  TokenStream ts;
  tokenstream_stack.push(ts);
  tokenize_file(f);

  //while (tokenstream_stack.top().current != tokenstream_stack.top().tokens.end()) {
  //  eval(parse(tokenstream_stack.top()));
  //}
  //tokenstream_stack.pop();
}
