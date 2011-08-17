#ifndef __GRAPHICS__SCOPE_COLORIZER_H__
#define __GRAPHICS__SCOPE_COLORIZER_H__

#include <util/syntax-state.h>


namespace richmath {
  class MathSequence;
  class SpanExpr;
  
  class ScopeColorizer: public Base {
    public:
      explicit ScopeColorizer(MathSequence *_sequence);
      
      int symbol_colorize(
        SyntaxState *state,
        int          start,
        SymbolKind   kind);
        
      void symdef_colorize_spanexpr(
        SyntaxState *state,
        SpanExpr    *se,    // "x"  "x:=y"
        SymbolKind   kind);
        
      void symdeflist_colorize_spanexpr(
        SyntaxState *state,
        SpanExpr    *se,    // "{symdefs ...}"
        SymbolKind   kind);
        
      void replacement_colorize_spanexpr(
        SyntaxState *state,
        SpanExpr    *se,    // "x->value"
        SymbolKind   kind);
      
      void scope_colorize_spanexpr(SyntaxState *state, SpanExpr *se);
    
    private:
      MathSequence *sequence;
  };
}

#endif // __GRAPHICS__SCOPE_COLORIZER_H__
