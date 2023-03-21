#include "type_util.h"
#include "hylic_ast.h"

CType *lstr() {
  static CType *_lstr;

  if (!_lstr) {
    _lstr = new CType;
    _lstr->basetype = PType::str;
    _lstr->dtype = DType::Local;
  }
  return _lstr;
}

CType *lu8() {
  static CType *_lu8;

  if (!_lu8) {
    _lu8 = new CType;
    _lu8->basetype = PType::u8;
    _lu8->dtype = DType::Local;
  }

  return _lu8;
}

CType *void_t() {
  static CType *_void_t;

  if (!_void_t) {
    _void_t = new CType;
    _void_t->basetype = PType::None;
    _void_t->dtype = DType::Local;
  }

  return _void_t;
}

CType far_ent(std::string ent_name) {
  CType t;
  t.basetype = PType::Entity;
  t.entity_name = ent_name;
  return t;
}
