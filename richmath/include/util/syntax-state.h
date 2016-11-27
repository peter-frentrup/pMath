#ifndef __UTIL__SYNTAX_STATE_H__
#define __UTIL__SYNTAX_STATE_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  typedef enum {
    Error,
    Global,
    LocalSymbol,
    Special,
    Parameter
  } SymbolKind;
  
  typedef enum {
    NoSpec,
    TableSpec,
    FunctionSpec,
    LocalSpec
  } LocalVariableForm;
  
  class GeneralSyntaxInfo: public Shareable {
    public:
      static SharedPtr<GeneralSyntaxInfo> std;
      
    public:
      GeneralSyntaxInfo();
      virtual ~GeneralSyntaxInfo();
      
    public:
      int glyph_style_colors[16];
  };
  
  class ScopePos: public Shareable {
    public:
      ScopePos(SharedPtr<ScopePos> super = 0);
      
      bool contains(SharedPtr<ScopePos> sub);
      
    private:
      SharedPtr<ScopePos> _super;
  };
  
  class SymbolInfo: public Shareable {
    public:
      SymbolInfo(
        SymbolKind            _kind = LocalSymbol,
        SharedPtr<ScopePos>   _pos = 0,
        SharedPtr<SymbolInfo> _next = 0);
      ~SymbolInfo();
      
      void add(SymbolKind _kind, SharedPtr<ScopePos> _pos);
      
    public:
      SymbolKind            kind;
      SharedPtr<ScopePos>   pos; // never nullptr!!!
      SharedPtr<SymbolInfo> next;
  };
  
  class SyntaxInformation {
    public:
      SyntaxInformation(Expr name);
      
    public:
      int minargs;
      int maxargs;
      
      LocalVariableForm locals_form;
      int locals_min;
      int locals_max;
  };
  
  class SyntaxState: public Base {
    public:
      SyntaxState();
      ~SyntaxState();
      
      void clear();
      
      SharedPtr<ScopePos> new_scope() {
        return current_pos = new ScopePos(current_pos);
      }
      
    public:
      bool in_pattern;
      bool in_function;
      
      SharedPtr<ScopePos>  current_pos;
      
      Hashtable <
      String,
      SharedPtr<SymbolInfo>,
      object_hash >   local_symbols;
  };
}

#endif // __UTIL__SYNTAX_STATE_H__
