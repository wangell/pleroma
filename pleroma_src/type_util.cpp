#include "type_util.h"

CType lstr() {
  CType t;
  t.basetype = PType::str;
  t.dtype = DType::Local;
  return t;
}

CType lu8() {
  CType t;
  t.basetype = PType::u8;
  t.dtype = DType::Local;
  return t;
}
