#ifndef RICHMATH__EVAL__BINDING_H__INCLUDED
#define RICHMATH__EVAL__BINDING_H__INCLUDED

#include <util/frontendobject.h>
#include <util/hashtable.h>
#include <util/pmath-extra.h>

extern pmath_symbol_t richmath_FE_KeyEvent;
extern pmath_symbol_t richmath_FE_KeyAlt;
extern pmath_symbol_t richmath_FE_KeyControl;
extern pmath_symbol_t richmath_FE_KeyShift;
extern pmath_symbol_t richmath_FE_ScopedCommand;

namespace richmath {
  class Document;
  
  bool init_bindings();
  void done_bindings();
}

#endif // RICHMATH__EVAL__BINDING_H__INCLUDED
