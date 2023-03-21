
#include "hylic_ast.h"
#include "hylic_eval.h"
#include "gc.h"

void mark(AstNode* root) {
  root->marked = true;

  switch (root->type) {
    case AstNodeType::ListNode:{
      auto tmp_lst = safe_ncast<ListNode*>(root, AstNodeType::ListNode);
      for (auto &k : tmp_lst->list) {
        mark(k);
      }
    } break;
  }
}

void run_gc(Vat *vat) {

  // Mark
  for (auto &[ent_k, ent_v] : vat->entities) {
    ent_v->marked = true;
    for (auto &[_, v] : ent_v->data) {
      mark(v);
    }
  }

  // Sweep
  for (auto &k : vat->all_entities) {
    if (!k->marked) {
      destroy_entity(k);
    } else {
      k->marked = false;
    }
  }

  for (auto it = vat->all_objects.begin(); it != vat->all_objects.end();) {
    if (!(*it)->marked) {
      destroy_ast_obj(*it);
      it = vat->all_objects.erase(it);
    } else {
      (*it)->marked = false;
      it++;
    }
  }
}
