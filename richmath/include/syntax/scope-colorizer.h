#ifndef RICHMATH__SYNTAX__SCOPE_COLORIZER_H__INCLUDED
#define RICHMATH__SYNTAX__SCOPE_COLORIZER_H__INCLUDED

#include <syntax/syntax-state.h>
#include <graphics/glyphs.h>


namespace richmath {
  class MathSequence;
  class SpanExpr;
  
  class ScopeColorizer: public Base {
    public:
      explicit ScopeColorizer(MathSequence &sequence);
      
      void scope_colorize_spanexpr(SyntaxState &state, SpanExpr *se);
      
      void comments_colorize();
      
      void syntax_colorize_spanexpr(SpanExpr *se);
      
      void arglist_errors_colorize_spanexpr(SpanExpr *se, float error_indicator_height);
      
    private:
      MathSequence &sequence;
  };
}

#endif // RICHMATH__SYNTAX__SCOPE_COLORIZER_H__INCLUDED
