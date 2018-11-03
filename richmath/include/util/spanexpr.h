#ifndef RICHMATH__UTIL__SPANEXPR_H__INCLUDED
#define RICHMATH__UTIL__SPANEXPR_H__INCLUDED

#include <util/pmath-extra.h>

#include <boxes/mathsequence.h>


namespace richmath {
  /** SpanExpr represents a span of Boxes (like Span class, but higher level).
     It can be seen like the pMath representation of boxes: A Span/SpanExpr is
     a list of other Spans or tokens (plain strings in pMath representation).
  
     SpanExpr is lazy loading and operates on a SpanArray. This means you build
     sub trees of the whole span tree of a MathSequence and then may expand()
     them to analyse the surrounding syntax.
  
     This class is used to easily analyse the syntax of a MathSequence for
     colorization.
  
     See ScopeColorizer::colorize_scope() for the main use case.
  
  
  
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
      /** Create an empty span.
          \param position The text position inside \a sequence.
          \param sequence The sequence box containing this span.
          
          You should use find() instead to create a span for a token.
          
          \see find()
       */
      SpanExpr(int position, MathSequence *sequence); // creates an empty span
      
      /** Create a span expression that wraps a pMath span
          \param start The start index of the \a span within \a sequence.
          \param span A pMath span.
          \param sequence The sequence containing the \a span.
       */
      SpanExpr(int start, Span span, MathSequence *sequence);
      
      ~SpanExpr();
      
      /** Allocate a span near some text position.
          \param sequence The sequence box containing the token.
          \param pos A text index.
          \param before Whether to get the span before \a pos or after.
          
          \see expand()
       */
      static SpanExpr *find(MathSequence *sequence, int pos, bool before = true);
      
      /** Allocate the span contaning this span
          \param self_destruction Whether the current sequence() can be left.
          
          This function generally transfers ownership to the returned span. 
          If you choose self_destruction=true (the default), you must not access this
          after the function returns (it may have been deleted).
          
          If you choose self_destruction=false, and the function returns nullptr, you still 
          own this span and have to delete it later.
          If you choose self_destruction=false, and the function returns a valid span 
          (not nullptr), you can still access this afterwards, but you need not delete it.
          (if you delete this afterwards, it will be detached from its parent span, which
          would later recreate a span at the same position on demand).
       */
      SpanExpr *expand(bool self_destruction = true);
      
      /** Get the start index of this span within sequence().
       */
      int start() { return _start; }
      
      /** Get the end index of this span (index of last character included in the span).
       */
      int end() { return _end; }
      
      /** Get the number of characters (UTF-16 code units) of the span.
       */
      int length() { return _end - _start + 1; }
      
      /** Get the underlying pMath span.
       */
      Span span() { return _span; }
      
      /** Get the number of sub-spans and tokens.
      
          If this is a simple token, the function returns 0.
       */
      int count() { return _items.length(); } // 0 if this is a simple token
      
      /** Try to get the span containing this span.
          
          May return nullptr if the parent span was not yet parsed.
          \see expand()
       */
      SpanExpr *parent() {   return _parent; }
      
      /** Get the sequence box containging this span.
       */
      MathSequence *sequence() { return _sequence; }
      
      /** Get a sub-span or token by index.
          \param i An index between 0 and count()-1
       */
      SpanExpr *item(int i);
      
      /** Get the start index of a sub-span or token.
          \param i An index between 0 and count()-1
       */
      int item_pos(int i) { return _items_pos[i]; }
      
      /** Test whether the this is a particular token.
          \param latin1 The token contend.
       */
      bool equals(const char *latin1);
      
      /** Test if a sub-span is a particular token.
       */
      bool item_equals(int i, const char *latin1);
      
      /** Get the first character of the span.
       */
      uint16_t first_char();
      
      /** Get the character of the span if it is a single character token, otherwise 0
       */
      uint16_t as_char() { return length() != 1 ? 0 : first_char(); }
      
      /** Get the span text content.
       */
      String as_text();
      
      /** Get the box represented by the span.
          
          \see is_box()
       */
      Box *as_box();
      
      /** Get the token represented by the span.
          \param prec An optional output parameter for the (infix) precedence.
          
          This looks into boxes like UnderoverscriptBox or SubsuperscriptBox.
       */
      pmath_token_t as_token(int *prec = nullptr);
      
      /** Get the prefix precedence of the token span.
          \param defprec The precedence as returned via as_token().
       */
      int as_prefix_prec(int defprec);
      
      /** Test whether this span is prefix, infix or postfix operator.
          \return -1 for prefix, 0 for infix, +1 for postfix.
       */
      int precedence(int *pos = nullptr); // -1,0,+1 = Prefix,Infix,Postfix
      
      /** Test whether this span represents a box.
         
          \see as_box()
       */
      bool is_box() { return as_char() == PMATH_CHAR_BOX; }
      
      /** Test whether this span represents an operand rather than an operator.
         
          \see Span::is_operand_start()
       */
      bool is_operand() { return _start <= _end && _sequence->span_array().is_operand_start(_start); }
      
      /** Get the first character of a sub-span or token.
          \param i A sub-span/token index between 0 and count()-1
       */
      uint16_t item_first_char(int i);
      
      /** Get the character of a single-character sub-span/token.
          \param i A sub-span/token index between 0 and count()-1
       */
      uint16_t item_as_char(int i);
      
      /** Get the content of a sub-span or token.
          \param i A sub-span/token index between 0 and count()-1
       */
      String item_as_text(int i);
      
      /** Get the box represented by a sub-span or token.
          \param i A sub-span/token index between 0 and count()-1
          
          \see item_is_box()
       */
      Box *item_as_box(int i);
      
      /** Test whether a sub-span or token is a box.
          \param i A sub-span/token index between 0 and count()-1
       */
      bool item_is_box(int i) { return item_as_char(i) == PMATH_CHAR_BOX; }
      
      /** Test whether a sub-span or token is an operand rather than an operator.
          \param i A sub-span/token index between 0 and count()-1
       */
      bool item_is_operand(int i) { return _sequence->span_array().is_operand_start(item_pos(i)); }
      
    private:
      SpanExpr(SpanExpr *parent, int start, Span span, MathSequence *sequence);
      void init(SpanExpr *parent, int start, Span span, MathSequence *sequence);
      
      Span item_span(int i);
      
    private:
      SpanExpr *_parent;
      int       _start;
      int       _end;
      Span      _span;
      MathSequence *_sequence;
      
      Array<int>       _items_pos;
      Array<SpanExpr*> _items;
  };
  
  bool is_comment_start_at(const uint16_t *str, const uint16_t *buf_end);
  SpanExpr *span_as_name(SpanExpr *span);
  
  class SequenceSpan { // a,b,c
    public:
      explicit SequenceSpan(SpanExpr *span, bool take_ownership);
      ~SequenceSpan();
      
      void set(SpanExpr *span, bool take_ownership);
      SequenceSpan &operator=(const SequenceSpan &other);
      
      bool is_sequence(){ return _is_sequence; }
      int  length(){      return _items.length(); }
      
      SpanExpr *item(int i); // 1-based; may return empty SpanExpr!
      SpanExpr *all(){ return _span; }
      
    protected:
      void init(SpanExpr *span);
      void reset();
      
    protected:
      SpanExpr         *_span;
      Array<SpanExpr*>  _items;
      bool              _is_sequence;
      bool              _has_ownership;
  };
  
  class FunctionCallSpan {
    public:
      FunctionCallSpan(SpanExpr *span);
      
      static bool is_simple_call( SpanExpr *span); // f(a,b,c)
      static bool is_complex_call(SpanExpr *span); // a.f(b,c)
      static bool is_call(        SpanExpr *span) { return is_simple_call(span) || is_complex_call(span); }
      static bool is_list(        SpanExpr *span);
      static bool is_sequence(    SpanExpr *span);
      
      bool is_simple_call(){  return is_simple_call(_span); }
      bool is_complex_call(){ return is_complex_call(_span); }
      bool is_call(){         return is_call(_span); }
      bool is_list() {        return is_list(_span); }
      
      SpanExpr *span() { return _span; }
      
      SpanExpr *arguments_span(){ return _args.all(); }
      
      /** Get the function head.
         
          \see is_call()
       */
      SpanExpr *function_head();
      
      /** Get a function argument, between 1 and function_argument_count(), inclusive.
      
          \see is_call()
       */
      SpanExpr *function_argument(int i); // 1-based
      
      /** Get the number of function arguments
      
          \see is_call()
       */
      int function_argument_count();
      
      /** Get a list item, between 1 and list_length(), inclusive.
      
          \see is_list()
       */
      SpanExpr *list_element(int i); // 1-based
      
      /** Get the number of list items.
      
          \see is_list()
       */
      int list_length();
    
    private:
      void init_args();
    
    private:
      SpanExpr     *_span;
      SequenceSpan  _args;
  };
  
  /* Blocks are syntactic as implemented by ExperimentalSyntax`
     Examples are "If(...) {...} Else If(...) {...}" or "Block {...}"
     
     They are characterized by being a concatenation of at least two operands 
     (parses as multiplication, but should not have multiplication sign)
     such that at least one of the operands is a list and the others are symbol 
     names or simple calls. 
     The first may not be a list and two lists may not follow immediately after 
     each other.
   */
  class BlockSpan {
    public:
      static bool maybe_block(SpanExpr *span);
  };
}

#endif // RICHMATH__UTIL__SPANEXPR_H__INCLUDED
