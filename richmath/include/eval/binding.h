#ifndef __EVAL__BINDING_H__
#define __EVAL__BINDING_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>


namespace richmath {
  class Document;
  
  typedef enum {
    NumberBoxSymbol,
    SymbolInfoSymbol,
    AddConfigShaperSymbol,
    InternalExecuteForSymbol,
    MenuSymbol,
    ItemSymbol,
    DelimiterSymbol,
    KeyEventSymbol,
    KeyAltSymbol,
    KeyControlSymbol,
    KeyShiftSymbol,
    SymbolDefinitionsSymbol,
    
    FrontEndSymbolsCount
  } FrontEndSymbolIndex;
  
  bool init_bindings();
  void done_bindings();
  
  // caller has to reference the result.
  Expr GetSymbol(FrontEndSymbolIndex i);
  
  void set_current_document(Document *document);
  Document *get_current_document();
  
  // toplevel windows must register themselves!
  extern Hashtable<int, Void, cast_hash> all_document_ids;
}

#endif // __EVAL__BINDING_H__
