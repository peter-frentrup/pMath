#ifndef __GRAPHICS__SCOPE_COLORIZER_H__
#define __GRAPHICS__SCOPE_COLORIZER_H__

#include <util/syntax-state.h>


namespace richmath {
  class MathSequence;
  class SpanExpr;
  
  class ScopeColorizer: public Base {
    public:
      explicit ScopeColorizer(MathSequence *_sequence);
      
      void scope_colorize_spanexpr(SyntaxState *state, SpanExpr *se);
      
      
      
      void comments_colorize_span(Span span, int *pos);
      
      void syntax_colorize_spanexpr(SpanExpr *se);
      
      void arglist_errors_colorize_spanexpr(          SpanExpr *se, float error_indicator_height);
      void arglist_errors_colorize_spanexpr_norecurse(SpanExpr *se, float error_indicator_height);
      
      void unknown_option_colorize_spanexpr(SpanExpr *se, Expr options);
      
    private:
      MathSequence *sequence;
  };
}

#endif // __GRAPHICS__SCOPE_COLORIZER_H__
