#include "hylic_parse.h"
#include "common.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_tokenizer.h"
#include "system.h"
#include <cassert>
#include <filesystem>
#include <functional>
#include <new>
#include <tuple>
#include <cctype>
#include "general_util.h"

struct ParseRes {
  bool success;
  AstNode *node;
};

// FIXME generalize to walk nodes func
int count_expression_tokens(AstNode* node) {
  switch (node->type) {
  case AstNodeType::NumberNode: {
    return 1;
  } break;
  }
  panic("");
}

void eat_newlines(ParseContext *ctx) {
  while (ctx->ts->accept(TokenType::Newline));
}

CType *parse_type(ParseContext *ctx) {

  CType *var_type = new CType;
  var_type->start = ctx->ts->current;

  DType dist = DType::Local;

  if (ctx->ts->accept(TokenType::LocVar)) {
    dist = DType::Local;
  } else if (ctx->ts->accept(TokenType::FarVar)) {
    dist = DType::Far;
  } else if (ctx->ts->accept(TokenType::AlnVar)) {
    dist = DType::Alien;
  } else if (ctx->ts->accept(TokenType::PromiseType)) {
    // All promises are local
    dist = DType::Local;
    CType *sub_type = parse_type(ctx);
    var_type->dtype = DType::Local;
    var_type->basetype = PType::Promise;
    var_type->subtype = sub_type;
    var_type->end = ctx->ts->current;
    return var_type;
  }

  if (ctx->ts->accept(TokenType::LeftBracket)) {
    CType *sub_type = parse_type(ctx);
    var_type->basetype = PType::List;
    var_type->subtype = sub_type;
    var_type->dtype = DType::Local;
    ctx->ts->expect(TokenType::RightBracket);
  } else {
    assert(ctx->ts->check(TokenType::Symbol));
    Token *basic_type = ctx->ts->accept(TokenType::Symbol);

    // Move this to tokenizer
    if (basic_type->lexeme == "u8") {
      var_type->basetype = PType::u8;
      var_type->dtype = DType::Local;
    } else if (basic_type->lexeme == "str") {
      var_type->basetype = PType::str;
      var_type->dtype = DType::Local;
    } else if (basic_type->lexeme == "void") {
      var_type->basetype = PType::None;
    } else if (basic_type->lexeme == "Entity") {
      var_type->basetype = PType::BaseEntity;
      var_type->dtype = dist;
    } else {
      std::string final_type = basic_type->lexeme;

      while (ctx->ts->accept(TokenType::ModUse)) {
        final_type += "►" + ctx->ts->accept(TokenType::Symbol)->lexeme;
      }
      var_type->basetype = PType::Entity;
      var_type->dtype = dist;
      var_type->entity_name = final_type;
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
    } else if (context->ts->check(TokenType::Self)) {
      context->ts->accept(TokenType::Self);
      auto expr1 = make_self();
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
        val_stack.push(expr1);
      }
    } else if (context->ts->check(TokenType::Dot)) {
      context->ts->accept(TokenType::Dot);

      // Range vs local call
      if (context->ts->accept(TokenType::Dot)) {
        InfixOp op;
        op.type = InfixOpType::Range;
        op_stack.push(op);
        val_stack.push(parse_expr(context));
      } else {
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
        op.type = InfixOpType::MessageSync;
        op.n_args = n_args;
        op.name = tt->lexeme;
        op_stack.push(op);
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
    } else if (context->ts->check(TokenType::IndexStart)) {
      context->ts->accept(TokenType::IndexStart);
      auto ind_expr = parse_expr(context);
      context->ts->expect(TokenType::IndexEnd);
      NumberNode* blah = (NumberNode*)ind_expr;
      printf("%s\n", ast_type_to_string(blah->type).c_str());
      val_stack.push(ind_expr);
      InfixOp op;
      op.type = InfixOpType::Index;
      op.n_args = 1;
      op.name = "Index";
      op_stack.push(op);
    } else if (context->ts->check(TokenType::Plus)) {
      context->ts->accept(TokenType::Plus);
      InfixOp op;
      op.type = InfixOpType::Plus;
      op.n_args = 2;
      op.name = "Plus";
      op_stack.push(op);
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
      // FIXME not sure if we should break here
      break;
      printf("%s\n", token_type_to_string(context->ts->get()->type));
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

    if (op.type == InfixOpType::Range) {
      auto expr2 = val_stack.top();
      val_stack.pop();

      auto expr1 = val_stack.top();
      val_stack.pop();

      val_stack.push(make_range(expr1, expr2));
    }

    if (op.type == InfixOpType::Index) {
      auto expr2 = val_stack.top();
      val_stack.pop();

      auto expr1 = val_stack.top();
      val_stack.pop();

      val_stack.push(make_index_node(expr2, expr1));
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
        // FIXME: All entity names must be capitalized? keep it?
        if (isupper(op.name[0])) {
          auto ent = make_create_entity(op.name, new_vat);
          val_stack.push(ent);
        } else {
          // FIXME Replace with keyword self
          std::reverse(args.begin(), args.end());
          val_stack.push(make_message_node(make_self(), op.name, mode, args));
        }
      } else {
        auto topstack = val_stack.top();
        val_stack.pop();
        std::reverse(args.begin(), args.end());
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
  if (context->ts->accept(TokenType::Comment)) {
    printf("getting comment\n");
    std::string comment;
    Token *comment_t;
    while ((comment_t = context->ts->get())->type != TokenType::Comment) {
      comment += comment_t->lexeme;
    }
    eat_newlines(context);
    return make_comment(comment);

  } else if ((t = context->ts->accept(TokenType::Symbol))) {

    // Creating a variable
    if (t->lexeme == "let") {
      Token *new_variable = context->ts->accept(TokenType::Symbol);
      SymbolNode *sym_node = (SymbolNode *)make_symbol(new_variable->lexeme);

      context->ts->expect(TokenType::Colon);

      // We can either take a nothing, a loc, or a far
      sym_node->ctype = *parse_type(context);

      context->ts->expect(TokenType::Equals);

      AstNode *expr = parse_expr(context);

      eat_newlines(context);

      return make_assignment(sym_node, expr);
    }

    if (context->ts->accept(TokenType::IndexStart)) {
      auto expr = parse_expr(context);
      context->ts->expect(TokenType::IndexEnd);

      printf("Here %d!\n", count_expression_tokens(expr));
      context->ts->go_back(1 + count_expression_tokens(expr) + 1);

        //if (context->ts->accept(TokenType::Message)) {
        //  // Need to go back all previous tokens start + end + count all inner expr tokens, then go_back(start + end + inner)
        //  panic("Unhandled");
        //} else if (context->ts->accept(TokenType::Equals)) {
        //  AstNode *expr2 = parse_expr(context);
        //  eat_newlines(context);

        //  return make_assignment(make_index_node(make_symbol(t->lexeme), expr), expr2);
        //} else {
        //  panic("Unhandled operator for index statement");
        //}
    }

    if (context->ts->accept(TokenType::Equals)) {
      SymbolNode *sym_node = (SymbolNode *)make_symbol(t->lexeme);
      auto expr = parse_expr(context);
      eat_newlines(context);
      return make_assignment(sym_node, expr);
    } else if (context->ts->accept(TokenType::For)) {
      auto for_generator = parse_expr(context);
      eat_newlines(context);

      std::vector<AstNode *> body;
      while (true) {
        int current_indent = 0;

        while (context->ts->accept(TokenType::Tab))
          current_indent++;

        if (current_indent == expected_indent + 1) {
          body.push_back(parse_stmt(context));
        } else {
          context->ts->go_back(current_indent);
          break;
        }
      }

      return make_for(t->lexeme, for_generator, body);
    } else {
      context->ts->go_back(1);
      auto expr = parse_expr(context);
      eat_newlines(context);
      return expr;
    }
  } else if (context->ts->accept(TokenType::PromiseType)) {

    auto prom_sym = context->ts->accept(TokenType::Symbol);
    eat_newlines(context);
    auto body = parse_block(context, expected_indent + 1);
    return make_promise_resolution_node(prom_sym->lexeme, body);
  } else if (context->ts->accept(TokenType::While)) {
    auto while_expr = parse_expr(context);
    eat_newlines(context);
    std::vector<AstNode *> body;
    while (true) {
      int current_indent = 0;

      while (context->ts->accept(TokenType::Tab))
        current_indent++;

      if (current_indent == expected_indent + 1) {
        body.push_back(parse_stmt(context, expected_indent + 1));
      } else {
        context->ts->go_back(current_indent);
        break;
      }
    }
    return make_while(while_expr, body);
  } else if (context->ts->accept(TokenType::Return)) {
    auto expr = parse_expr(context);
    eat_newlines(context);
    return make_return(expr);
  } else if (context->ts->accept(TokenType::Newline)) {
    // This shouldn't happen ever
    eat_newlines(context);
    panic("");
    return nullptr;
  } else if (context->ts->accept(TokenType::Match)) {
    auto match_expr = parse_expr(context);
    eat_newlines(context);

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
        eat_newlines(context);
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
    eat_newlines(context);
    return expr;
  }

  // return parse_expr();

  panic("Unhandled parse");
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

    if (!context->ts->accept(TokenType::Comma)) {
      break;
    }
  }
  context->ts->expect(TokenType::RightParen);

  // Return type
  context->ts->expect(TokenType::Minus);
  context->ts->expect(TokenType::GreaterThan);

  CType *return_type = parse_type(context);

  eat_newlines(context);

  auto body = parse_block(context, 2);

  auto func_stmt = make_function(func_name->lexeme, args, body, param_types, pure);
  func_stmt->ctype = *return_type;
  return func_stmt;
}

AstNode *parse_record(ParseContext *context) {

  context->ts->accept(TokenType::Symbol);

  eat_newlines(context);

  while (true) {
    int current_indent = 0;

    while (context->ts->accept(TokenType::Tab))
      current_indent++;

    if (current_indent > 1) {
      panic("Invalid record type");
    }

    if (current_indent == 0) {
      printf("Breaking!\n");
      break;
    }

    // Valid record

    context->ts->accept(TokenType::Symbol);

    if (context->ts->accept(TokenType::LeftBrace)) {
      // Named

      while(!context->ts->check(TokenType::RightBrace)) {
        context->ts->accept(TokenType::Symbol);
        context->ts->accept(TokenType::Colon);
        parse_type(context);

        if (!context->ts->check(TokenType::Comma)) {
          break;
        }

        context->ts->accept(TokenType::Comma);
      }

      context->ts->accept(TokenType::RightBrace);

    } else if (context->ts->accept(TokenType::LeftParen)) {
      // Anonymous
      while (!context->ts->check(TokenType::RightParen)) {
        parse_type(context);

        if (!context->ts->check(TokenType::Comma)) {
          break;
        }

        context->ts->accept(TokenType::Comma);
      }

      context->ts->accept(TokenType::RightParen);
    } else {
      panic("Invalid record member");
    }

    eat_newlines(context);
  }
  printf("HERE!\n");

  eat_newlines(context);

  return make_nop();
}

AstNode *parse_actor(ParseContext *context) {
  std::map<std::string, FuncStmt *> functions;
  std::map<std::string, AstNode *> data;
  std::vector<std::string> preamble;

  Token *actor_name;
  assert(actor_name = context->ts->accept(TokenType::Symbol));

  // Parse inoculation list
  std::vector<InoCap> inocaps;
  if (context->ts->accept(TokenType::LeftBrace)) {
    while (true) {
      auto cap = context->ts->accept(TokenType::Symbol);

      // Empty inoc table
      if (!cap) {
        break;
      }
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
        panic("Invalid inocap");
      }
    }

    context->ts->expect(TokenType::RightBrace);
  }

  eat_newlines(context);

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

    if (context->ts->accept(TokenType::Minus)) {
      printf("GOT PREAMBLE!\n");

      auto rq = context->ts->accept(TokenType::Symbol);
      preamble.push_back(rq->lexeme);
    } else if (context->ts->check(TokenType::Symbol)) {
      // Data section
      Token *data_sym = context->ts->accept(TokenType::Symbol);

      context->ts->expect(TokenType::Colon);

      CType *data_type = parse_type(context);

      data[data_sym->lexeme] = new AstNode;
      data[data_sym->lexeme]->ctype = *data_type;

    } else if (context->ts->check(TokenType::Function) || context->ts->check(TokenType::Pure)) {
      // Parse functions
      FuncStmt *func = (FuncStmt *)parse_function(context);
      functions[func->name] = func;
    } else {
      panic("Invalid entity");
    }

    eat_newlines(context);

    if (context->ts->accept(TokenType::EndOfFile)) {
      break;
    }

    //context->ts->expect(TokenType::Newline);
  }

  return make_actor(context->module, actor_name->lexeme, functions, data, inocaps, preamble, {});
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

HylicModule *parse(std::string abs_mod_path, TokenStream *stream) {

  ParseContext context;
  context.ts = stream;

  std::map<std::string, HylicModule*> imports;
  std::map<std::string, AstNode *> symbol_map;

  HylicModule *hm = new HylicModule;
  context.module = hm;

  while (stream->current != stream->tokens.end()) {
    if (context.ts->accept(TokenType::Import)) {
      // ModuleStmt
      std::string mod_name = context.ts->accept(TokenType::Symbol)->lexeme;
      while (context.ts->accept(TokenType::ModUse)) {
        mod_name += "►" + context.ts->accept(TokenType::Symbol)->lexeme;
      }
      HylicModule *imported_mod;
      if (is_system_module(mod_name)) {
        imported_mod = load_system_module(system_import_to_enum(mod_name));
      } else {
        panic("NYI");
      }

      eat_newlines(&context);
      hm->imports[mod_name] = imported_mod;
    } else if (context.ts->accept(TokenType::Actor)) {
      EntityDef *actor = (EntityDef *)parse_actor(&context);
      actor->abs_mod_path = abs_mod_path + "►" + actor->name;
      symbol_map[actor->name] = (AstNode *)actor;
      //print_ast(actor);
    } else if (context.ts->accept(TokenType::Record)) {
      parse_record(&context);
    } else if (context.ts->accept(TokenType::Newline)) {
      eat_newlines(&context);
    } else {
      printf("Failed on token %s\n", token_type_to_string((*context.ts->current)->type));
      panic("Invalid character in entity def");
    }
  }

  hm->entity_defs = symbol_map;

  return hm;
}
