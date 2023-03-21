#include "hylic_compiler.h"
#include "hylic_ast.h"
#include <tuple>

// Instructions
namespace Hlcn {
const u32 Exit = 0x00;
const u32 Call = 0x06;
};

void cc_u64(CompileContext *cc, u64 value) {
  cc->comp_out.put(value << 56);
  cc->comp_out.put(value << 48);
  cc->comp_out.put(value << 40);
  cc->comp_out.put(value << 32);
  cc->comp_out.put(value << 24);
  cc->comp_out.put(value << 16);
  cc->comp_out.put(value << 8);
  cc->comp_out.put(value);
}

void cc_u32(CompileContext *cc, u32 value) {
  cc->comp_out.put(value << 24);
  cc->comp_out.put(value << 16);
  cc->comp_out.put(value << 8);
  cc->comp_out.put(value);
}

void cc_constant(CompileContext *cc, NumberNode* node) {
  cc->comp_out.put(0x08);
  cc->comp_out.put(node->value);
}

void cc_operator(CompileContext *cc, OperatorExpr* node) {
  compile_node(cc, node->term1);
  compile_node(cc, node->term2);

  switch(node->op) {
  case OperatorExpr::Op::Plus:
    cc->comp_out.put(0x02);
    break;
  case OperatorExpr::Op::Minus:
    cc->comp_out.put(0x03);
    break;
  case OperatorExpr::Op::Times:
    cc->comp_out.put(0x04);
    break;
  case OperatorExpr::Op::Divide:
    cc->comp_out.put(0x05);
    break;
  }
}

void cc_message(CompileContext *cc, MessageNode* node) {
  if (node->function_name == "Exit") {
    cc->comp_out.put(Hlcn::Exit);
    cc->comp_out.put(0x0);
    return;
  }

  if (node->comm_mode == CommMode::Sync) {
    cc->comp_out.put(Hlcn::Call);

    // thunk
    cc->function_thunks.push_back(std::make_tuple(node->function_name, cc->comp_out.tellp()));
    cc_u64(cc, 0x0);
    //cc->comp_out.put(cc->functions[node->function_name]);
    //printf("Calling %d\n", cc->functions[node->function_name]);
  } else if (node->comm_mode == CommMode::Async) {
  } else {
    panic("NYI");
  }
}

void cc_function(CompileContext *cc, FuncStmt* stmt) {
  for (auto &k: stmt->body) {
    compile_node(cc, k);
  }

  cc->comp_out.put(0x07);
}

void compile_node(CompileContext *cc, AstNode* in_node) {
  switch (in_node->type) {
  case AstNodeType::NumberNode: {
    auto node = safe_ncast<NumberNode *>(in_node, AstNodeType::NumberNode);
    cc_constant(cc, node);
  } break;
  case AstNodeType::OperatorExpr: {
    auto node = safe_ncast<OperatorExpr *>(in_node, AstNodeType::OperatorExpr);
    cc_operator(cc, node);
  } break;
  case AstNodeType::EntityDef: {
    printf("ENT!\n");
    auto ent_def_node = safe_ncast<EntityDef*>(in_node, AstNodeType::EntityDef);
    for (auto &[func_name, func_def] : ent_def_node->functions) {
      auto func_node = safe_ncast<FuncStmt *>(func_def, AstNodeType::FuncStmt);
      cc->functions[ent_def_node->name + "::" + func_node->name] = cc->comp_out.tellp();
      printf("%s\n", func_name.c_str());
      cc_function(cc, func_node);
    }
  } break;
  case AstNodeType::MessageNode: {
    auto node = safe_ncast<MessageNode *>(in_node, AstNodeType::MessageNode);
    cc_message(cc, node);
  } break;
  }
}

void _compile(std::vector<AstNode*> test_body) {
  CompileContext cc;
  cc.comp_out.open("../halcyonist/out.plmb", std::ios::binary | std::ios::out);

  for (auto &k: test_body) {
    compile_node(&cc, k);
  }

  for (auto &z : cc.function_thunks) {
    std::string func_name = std::get<0>(z);
    u64 thunk_pos = std::get<1>(z);

    cc.comp_out.seekp(thunk_pos);
    cc_u64(&cc, cc.functions[func_name]);
    printf("Fixed it with func pos %d\n", thunk_pos);
  }

  cc.comp_out.close();
}

void compile(std::map<std::string, AstNode *> program) {

  auto func1 = (FuncStmt*) make_function("tester", {}, {make_operator_expr(OperatorExpr::Op::Plus, make_message_node(nullptr, "blah", CommMode::Sync, {}), make_operator_expr(OperatorExpr::Op::Plus, make_number(4), make_number(6))),}, {}, false);

  auto func2 = (FuncStmt*) make_function("blah", {}, {make_operator_expr(OperatorExpr::Op::Plus, make_number(9), make_operator_expr(OperatorExpr::Op::Plus, make_number(4), make_number(6))),}, {}, false);

  std::map<std::string, FuncStmt *> funcs = {
      {"blah", func2},
      {"tester", func1},
  };

  auto ex_act = make_actor(nullptr, "TestEnt", funcs, {}, {}, {}, {});

  _compile({
      make_message_node(nullptr, "TestEnt::blah", CommMode::Sync, {}),
      ex_act,
  });
}
