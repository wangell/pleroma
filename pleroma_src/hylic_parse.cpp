#include "hylic_parse.h"
#include "common.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_tokenizer.h"
#include <cassert>
#include <filesystem>
#include <functional>
#include <new>
#include <tuple>
#include <cctype>

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

CType *parse_type(ParseContext *ctx) {

  CType *var_type = new CType;
  var_type->start = ctx->ts->current;

  MessageDistance dist = MessageDistance::Local;

  if (ctx->ts->accept(TokenType::LocVar)) {
    dist = MessageDistance::Local;
  } else if (ctx->ts->accept(TokenType::FarVar)) {
    dist = MessageDistance::Far;
  } else if (ctx->ts->accept(TokenType::AlnVar)) {
    dist = MessageDistance::Alien;
  } else if (ctx->ts->accept(TokenType::PromiseType)) {
    // All promises are local
    dist = MessageDistance::Local;
    CType *sub_type = parse_type(ctx);
    var_type->basetype = PType::Promise;
    var_type->subtype = sub_type;
    var_type->end = ctx->ts->current;
    return var_type;
  }

  if (ctx->ts->accept(TokenType::LeftBracket)) {
    CType *sub_type = parse_type(ctx);
    var_type->basetype = PType::List;
    var_type->subtype = sub_type;
    ctx->ts->expect(TokenType::RightBracket);
  } else {
    assert(ctx->ts->check(TokenType::Symbol));
    Token *basic_type = ctx->ts->accept(TokenType::Symbol);

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

  var_type->end = ctx->ts->current;
  return var_type;
}

AstNode *parse_expr(ParseContext *context) {
  std::stack<InfixOp> op_stack;
  std::stack<AstNode *> val_stack;

  while (!context->ts->check(TokenType::Newline) && !context->ts->check(TokenType::RightParen) &&
         !context->ts->check(TokenType::Comma) && !context->ts->check(TokenType::RightBracket)) {
    if (context->ts->accept(TokenType::LeftParen)) {
      auto body = parse_expr(context);
      context->ts->expect(TokenType::RightParen);
      return body;
    } else if (context->ts->check(TokenType::Number)) {
      auto expr1 = make_number(strtol(context->ts->accept(TokenType::Number)->lexeme.c_str(), nullptr, 10));
      val_stack.push(expr1);
    } else if (context->ts->check(TokenType::True)) {
      context->ts->accept(TokenType::True);
      auto expr1 = make_boolean(true);
      val_stack.push(expr1);
    } else if (context->ts->check(TokenType::False)) {
      context->ts->accept(TokenType::False);
      auto expr1 = make_boolean(false);
      val_stack.push(expr1);
    } else if (context->ts->accept(TokenType::Dollar)) {
      // List in future
      // TODO: need to parse expr and then ensure the type is entity
      auto tt = context->ts->accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      if (context->ts->accept(TokenType::LeftParen)) {

        int n_args = 0;
        // While next token does not equal right paren
        while (!context->ts->check(TokenType::RightParen)) {
          auto expr = parse_expr(context);
          val_stack.push(expr);
          n_args++;

          if (!context->ts->check(TokenType::RightParen)) {
            context->ts->expect(TokenType::Comma);
          }
        }

        context->ts->expect(TokenType::RightParen);

        InfixOp op;
        op.type = InfixOpType::NewVat;
        op.n_args = n_args;
        op.name = tt->lexeme;
        op_stack.push(op);
      } else {
        assert(false);
      }

    } else if (context->ts->check(TokenType::LeftBracket)) {
      context->ts->expect(TokenType::LeftBracket);

      std::vector<AstNode *> list;
      while (!context->ts->check(TokenType::RightBracket)) {
        auto expr = parse_expr(context);
        list.push_back(expr);

        if (!context->ts->check(TokenType::RightBracket)) {
          context->ts->expect(TokenType::Comma);
        }
      }

      context->ts->expect(TokenType::RightBracket);

      CType *list_type = new CType;
      if (list.empty()) {
        list_type->basetype = PType::NotAssigned;
      } else {
        list_type->basetype = list[0]->ctype.basetype;
      }

      val_stack.push(make_list(list, list_type));
    } else if (context->ts->check(TokenType::String)) {
      auto expr = context->ts->accept(TokenType::String);
      val_stack.push(make_string(expr->lexeme));
    } else if (context->ts->check(TokenType::Symbol)) {
      auto tt = context->ts->accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      if (context->ts->accept(TokenType::ModUse)) {
        auto expr2 = context->ts->accept(TokenType::String);
        val_stack.push(make_mod_use(tt->lexeme, parse_expr(context)));
      } else if (context->ts->accept(TokenType::LeftParen)) {

        int n_args = 0;
        // While next token does not equal right paren
        while (!context->ts->check(TokenType::RightParen)) {
          auto expr = parse_expr(context);
          val_stack.push(expr);
          n_args++;

          if (!context->ts->check(TokenType::RightParen)) {
            context->ts->expect(TokenType::Comma);
          }
        }

        context->ts->expect(TokenType::RightParen);

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
    } else if (context->ts->check(TokenType::Message)) {
      context->ts->accept(TokenType::Message);
      auto tt = context->ts->accept(TokenType::Symbol);
      auto expr1 = make_symbol(tt->lexeme);

      context->ts->accept(TokenType::LeftParen);

      int n_args = 0;
      // While next token does not equal right paren
      while (!context->ts->check(TokenType::RightParen)) {
        auto expr = parse_expr(context);
        val_stack.push(expr);
        n_args++;

        if (!context->ts->check(TokenType::RightParen)) {
          context->ts->expect(TokenType::Comma);
        }
      }

      context->ts->expect(TokenType::RightParen);

      InfixOp op;
      op.type = InfixOpType::MessageAsync;
      op.n_args = n_args;
      op.name = tt->lexeme;
      op_stack.push(op);

    } else if (context->ts->check(TokenType::Plus)) {
      context->ts->accept(TokenType::Plus);
      InfixOp op;
      op.type = InfixOpType::Plus;
      op.n_args = 2;
      op.name = "Plus";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::Dot)) {
      context->ts->accept(TokenType::Dot);
      InfixOp op;
      op.type = InfixOpType::Namespace;
      op.n_args = 1;
      op.name = "Namespace";
      op_stack.push(op);
      val_stack.push(parse_expr(context));
    } else if (context->ts->check(TokenType::LessThan)) {
      context->ts->accept(TokenType::LessThan);
      InfixOp op;
      op.type = InfixOpType::LessThan;
      op.n_args = 2;
      op.name = "LessThan";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::LessThanEqual)) {
      context->ts->accept(TokenType::LessThanEqual);
      InfixOp op;
      op.type = InfixOpType::LessThanEqual;
      op.n_args = 2;
      op.name = "LessThanEqual";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::GreaterThan)) {
      context->ts->accept(TokenType::GreaterThan);
      InfixOp op;
      op.type = InfixOpType::GreaterThan;
      op.n_args = 2;
      op.name = "GreaterThan";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::GreaterThanEqual)) {
      context->ts->accept(TokenType::GreaterThanEqual);
      InfixOp op;
      op.type = InfixOpType::GreaterThanEqual;
      op.n_args = 2;
      op.name = "GreaterThanEqual";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::EqualsEquals)) {
      context->ts->accept(TokenType::EqualsEquals);
      InfixOp op;
      op.type = InfixOpType::Equals;
      op.n_args = 2;
      op.name = "Equals";
      op_stack.push(op);
    } else {
      printf("%d\n", context->ts->get()->type);
      assert(false);
    }
  }

  AstNode *ret_expr;

  while (!op_stack.empty()) {
    InfixOp op = op_stack.top();
    op_stack.pop();

    if (op.type == InfixOpType::LessThan || op.type == InfixOpType::LessThanEqual || op.type == InfixOpType::GreaterThan || op.type == InfixOpType::GreaterThanEqual || op.type == InfixOpType::Equals) {
      auto expr1 = val_stack.top();
      val_stack.pop();
      auto expr2 = val_stack.top();
      val_stack.pop();

      if (op.type == InfixOpType::LessThan) {
        val_stack.push(make_boolean_expr(BooleanExpr::LessThan, expr2, expr1));
      } else if (op.type == InfixOpType::LessThanEqual) {
        val_stack.push(make_boolean_expr(BooleanExpr::LessThan, expr2, expr1));
      } else if (op.type == InfixOpType::GreaterThan) {
        val_stack.push(make_boolean_expr(BooleanExpr::GreaterThan, expr2, expr1));
      } else if (op.type == InfixOpType::GreaterThanEqual) {
        val_stack.push(make_boolean_expr(BooleanExpr::GreaterThanEqual, expr2, expr1));
      } else if (op.type == InfixOpType::Equals) {
        val_stack.push(make_boolean_expr(BooleanExpr::Equals, expr2, expr1));
      } else {
        assert(false);
      }
    }

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
        if (isupper(op.name[0])) {
          val_stack.push(make_create_entity(op.name, new_vat));
        } else {
          val_stack.push(make_message_node(make_entity_ref(0, 0, 0), op.name, mode, args));
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

AstNode *parse_stmt(ParseContext *context, int expected_indent = 0) {

  // NOTE create function to context->ts->accept row of symbols
  Token *t;
  if ((t = context->ts->accept(TokenType::Symbol))) {

    // Creating a variable
    if (t->lexeme == "let") {
      Token *new_variable = context->ts->accept(TokenType::Symbol);
      SymbolNode *sym_node = (SymbolNode *)make_symbol(new_variable->lexeme);

      context->ts->expect(TokenType::Colon);

      // We can either take a nothing, a loc, or a far
      sym_node->ctype = *parse_type(context);

      context->ts->expect(TokenType::Equals);

      AstNode *expr = parse_expr(context);

      context->ts->expect(TokenType::Newline);

      return make_assignment(sym_node, expr);
    }

    if (context->ts->accept(TokenType::Equals)) {
      SymbolNode *sym_node = (SymbolNode *)make_symbol(t->lexeme);
      auto expr = parse_expr(context);
      context->ts->expect(TokenType::Newline);
      return make_assignment(sym_node, expr);
    } else if (context->ts->accept(TokenType::For)) {
      auto for_generator = parse_expr(context);
      context->ts->expect(TokenType::Newline);

      std::vector<AstNode *> body;
      while (true) {
        int current_indent = 0;

        while (context->ts->accept(TokenType::Tab))
          current_indent++;

        if (current_indent == expected_indent + 1) {
          body.push_back(parse_stmt(context));
          context->ts->expect(TokenType::Newline);
        } else {
          break;
        }
      }

      return make_for(t->lexeme, for_generator, body);
    } else {
      context->ts->go_back(1);
      auto expr = parse_expr(context);
      context->ts->expect(TokenType::Newline);
      return expr;
    }
  } else if (context->ts->accept(TokenType::PromiseType)) {

    auto prom_sym = context->ts->accept(TokenType::Symbol);
    context->ts->expect(TokenType::Newline);
    auto body = parse_block(context, expected_indent + 1);
    return make_promise_resolution_node(prom_sym->lexeme, body);
  } else if (context->ts->accept(TokenType::While)) {
    printf("Parsing while...\n");
    auto while_expr = parse_expr(context);
    context->ts->expect(TokenType::Newline);
    std::vector<AstNode *> body;
    while (true) {
      int current_indent = 0;

      while (context->ts->accept(TokenType::Tab))
        current_indent++;

      if (current_indent == expected_indent + 1) {
        body.push_back(parse_stmt(context, expected_indent + 1));
        context->ts->expect(TokenType::Newline);
      } else {
        break;
      }
    }
    return make_while(while_expr, body);
  } else if (context->ts->accept(TokenType::Return)) {
    auto expr = parse_expr(context);
    // FIXME
    // expect(TokenType::Newline);
    return make_return(expr);
  } else if (context->ts->accept(TokenType::Newline)) {
    // return nop node?
    return nullptr;
  } else if (context->ts->accept(TokenType::Match)) {
    auto match_expr = parse_expr(context);
    context->ts->expect(TokenType::Newline);

    std::vector<std::tuple<AstNode *, std::vector<AstNode *>>> match_cases;
    AstNode *match_case = nullptr;
    std::vector<AstNode *> case_body;
    while (true) {
      int current_indent = 0;

      while (context->ts->accept(TokenType::Tab))
        current_indent++;

      if (current_indent == expected_indent + 1) {
        if (match_case) {
          match_cases.push_back(std::make_tuple(match_case, case_body));
        }
        if (context->ts->accept(TokenType::Fallthrough)) {
          match_case = make_fallthrough();
        } else {
          match_case = parse_expr(context);
        }
        case_body.clear();
        context->ts->expect(TokenType::Newline);
      } else if (current_indent > expected_indent + 1) {
        // FIXME is this the right indnet level?
        case_body.push_back(parse_stmt(context, expected_indent + 2));
      } else if (current_indent < expected_indent + 1) {
        // We're done
        match_cases.push_back(std::make_tuple(match_case, case_body));
        context->ts->go_back(current_indent);
        break;
      }
    }

    return make_match(match_expr, match_cases);
  } else {
    // try parse expr
    auto expr = parse_expr(context);
    context->ts->expect(TokenType::Newline);
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

    while (context->ts->accept(TokenType::Tab))
      current_indent++;

    if (current_indent != expected_indent) {
      context->ts->go_back(current_indent);
      break;
    }

    auto stmt = parse_stmt(context, expected_indent);
    block.push_back(stmt);
  }

  return block;
}

AstNode *parse_function(ParseContext *context) {
  bool pure;
  if (context->ts->accept(TokenType::Function)) {
    pure = false;
  } else if (context->ts->accept(TokenType::Pure)) {
    pure = true;
  }
  auto func_name = context->ts->accept(TokenType::Symbol);

  std::vector<std::string> args;
  std::vector<CType *> param_types;

  Token *arg;
  context->ts->expect(TokenType::LeftParen);
  while ((arg = context->ts->accept(TokenType::Symbol))) {

    context->ts->expect(TokenType::Colon);
    CType *type_spec = parse_type(context);

    args.push_back(arg->lexeme);
    param_types.push_back(type_spec);
  }
  context->ts->expect(TokenType::RightParen);

  // Return type
  context->ts->expect(TokenType::Minus);
  context->ts->expect(TokenType::GreaterThan);

  CType *return_type = parse_type(context);

  context->ts->expect(TokenType::Newline);

  auto body = parse_block(context, 2);

  auto func_stmt = make_function(func_name->lexeme, args, body, param_types);
  func_stmt->ctype = *return_type;
  return func_stmt;
}

AstNode *parse_actor(ParseContext *context) {
  std::map<std::string, FuncStmt *> functions;
  std::map<std::string, AstNode *> data;

  Token *actor_name;
  assert(actor_name = context->ts->accept(TokenType::Symbol));

  // Parse inoculation list
  std::vector<InoCap> inocaps;
  if (context->ts->accept(TokenType::LeftBrace)) {
    while (true) {
      auto cap = context->ts->accept(TokenType::Symbol);
      context->ts->expect(TokenType::Colon);
      auto cap_type = parse_type(context);

      InoCap ic;
      ic.var_name = cap->lexeme;
      ic.ctype = cap_type;
      inocaps.push_back(ic);

      if (context->ts->check(TokenType::RightBrace)) {
        break;
      } else if (context->ts->check(TokenType::Comma)) {
        context->ts->accept(TokenType::Comma);
      } else {
        printf("%d\n", context->ts->peek()->type);
        assert(false);
      }
    }

    context->ts->expect(TokenType::RightBrace);
  }

  context->ts->expect(TokenType::Newline);

  while (true) {
    int current_indent = 0;

    while (context->ts->accept(TokenType::Tab)) {
      current_indent++;
    }

    if (current_indent == 0) {
      // If we're at the end of the file, end it
      if (context->ts->accept(TokenType::EndOfFile)) {
        break;
      }

      if (context->ts->check(TokenType::Actor)) {
        // Spit it back out, return
        context->ts->go_back(1);
        break;
      }

      context->ts->expect_eos();
      continue;
    }

    if (current_indent != 1) {
      context->ts->go_back(current_indent);
      break;
    }

    // Parse data section
    if (context->ts->check(TokenType::Symbol)) {
      Token *data_sym = context->ts->accept(TokenType::Symbol);

      context->ts->expect(TokenType::Colon);

      CType *data_type = parse_type(context);

      data[data_sym->lexeme] = new AstNode;

    } else if (context->ts->check(TokenType::Function)) {
      // Parse functions
      FuncStmt *func = (FuncStmt *)parse_function(context);
      functions[func->name] = func;
    } else {
      assert(false);
    }

    if (context->ts->accept(TokenType::EndOfFile)) {
      break;
    }

    //context->ts->expect(TokenType::Newline);
  }

  return make_actor(context->module, actor_name->lexeme, functions, data, inocaps);
}

std::map<std::string, TLUserType> get_tl_types(TokenStream* ts) {

  std::map<std::string, TLUserType> tl_types;

  while (ts->current != ts->tokens.end()) {

    if (ts->accept(TokenType::Actor)) {
      Token *sym = ts->accept(TokenType::Symbol);
      tl_types[sym->lexeme] = TLUserType::Entity;
    } else {
      ts->get();
    }
  }

  ts->reset();

  return tl_types;
}

HylicModule *parse(TokenStream *stream) {

  ParseContext context;
  context.tl_symbol_table = get_tl_types(stream);
  context.ts = stream;

  std::map<std::string, HylicModule*> imports;
  std::map<std::string, AstNode *> symbol_map;

  HylicModule *hm = new HylicModule;
  context.module = hm;

  while (stream->current != stream->tokens.end()) {
    if (context.ts->accept(TokenType::Import)) {
      // ModuleStmt
      auto mod_name_tok = context.ts->accept(TokenType::Symbol);
      std::string mod_name = mod_name_tok->lexeme;
      std::string mod_path = mod_name + ".po";

      bool namespaced = true;

      if (!std::filesystem::exists(mod_path)) {
        printf("Module %s does not exist.\n", mod_name.c_str());
        exit(1);
      }

      TokenStream *new_stream = tokenize_file(mod_path);
      auto mod_file = parse(new_stream);

      Token *sym = context.ts->get();
      context.ts->expect(TokenType::Newline);
      //imports[mod_name] = mod_file;
      hm->imports[mod_name] = mod_file;
    } else if (context.ts->accept(TokenType::Actor)) {
      EntityDef *actor = (EntityDef *)parse_actor(&context);
      symbol_map[actor->name] = (AstNode *)actor;
      //print_ast(actor);
    } else if (context.ts->accept(TokenType::Newline)) {
      // Skip
      make_nop();
    } else {
      printf("Failed on token %d\n", (*context.ts->current)->type);
      assert(false);
    }
  }

  hm->entity_defs = symbol_map;

  return hm;
}
