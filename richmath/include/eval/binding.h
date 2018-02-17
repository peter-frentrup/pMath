#ifndef __EVAL__BINDING_H__
#define __EVAL__BINDING_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>


namespace richmath {
  class Document;
  
  enum class FESymbolIndex {
    NumberBox,
    SymbolInfo,
    AddConfigShaper,
    InternalExecuteFor,
    Menu,
    Item,
    Delimiter,
    KeyEvent,
    KeyAlt,
    KeyControl,
    KeyShift,
    SymbolDefinitions,
    FileOpenDialog,
    FileSaveDialog,
    ColorDialog,
    FontDialog,
    ControlActive,
    CopySpecial,
    AutoCompleteName,
    AutoCompleteFile,
    AutoCompleteOther,
    ScopedCommand,
    
    FrontEndSymbolsCount
  };
  
  bool init_bindings();
  void done_bindings();
  
  Expr GetSymbol(FESymbolIndex i);
  
  void set_current_document(Document *document);
  Document *get_current_document();
  
  // toplevel windows must register themselves!
  extern Hashtable<int, Void, cast_hash> all_document_ids;
}

#endif // __EVAL__BINDING_H__
