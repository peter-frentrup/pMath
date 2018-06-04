#ifndef __EVAL__BINDING_H__
#define __EVAL__BINDING_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>

extern pmath_symbol_t richmath_FE_Menu;
extern pmath_symbol_t richmath_FE_Item;
extern pmath_symbol_t richmath_FE_Delimiter;

extern pmath_symbol_t richmath_FE_KeyEvent;
extern pmath_symbol_t richmath_FE_KeyAlt;
extern pmath_symbol_t richmath_FE_KeyControl;
extern pmath_symbol_t richmath_FE_KeyShift;
extern pmath_symbol_t richmath_FE_ScopedCommand;

namespace richmath {
  class Document;
  
  bool init_bindings();
  void done_bindings();
  
  void set_current_document(Document *document);
  Document *get_current_document();
  
  // toplevel windows must register themselves!
  extern Hashtable<int, Void, cast_hash> all_document_ids;
}

#endif // __EVAL__BINDING_H__
