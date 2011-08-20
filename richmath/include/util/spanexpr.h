#ifndef __UTIL__SPANEXPR_H__
#define __UTIL__SPANEXPR_H__

#include <util/pmath-extra.h>

#include <boxes/mathsequence.h>


namespace richmath {
  /* SpanExpr represents a span of Boxes (like Span class, but higher level).
     It can be seen like the pMath representation of boxes: A Span/SpanExpr is
     a list of other Spans or tokens (plain strings in pMath representation).
  
     SpanExpr is lazy loading and operates on a SpanArray. This means you build
     sub trees of the whole span tree of a MathSequence and then may expand()
     them to analyse the surrounding syntax.
  
     This class is used to easily analyse the syntax of a MathSequence for
     colorization.
  
     See SyntaxColorizer::colorize_scope() for the main use case.
  
  
  
     It could also be used for IntelliSense-like features. Imagine you have
       Plot(Sin(x), x->0..2Pi, Ax )
                                 ^--- and the cursor is here
     When you now press Ctrl+K, a popup with a list of all options of the Plot
     function that begin with "Ax" should appear.
     Therfore you have to notice that
      (a) the cursor is inside a Plot call expression
      (b) the cursor is at the third argument of this call expression
      (c) the third argument (and all following) normally contain option rules
  
     Point (c) can be done with the SyntaxInformation class and pMath
     SyntaxInformation() function.
  
     (a) and (b) can be done with SpanExpr:
      - Find the beginning of the current word (the Position of the "A") by
        iterating backwards from the cursor position until you find a token end
        (SpanArray::is_token_end()) The character after that token end is our
        token start.
  
      - Create a new SpanExpr(token_start, 0, sequence). This will contain our
        whole word ("Ax")
  
      - expand() the SpanExpr until you find some SpanExpr of the form
           {function_name, "(", arguments, ")"}
        or
           {arg1, "." function_name, "(", arguments, ")"}
        any spaces/comments are automatically ignored by this API.
  
      - If there is SyntaxInformation about function_name (in our case "Plot"),
        continue.
  
      - Calculate the argument number of our initial SpanExpr ( = 3 )
  
      Now you can evaluate "Options(Plot)" and filter the resulting list of
      rules to gat those option names that begin with "Ax"
  
   */
  class SpanExpr: public Base {
    public:
      SpanExpr(int start, Span span, MathSequence *sequence);
      ~SpanExpr();
      
      static SpanExpr *find(MathSequence *sequence, int pos, bool before = true);
      SpanExpr *expand(bool self_destruction = true);
      
      int start() { return _start; }
      int end() {   return _end; }
      int length() { return _end - _start + 1; }
      Span span() {  return _span; }
      int count() { return _items.length(); } // 0 if this is a simple token
      SpanExpr *parent() { return _parent; }
      MathSequence *sequence() { return _sequence; }
      
      SpanExpr *item(int i);
      int item_pos(int i) { return _items_pos[i]; }
      
      bool equals(const char *latin1);
      
      bool item_equals(int i, const char *latin1);
      
      uint16_t first_char() { return count() > 0 ? 0 : _sequence->text()[_start]; }
      
      uint16_t      as_char() { return length() != 1 ? 0 : first_char(); }
      String        as_text();
      Box          *as_box();
      
      pmath_token_t as_token(int *prec = 0);
      int as_prefix_prec(int defprec);
      int precedence(int *pos = 0); // -1,0,+1 = Prefix,Infix,Postfix
      
      bool is_box() {     return as_char() == PMATH_CHAR_BOX; }
      bool is_operand() { return _sequence->span_array().is_operand_start(_start); }
      
      uint16_t      item_first_char(int i);
      uint16_t      item_as_char(int i);
      String        item_as_text(int i);
      Box          *item_as_box(int i);
      bool          item_is_box(int i) { return item_as_char(i) == PMATH_CHAR_BOX; }
      bool          item_is_operand(int i) { return _sequence->span_array().is_operand_start(item_pos(i)); }
      
    private:
      SpanExpr(SpanExpr *parent, int start, Span span, MathSequence *sequence);
      void init(SpanExpr *parent, int start, Span span, MathSequence *sequence);
      
    private:
      SpanExpr *_parent;
      int       _start;
      int       _end;
      Span      _span;
      MathSequence *_sequence;
      
      Array<int>       _items_pos;
      Array<SpanExpr*> _items;
  };
  
  class FunctionCallSpan {
    public:
      FunctionCallSpan(SpanExpr *span): _span(span) {}
      
      bool is_sequence();       // a,b,c
      bool is_simple_call();    // f(a,b,c)
      bool is_complex_call();   // a.f(b,c)
      bool is_call() { return is_simple_call() || is_complex_call(); }
      
      bool is_simple_list();
      bool is_list();
      
      SpanExpr *sequence_argument(int i); // 1-based
      SpanExpr *function_head();
      SpanExpr *function_argument(int i); // 1-based
      int function_argument_count();
      
      SpanExpr *list_element(int i); // 1-based
      int list_length();
      
    private:
      SpanExpr *_span;
  };
}

#endif // __UTIL__SPANEXPR_H__
