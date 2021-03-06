#include "hylic_parse.h"
#include "common.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_tokenizer.h"
#include <functional>
#include <new>
#include <tuple>

struct ParseRes {
  bool success;
  AstNode *node;
};

bool check_type(std::string type) {
  // check type against symbol table

  if (type == "u8")
    return true;

  return false;
}

void expect(TokenType t) {
  if (tokenstream.current == tokenstream.tokens.end()) {
    printf("Reached end of tokenstream\n");
    exit(1);
  }

  auto curr = tokenstream.get();
  if (curr->type != t) {
    printf("Expected token type: %s but got %s(%d), at line %d\n",
           token_type_to_string(t), token_type_to_string(curr->type),
           curr->type, tokenstream.line_number);
    exit(1);
  }

  if (curr->type == TokenType::Newline)
    tokenstream.line_number++;
}

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

std::vector<Token *> accept_all(std::vector<TokenType> toks) {
  assert(toks.size() > 0);

  // FIXME need to check for toks.length until end
  if (tokenstream.current == tokenstream.tokens.end()) {
    return {};
  }

  std::vector<Token *> return_toks;

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

Token *accept(TokenType t) {
  if (tokenstream.current == tokenstream.tokens.end()) {
    return nullptr;
  }
  auto g = tokenstream.get();
  if (g->type == t) {

    if (g->type == TokenType::Newline)
      tokenstream.line_number++;

    return g;
  } else {
    tokenstream.current--;
    return nullptr;
  }
}

CType *parse_type() {

  CType *var_type = new CType;
  var_type->start = tokenstream.current;

  MessageDistance dist = MessageDistance::Local;

  if (accept(TokenType::LocVar)) {
    dist = MessageDistance::Local;
  } else if (accept(TokenType::FarVar)) {
    dist = MessageDistance::Far;
  } else if (accept(TokenType::AlnVar)) {
    dist = MessageDistance::Alien;
  } else if (accept(TokenType::PromiseType)) {
    // All promises are local
    dist = MessageDistance::Local;
    CType *sub_type = parse_type();
    var_type->basetype = PType::Promise;
    var_type->subtype = sub_type;
    var_type->end = tokenstream.current;
    return var_type;
  }

  if (accept(TokenType::LeftBracket)) {
    CType *sub_type = parse_type();
    var_type->basetype = PType::List;
    var_type->subtype = sub_type;
    expect(TokenType::RightBracket);
  } else {
    assert(check(TokenType::Symbol));
    Token *basic_type = accept(TokenType::Symbol);

    // Move this to tokenizer
    if (basic_type->lexeme == "u8") {
      var_type->basetype = PType::u8;
    } else if (basic_type->lexeme == "str") {
      var_type->basetype = PType::str;
    } else if (basic_type->lexeme == "void") {
      var_type->basetype = PType::None;
    } else {
      var_type->basetype = PType::Entity;
      var_type->entity_name = basic_type->lexeme;
    }
  }

  var_type->end = tokenstream.current;
  return var_type;
}

AstNode *parse_expr(ParseContext *context) {
  std::stack<InfixOp> op_stack;
  std::stack<AstNode *> val_stack;

  while (!check(TokenType::Newline) && !check(TokenType::RightParen) &&
         !check(TokenType::Comma) && !check(TokenType::RightBracket)) {
    if (accept(TokenType::LeftParen)) {
      auto body = parse_expr(context);
      expect(TokenType::RightParen);
      return body;
    } else if (check(TokenType::Number)) {
      auto expr1 = make_number(
          strtol(accept(TokenType::Number)->lexeme.c_str(), nullptr, 10));
      val_stack.push(expr1);
    } else if (accept(TokenType::Dollar)) {
      // List in future
      auto tt = accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      if (accept(TokenType::LeftParen)) {

        int n_args = 0;
        // While next token does not equal right paren
        while (!check(TokenType::RightParen)) {
          auto expr = parse_expr(context);
          val_stack.push(expr);
          n_args++;

          if (!check(TokenType::RightParen)) {
            expect(TokenType::Comma);
          }
        }

        expect(TokenType::RightParen);
        printf("ya\n");

        InfixOp op;
        op.type = InfixOpType::NewVat;
        op.n_args = n_args;
        op.name = tt->lexeme;
        op_stack.push(op);
      } else {
        assert(false);
      }

    } else if (check(TokenType::LeftBracket)) {
      expect(TokenType::LeftBracket);

      std::vector<AstNode *> list;
      while (!check(TokenType::RightBracket)) {
        auto expr = parse_expr(context);
        list.push_back(expr);

        if (!check(TokenType::RightBracket)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightBracket);

      CType *list_type = new CType;
      if (list.empty()) {
        list_type->basetype = PType::NotAssigned;
      } else {
        list_type->basetype = list[0]->ctype.basetype;
      }

      val_stack.push(make_list(list, list_type));
    } else if (check(TokenType::String)) {
      auto expr = accept(TokenType::String);
      val_stack.push(make_string(expr->lexeme));
    } else if (check(TokenType::Symbol)) {
      auto tt = accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      if (accept(TokenType::LeftParen)) {

        int n_args = 0;
        // While next token does not equal right paren
        while (!check(TokenType::RightParen)) {
          auto expr = parse_expr(context);
          val_stack.push(expr);
          n_args++;

          if (!check(TokenType::RightParen)) {
            expect(TokenType::Comma);
          }
        }

        expect(TokenType::RightParen);

        InfixOp op;
        op.type = InfixOpType::MessageSync;
        op.n_args = n_args;
        op.name = tt->lexeme;
        op_stack.push(op);
        // val_stack.push(make_message_node(, tt->lexeme, CommMode::Sync,
        // args));
      } else {

        // TODO make a keyword
        if (tt->lexeme == "self") {
          val_stack.push(make_entity_ref(0, 0, 0));
        } else {
          val_stack.push(expr1);
        }
      }
    } else if (check(TokenType::Message)) {
      accept(TokenType::Message);
      auto tt = accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      accept(TokenType::LeftParen);

      int n_args = 0;
      // While next token does not equal right paren
      while (!check(TokenType::RightParen)) {
        auto expr = parse_expr(context);
        val_stack.push(expr);
        n_args++;

        if (!check(TokenType::RightParen)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightParen);

      InfixOp op;
      op.type = InfixOpType::MessageAsync;
      op.n_args = n_args;
      op.name = tt->lexeme;
      op_stack.push(op);

    } else if (check(TokenType::Plus)) {
      accept(TokenType::Plus);
      InfixOp op;
      op.type = InfixOpType::Plus;
      op.n_args = 2;
      op.name = "Plus";
      op_stack.push(op);
    } else if (check(TokenType::Dot)) {
      accept(TokenType::Dot);
      InfixOp op;
      op.type = InfixOpType::Namespace;
      op.n_args = 1;
      op.name = "Namespace";
      op_stack.push(op);
      val_stack.push(parse_expr(context));
    } else {
      printf("%d\n", tokenstream.get()->type);
      assert(false);
    }
  }

  AstNode *ret_expr;

  while (!op_stack.empty()) {
    InfixOp op = op_stack.top();
    op_stack.pop();

    if (op.type == InfixOpType::Plus) {
      auto expr1 = val_stack.top();
      val_stack.pop();
      auto expr2 = val_stack.top();
      val_stack.pop();

      val_stack.push(make_operator_expr(OperatorExpr::Plus, expr1, expr2));
    }

    if (op.type == InfixOpType::Namespace) {
      // Field
      auto expr2 = val_stack.top();
      val_stack.pop();

      // Accessing
      auto expr1 = val_stack.top();
      val_stack.pop();

      val_stack.push(make_namespace_access(expr1, expr2));
    }

    if (op.type == InfixOpType::MessageSync ||
        op.type == InfixOpType::MessageAsync ||
        op.type == InfixOpType::NewVat) {
      std::vector<AstNode *> args;
      for (int k = 0; k < op.n_args; ++k) {
        args.push_back(val_stack.top());
        val_stack.pop();
      }

      CommMode mode;
      if (op.type == InfixOpType::MessageAsync) {
        mode = CommMode::Async;
      } else {
        mode = CommMode::Sync;
      }

      if (val_stack.empty()) {
        bool new_vat = false;
        if (op.type == InfixOpType::NewVat) {
          new_vat = true;
        }
        // FIXME
        if (op.name == "Net") {
          val_stack.push(make_create_entity("Net", new_vat));
        } else if (op.name == "Io") {
          val_stack.push(make_create_entity("Io", new_vat));
        } else if (context->tl_symbol_table.find(op.name) !=
                   context->tl_symbol_table.end()) {
          val_stack.push(make_create_entity(op.name, new_vat));
        } else {
          val_stack.push(
              make_message_node(make_entity_ref(0, 0, 0), op.name, mode, args));
        }
      } else {
        auto topstack = val_stack.top();
        val_stack.pop();
        val_stack.push(make_message_node(topstack, op.name, mode, args));
      }
    }
  }

  assert(val_stack.size() == 1);

  return val_stack.top();
}

AstNode *parse_expr_old(ParseContext *context) {
  if (accept(TokenType::LeftParen)) {
    auto body = parse_expr(context);
    expect(TokenType::RightParen);
    return body;
  } else if (accept(TokenType::Return)) {
    return make_return(parse_expr(context));
  } else if (check(TokenType::String)) {
    // FIXME - handle operator expressions
    auto s = accept(TokenType::String);
    return make_string(s->lexeme);
  } else if (check(TokenType::Symbol)) {
    auto tt = accept(TokenType::Symbol);
    auto expr1 = make_symbol(tt->lexeme);

    // Namespace access
    if (accept(TokenType::Dot)) {
      // return make_namespace_access(tt->lexeme, parse_expr(context));
    }

    if (accept(TokenType::LeftParen)) {

      std::vector<AstNode *> args;
      // While next token does not equal right paren
      while (!check(TokenType::RightParen)) {
        auto expr = parse_expr(context);
        args.push_back(expr);

        if (!check(TokenType::RightParen)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightParen);

      // return make_message_node(tt->lexeme, CommMode::Sync, args);
    }

    if (accept(TokenType::Message)) {

      auto target_function = accept(TokenType::Symbol);

      expect(TokenType::LeftParen);

      std::vector<AstNode *> args;
      // While next token does not equal right paren
      while (!check(TokenType::RightParen)) {
        auto expr = parse_expr(context);
        args.push_back(expr);

        if (!check(TokenType::RightParen)) {
          expect(TokenType::Comma);
        }
      }

      expect(TokenType::RightParen);

      printf("Parse: send message %s to %s\n", target_function->lexeme.c_str(),
             tt->lexeme.c_str());
      // return make_message_node(target_function->lexeme, CommMode::Async,
      // args);
    }

    if (accept(TokenType::Plus)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Plus, expr1, expr2);
    } else if (accept(TokenType::Minus)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Minus, expr1, expr2);
    } else if (accept(TokenType::Star)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Times, expr1, expr2);
    } else if (accept(TokenType::Slash)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Divide, expr1, expr2);
    } else if (accept(TokenType::LessThan)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::LessThan, expr1, expr2);
    } else if (accept(TokenType::LessThanEqual)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::LessThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThanEqual)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::GreaterThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThan)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::GreaterThan, expr1, expr2);
    }

    return expr1;
  } else if (check(TokenType::Number)) {
    auto expr1 = make_number(
        strtol(accept(TokenType::Number)->lexeme.c_str(), nullptr, 10));
    if (accept(TokenType::Plus)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Plus, expr1, expr2);
    }
    if (accept(TokenType::Minus)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Minus, expr1, expr2);
    }
    if (accept(TokenType::Star)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Times, expr1, expr2);
    }

    if (accept(TokenType::Slash)) {
      auto expr2 = parse_expr(context);
      return make_operator_expr(OperatorExpr::Divide, expr1, expr2);
    }

    if (accept(TokenType::LessThan)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::LessThan, expr1, expr2);
    } else if (accept(TokenType::LessThanEqual)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::LessThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThanEqual)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::GreaterThanEqual, expr1, expr2);
    } else if (accept(TokenType::GreaterThan)) {
      auto expr2 = parse_expr(context);
      return make_boolean_expr(BooleanExpr::GreaterThan, expr1, expr2);
    }

    return expr1;
  } else if (accept(TokenType::Message)) {
    auto tt = accept(TokenType::Symbol);
    expect(TokenType::LeftParen);

    std::vector<AstNode *> args;
    // While next token does not equal right paren
    while (!check(TokenType::RightParen)) {
      auto expr = parse_expr(context);
      args.push_back(expr);

      if (!check(TokenType::RightParen)) {
        expect(TokenType::Comma);
      }
    }

    expect(TokenType::RightParen);

    assert(false);
    // return make_message_node(tt->lexeme, CommMode::Async, args);

  } else if (check(TokenType::True) || check(TokenType::False)) {
    if (accept(TokenType::True)) {
      return make_boolean(true);
    } else if (accept(TokenType::False)) {
      return make_boolean(false);
    }
  } else if (accept(TokenType::LeftBracket)) {

    CType *ctype = new CType;
    ctype->basetype = PType::NotAssigned;

    if (accept(TokenType::RightBracket)) {
      // Empty list
      return make_list({}, ctype);
    }

    std::vector<AstNode *> list;
    while (true) {
      auto lexpr = parse_expr(context);
      list.push_back(lexpr);

      if (check(TokenType::RightBracket)) {
        break;
      }

      expect(TokenType::Comma);
    }

    expect(TokenType::RightBracket);

    // FIXME
    ctype->basetype = list[0]->ctype.basetype;
    ctype->subtype = list[0]->ctype.subtype;

    return make_list(list, ctype);

  } else if (accept(TokenType::LeftBrace)) {
    // While next token does not equal right paren
    std::map<std::string, AstNode *> table_vals;
    while (!check(TokenType::RightBrace)) {

      std::string label;

      Token *t;
      if ((t = accept(TokenType::Symbol))) {
        label = t->lexeme;
      }

      if ((t = accept(TokenType::Number))) {
        label = t->lexeme;
      }

      expect(TokenType::Colon);

      auto expr = parse_expr(context);

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

AstNode *parse_stmt(ParseContext *context, int expected_indent = 0) {

  // NOTE create function to accept row of symbols
  Token *t;
  if ((t = accept(TokenType::Symbol))) {

    // Creating a variable
    if (t->lexeme == "let") {
      Token *new_variable = accept(TokenType::Symbol);
      SymbolNode *sym_node = (SymbolNode *)make_symbol(new_variable->lexeme);

      expect(TokenType::Colon);

      // We can either take a nothing, a loc, or a far
      sym_node->ctype = *parse_type();

      expect(TokenType::Equals);

      AstNode *expr = parse_expr(context);

      expect(TokenType::Newline);

      return make_assignment(sym_node, expr);
    }

    if (accept(TokenType::Equals)) {
      SymbolNode *sym_node = (SymbolNode *)make_symbol(t->lexeme);
      auto expr = parse_expr(context);
      expect(TokenType::Newline);
      return make_assignment(sym_node, expr);
    } else if (accept(TokenType::For)) {
      auto for_generator = parse_expr(context);
      expect(TokenType::Newline);

      std::vector<AstNode *> body;
      while (true) {
        int current_indent = 0;

        while (accept(TokenType::Tab))
          current_indent++;

        if (current_indent == expected_indent + 1) {
          body.push_back(parse_stmt(context));
          expect(TokenType::Newline);
        } else {
          break;
        }
      }

      return make_for(t->lexeme, for_generator, body);
    } else {
      tokenstream.go_back(1);
      auto expr = parse_expr(context);
      expect(TokenType::Newline);
      return expr;
    }
  } else if (accept(TokenType::PromiseType)) {

    auto prom_sym = accept(TokenType::Symbol);
    expect(TokenType::Newline);
    auto body = parse_block(context, expected_indent + 1);
    return make_promise_resolution_node(prom_sym->lexeme, body);
  } else if (accept(TokenType::While)) {
    printf("Parsing while...\n");
    auto while_expr = parse_expr(context);
    expect(TokenType::Newline);
    std::vector<AstNode *> body;
    while (true) {
      int current_indent = 0;

      while (accept(TokenType::Tab))
        current_indent++;

      if (current_indent == expected_indent + 1) {
        body.push_back(parse_stmt(context, expected_indent + 1));
        expect(TokenType::Newline);
      } else {
        break;
      }
    }
    return make_while(while_expr, body);
  } else if (accept(TokenType::Return)) {
    auto expr = parse_expr(context);
    // FIXME
    // expect(TokenType::Newline);
    return make_return(expr);
  } else if (accept(TokenType::Newline)) {
    // return nop node?
    return nullptr;
  } else if (accept(TokenType::Match)) {
    auto match_expr = parse_expr(context);
    expect(TokenType::Newline);

    std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> match_cases;
    AstNode *match_case = nullptr;
    std::vector<AstNode *> case_body;
    while (true) {
      int current_indent = 0;

      while (accept(TokenType::Tab))
        current_indent++;

      if (current_indent == expected_indent + 1) {
        if (match_case) {
          match_cases.push_back(std::make_tuple(match_case, case_body));
        }
        if (accept(TokenType::Fallthrough)) {
          match_case = make_fallthrough();
        } else {
          match_case = parse_expr(context);
        }
        case_body.clear();
        expect(TokenType::Newline);
      } else if (current_indent > expected_indent + 1) {
        // FIXME is this the right indnet level?
        case_body.push_back(parse_stmt(context, expected_indent + 2));
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
    auto expr = parse_expr(context);
    expect(TokenType::Newline);
    return expr;
  }

  // return parse_expr();

  assert(false);
}

std::vector<AstNode *> parse_block(ParseContext *context,
                                   int expected_indent = 0) {
  std::vector<AstNode *> block;
  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab))
      current_indent++;

    if (current_indent != expected_indent) {
      tokenstream.go_back(current_indent);
      break;
    }

    auto stmt = parse_stmt(context, expected_indent);
    block.push_back(stmt);
  }
  return block;
}

AstNode *parse_function(ParseContext *context) {
  bool pure;
  if (accept(TokenType::Function)) {
    pure = false;
  } else if (accept(TokenType::Pure)) {
    pure = true;
  }
  auto func_name = accept(TokenType::Symbol);

  std::vector<std::string> args;
  std::vector<CType *> param_types;

  Token *arg;
  expect(TokenType::LeftParen);
  while ((arg = accept(TokenType::Symbol))) {

    expect(TokenType::Colon);
    CType *type_spec = parse_type();

    args.push_back(arg->lexeme);
    param_types.push_back(type_spec);
  }
  expect(TokenType::RightParen);

  // Return type
  expect(TokenType::Minus);
  expect(TokenType::GreaterThan);

  CType *return_type = parse_type();

  expect(TokenType::Newline);

  auto body = parse_block(context, 2);

  auto func_stmt = make_function(func_name->lexeme, args, body, param_types);
  func_stmt->ctype = *return_type;
  return func_stmt;
}

AstNode *parse_actor(ParseContext *context) {
  std::map<std::string, FuncStmt *> functions;
  std::map<std::string, AstNode *> data;

  Token *actor_name;
  assert(actor_name = accept(TokenType::Symbol));

  // Parse inoculation list
  if (accept(TokenType::LeftBrace)) {
    while (true) {
      accept(TokenType::Symbol);
      expect(TokenType::Colon);
      parse_type();

      if (check(TokenType::RightBrace)) {
        break;
      } else if (check(TokenType::Comma)) {
        accept(TokenType::Comma);
      } else {
        printf("%d\n", tokenstream.peek()->type);
        assert(false);
      }
    }

    expect(TokenType::RightBrace);
  }

  expect(TokenType::Newline);

  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab)) {
      current_indent++;
    }

    if (current_indent == 0) {
      // If we're at the end of the file, end it
      if (tokenstream.current == tokenstream.tokens.end()) {
        break;
      }

      if (check(TokenType::Actor)) {
        // Spit it back out, return
        tokenstream.go_back(1);
        break;
      }

      expect(TokenType::Newline);
      continue;
    }

    if (current_indent != 1) {
      tokenstream.go_back(current_indent);
      break;
    }

    // Parse data section
    if (check(TokenType::Symbol)) {
      Token *data_sym = accept(TokenType::Symbol);

      expect(TokenType::Colon);

      CType *data_type = parse_type();

      data[data_sym->lexeme] = new AstNode;

    } else if (check(TokenType::Function)) {
      // Parse functions
      FuncStmt *func = (FuncStmt *)parse_function(context);
      functions[func->name] = func;
    } else {
      assert(false);
    }

    expect(TokenType::Newline);
  }

  return make_actor(actor_name->lexeme, functions, data);
}

std::map<std::string, TLUserType> get_tl_types(TokenStream stream) {

  std::map<std::string, TLUserType> tl_types;

  while (tokenstream.current != tokenstream.tokens.end()) {

    if (accept(TokenType::Actor)) {
      Token *sym = accept(TokenType::Symbol);
      tl_types[sym->lexeme] = TLUserType::Entity;
    } else {
      tokenstream.get();
    }
  }

  tokenstream.reset();

  return tl_types;
}

std::map<std::string, AstNode *> parse(TokenStream stream) {

  ParseContext context;
  context.tl_symbol_table = get_tl_types(stream);

  std::map<std::string, AstNode *> symbol_map;

  while (tokenstream.current != tokenstream.tokens.end()) {
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
      EntityDef *actor = (EntityDef *)parse_actor(&context);
      symbol_map[actor->name] = (AstNode *)actor;
      print_ast(actor);
    } else if (accept(TokenType::Newline)) {
      // Skip
      make_nop();
    } else {
      printf("Failed on token %d\n", (*tokenstream.current)->type);
      assert(false);
    }
  }

  return symbol_map;
}
