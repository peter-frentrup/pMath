#ifndef RICHMATH__SYNTAX__SYNTAX_STATE_H__INCLUDED
#define RICHMATH__SYNTAX__SYNTAX_STATE_H__INCLUDED

#include <graphics/color.h>

#include <util/hashtable.h>
#include <util/sharedptr.h>


namespace richmath {
  enum class SymbolKind: int8_t {
    Error,
    Global,
    LocalSymbol,
    Special,
    Parameter
  };
  
  enum class LocalVariableForm: int8_t {
    NoSpec,
    TableSpec,
    FunctionSpec,
    LocalSpec
  };
  
  class ScopePos: public Shareable {
    public:
      ScopePos(SharedPtr<ScopePos> super = nullptr);
      
      bool contains(SharedPtr<ScopePos> sub);
      
    private:
      SharedPtr<ScopePos> _super;
  };
  
  class SymbolInfo: public Shareable {
    public:
      SymbolInfo(
        SymbolKind            _kind = SymbolKind::LocalSymbol,
        SharedPtr<ScopePos>   _pos = nullptr,
        SharedPtr<SymbolInfo> _next = nullptr);
      ~SymbolInfo();
      
      void add(SymbolKind _kind, SharedPtr<ScopePos> _pos);
      
    public:
      SharedPtr<ScopePos>   pos; // never nullptr!!!
      SharedPtr<SymbolInfo> next;
      SymbolKind            kind;
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
      bool is_keyword;
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
      
      Hashtable< String, SharedPtr<SymbolInfo> > local_symbols;
  };
}

#endif // RICHMATH__SYNTAX__SYNTAX_STATE_H__INCLUDED
