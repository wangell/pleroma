#include "io.h"
#include "ffi.h"
#include "../type_util.h"
#include <fstream>
#include <iostream>

#include <iostream>
#include <string>
#include <vector>

const u64 chunk_size = 4096;

// Filename -> node ID -> chunk names
std::map<std::string, std::map<int, std::vector<std::string>>> chunk_map;

// ZenoMaster
AstNode *zeno_create(EvalContext *context, std::vector<AstNode *> args) {

  chunk_map["example.dat"][0].push_back("example-0.dat");

  return make_number(0);
}

AstNode *zeno_new(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *zeno_checkout(EvalContext *context, std::vector<AstNode *> args) {

  std::string file_id = extract_string(args[0]);
  printf("HERE!\n");

  return make_string(chunk_map[file_id][0][0]);
}

// ZenoNode

// Zfile

std::string read_local_file(std::string chunk_name) {
  std::ifstream rf(chunk_name, std::ios::in | std::ios::binary);

  std::string f;

  for (int i = 0; i < 4096; i++) {
    char z;
    rf.read(&z, 1);
    f += z;
  }

  return f;
}

void write_file() {
  for (int i = 0; i < 5; ++i) {
    std::ofstream wf("example-" + std::to_string(i) + ".dat", std::ios::out | std::ios::binary);
    if (!wf) {
      assert(false);
    }

    for (int n = 0; n < 4096; ++n) {
      char *blah = "abcdefgh";

      wf.write(blah + i, 1);
    }
    wf.close();
    printf("DONE\n");
  }
}

AstNode *zfile_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *zfile_get_chunks(EvalContext *context, std::vector<AstNode *> args) {
  // return make_message_node(make_symbol("zm"), std::string function_name, CommMode comm_mode, std::vector<AstNode *> args)
  return make_number(0);
}

AstNode *zfile_assemble_chunks(EvalContext *context, std::vector<AstNode *> args) {
  auto chunk_list = (ListNode *)args[0];

  std::string lol;
  for (int i = 0; i < chunk_list->list.size(); ++i) {
    assert(chunk_list->list[i]->type == AstNodeType::StringNode);
    std::string chunk_name = ((StringNode *)chunk_list->list[i])->value;
    lol += read_local_file(chunk_name);
  }

  return make_string(lol);
}

AstNode *zfile_test(EvalContext *context, std::vector<AstNode *> args) {

  auto m1 = eval_message_node(context, cfs(context).entity->data["zm"], CommMode::Async, "checkout", {args[0]});

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;

  auto slist = make_list({make_string("example-0.dat"), make_string("example-1.dat")}, str_type);
  auto m2 = eval_message_node(context, make_entity_ref(-1, -1, -1), CommMode::Sync, "assemble-chunks", {slist});
  printf("HERE!\n");

  return m2;
}

std::map<std::string, AstNode*> load_zeno() {
  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;

  CType *chunk_list_type = new CType;
  chunk_list_type->basetype = PType::List;
  chunk_list_type->dtype = DType::Local;
  chunk_list_type->subtype = new CType;
  chunk_list_type->subtype->basetype = PType::str;
  chunk_list_type->subtype->dtype = DType::Local;

  std::map<std::string, FuncStmt *> zmaster_functions = {
      {"create", setup_direct_call(zeno_create, "create", {}, {}, none_type)},
      {"new", setup_direct_call(zeno_new, "new", {}, {}, none_type)},
      {"checkout", setup_direct_call(zeno_checkout, "checkout", {"filename"}, {str_type}, *chunk_list_type)},
  };

  std::map<std::string, FuncStmt *> zfile_functions = {
      {"create", setup_direct_call(zfile_create, "create", {}, {}, none_type)},
      {"test", setup_direct_call(zfile_test, "test", {"filename"}, {str_type}, *str_type)},
      {"assemble-chunks", setup_direct_call(zfile_assemble_chunks, "assemble-chunks", {"chunks"}, {chunk_list_type}, *str_type)},
  };

  return {
    {"ZenoMaster", make_actor(nullptr, "ZenoMaster", zmaster_functions, {}, {}, {}, {})},
    {"Zfile", make_actor(nullptr, "Zfile", zfile_functions, {}, {}, {}, {})}
  };
}
