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
  
  namespace impl {
    bool init_document_functions();
    void done_document_functions();
  }
  
  void set_current_document(Document *document);
  Document *get_current_document();
}

#endif // RICHMATH__EVAL__BINDING_H__INCLUDED
