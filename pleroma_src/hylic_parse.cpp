#include "hylic_parse.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_tokenizer.h"

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

PType *parse_type() {
  PType *var_type = new PType;

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
    PType *sub_type = parse_type();
    return var_type;
  }

  if (accept(TokenType::LeftBracket)) {
    PType *sub_type = parse_type();
    expect(TokenType::RightBracket);
  } else {
    assert(check(TokenType::Symbol));
    Token *basic_type = accept(TokenType::Symbol);
  }

  return var_type;
}

AstNode *parse_expr(ParseContext* context) {
  if (accept(TokenType::LeftParen)) {
    auto body = parse_expr(context);
    expect(TokenType::RightParen);
    return body;
  } else if (accept(TokenType::Return)) {
    return make_return(parse_expr(context));
  } else if (check(TokenType::String)) {
    auto s = accept(TokenType::String);
    return make_string(s->lexeme);
  } else if (check(TokenType::Symbol)) {
    auto tt = accept(TokenType::Symbol);
    auto expr1 = make_symbol(tt->lexeme);

    // Namespace access
    if (accept(TokenType::Dot)) {
      return make_namespace_access(tt->lexeme, parse_expr(context));
      //return make_table_access(expr1, , bool breakthrough)
    }
    //if (check(TokenType::Dot)) {
    //  AstNode* taccess = nullptr;
    //  while (accept(TokenType::Dot)) {
    //    Token *exp;
    //    if (check(TokenType::Symbol)) {
    //      exp = accept(TokenType::Symbol);
    //    }
    //    if (taccess) {
    //      taccess = make_table_access(taccess, exp->lexeme, false);
    //    } else {
    //      taccess = make_table_access(expr1, exp->lexeme, false);
    //    }
    //  }
    //  return taccess;
    //}

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

      if (context->tl_symbol_table.find(tt->lexeme) != context->tl_symbol_table.end()) {
        // Just mkae this a string!
        auto ret_node = make_create_entity(tt->lexeme, false);
        return ret_node;
      } else {
        // Just mkae this a string! all vars should be strings
        auto ret_node = make_message_node("self", tt->lexeme, MessageDistance::Local, CommMode::Sync, args);
        return ret_node;
      }

      assert(false);
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

      printf("Parse: send message %s to %s\n", target_function->lexeme.c_str(), tt->lexeme.c_str());
      return make_message_node(tt->lexeme, target_function->lexeme, MessageDistance::Local, CommMode::Async, args);
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

    return make_message_node("self", tt->lexeme, MessageDistance::Local, CommMode::Async, args);

  } else if (check(TokenType::True) || check(TokenType::False)) {
    if (accept(TokenType::True)) {
      return make_boolean(true);
    } else if (accept(TokenType::False)) {
      return make_boolean(false);
    }
  } else if (accept(TokenType::LeftBracket)) {

    if (accept(TokenType::RightBracket)) {
      // Empty list
      return make_list({});
    }

    std::vector<AstNode*> list;
    while (true) {
      auto lexpr = parse_expr(context);
      list.push_back(lexpr);

      if (check(TokenType::RightBracket)) {
        break;
      }

      expect(TokenType::Comma);
    }

    expect(TokenType::RightBracket);

    return make_list(list);

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

AstNode *parse_stmt(ParseContext* context, int expected_indent = 0) {

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

      AstNode *expr = parse_expr(context);

      expect(TokenType::Newline);

      return make_assignment(new_variable->lexeme, expr);
    }

    if (accept(TokenType::Equals)) {
      auto expr = parse_expr(context);
      expect(TokenType::Newline);
      return make_assignment(t->lexeme, expr);
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
    printf("doin a promise\n");
    expect(TokenType::Newline);
    auto body = parse_block(context, expected_indent + 1);
    return make_promise_resolution_node(body);
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

std::vector<AstNode *> parse_block(ParseContext* context, int expected_indent = 0) {
  std::vector<AstNode *> block;
  while (true) {
    int current_indent = 0;

    while (accept(TokenType::Tab)) current_indent++;

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

  Token *arg;
  expect(TokenType::LeftParen);
  while ((arg = accept(TokenType::Symbol))) {

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

  auto body = parse_block(context, 2);

  return make_function(func_name->lexeme, args, body);
}

AstNode *parse_actor(ParseContext *context) {
  std::map<std::string, FuncStmt *> functions;
  std::map<std::string, AstNode *> data;

  Token *actor_name;
  if (!(actor_name = accept(TokenType::Symbol))) {
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

    FuncStmt *func = (FuncStmt *)parse_function(context);
    functions[func->name] = func;

    expect(TokenType::Newline);
  }

  return make_actor(actor_name->lexeme, functions, data);
}

std::map<std::string, TLUserType> get_tl_types(TokenStream stream) {

  std::map<std::string, TLUserType> tl_types;

  while (tokenstream.current != tokenstream.tokens.end()) {

    if (accept(TokenType::Actor)) {
      Token* sym = accept(TokenType::Symbol);
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
