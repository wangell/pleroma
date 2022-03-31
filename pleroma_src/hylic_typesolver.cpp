#include "hylic_typesolver.h"
#include "hylic_ast.h"
#include <cassert>

bool typesolve_sub(AstNode* node) {

  switch(node->type) {
  case AstNodeType::EntityDef:
    {
      bool all_valid = true;
      auto ent_node = (EntityDef*)node;
      for (auto &[k, v] : ent_node->functions) {
        all_valid = all_valid && typesolve_sub(v);
      }
    }
    break;
  }

  // We should never reach here
  assert(false);
}

void typesolve(std::map<std::string, AstNode *> program) {
  return;

  bool all_valid = true;
  for (auto &[k, v] : program) {
    all_valid = all_valid && typesolve_sub(v);
  }

  assert (all_valid);
}
