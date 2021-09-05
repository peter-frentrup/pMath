#ifndef RICHMATH__UTIL__PMATH_EXTRA_H__INCLUDED
#define RICHMATH__UTIL__PMATH_EXTRA_H__INCLUDED

#include <pmath-cpp.h>

#include <util/array.h>
#include <util/base.h>


extern pmath_symbol_t richmath_System_Ceiling;
extern pmath_symbol_t richmath_System_Floor;
extern pmath_symbol_t richmath_System_Plus;
extern pmath_symbol_t richmath_System_Power;
extern pmath_symbol_t richmath_System_Round;
extern pmath_symbol_t richmath_System_Rule;
extern pmath_symbol_t richmath_System_RuleDelayed;
extern pmath_symbol_t richmath_System_Times;

namespace richmath {
  using namespace pmath; // bad style!!!
  
  inline bool is_utf16_high(uint16_t c) { return (c & 0xFC00) == 0xD800; }
  inline bool is_utf16_low(uint16_t c) {  return (c & 0xFC00) == 0xDC00; }
  
  // 0xFFFFFFFFU on error
  uint32_t unicode_to_utf32(String s);
  
  Expr expand_string_boxes(String s);
  
  inline ArrayView<const uint16_t> buffer_view(String s) { return ArrayView<const uint16_t>(s.length(), s.buffer()); }
  
  class Span {
    public:
      Span(pmath_span_t *data): _data(data) {}
      operator bool() { return _data != nullptr; }
      
      Span next() { return Span(pmath_span_next(_data)); }
      int  end() {  return pmath_span_end(_data); }
      
      bool operator==(const Span &other) { return _data == other._data; }
      bool operator!=(const Span &other) { return _data != other._data; }
    private:
      pmath_span_t *_data;
  };
  
  class SpanArray: public Base {
    public:
      SpanArray(pmath_span_array_t *spans = nullptr);
      ~SpanArray();
      
      pmath_span_array_t *extract_array();
      pmath_span_array_t *array() { return _array; }
      
      SpanArray &operator=(pmath_span_array_t *spans);
      
      int length() const {
        return pmath_span_array_length(_array);
      }
      
      Span operator[](int i) const {
        return Span(pmath_span_at(_array, i));
      }
      
      int next_token(int i) const {
        int len  = length();
        while(i < len && !is_token_end(i))
          ++i;
        return i + 1;
      }
      
      bool is_token_end(int i) const {
        return pmath_span_array_is_token_end(_array, i);
      }
      
      bool is_operand_start(int i) const {
        return pmath_span_array_is_operand_start(_array, i);
      }
      
    private:
      pmath_span_array_t *_array;
  };

  inline Expr Rule(       Expr l, Expr r) { return Expr(pmath_expr_new_extended(pmath_ref(richmath_System_Rule),        2, l.release(), r.release())); }
  inline Expr RuleDelayed(Expr l, Expr r) { return Expr(pmath_expr_new_extended(pmath_ref(richmath_System_RuleDelayed), 2, l.release(), r.release())); }
  
  inline Expr Power(Expr x, Expr y) { return Call(Symbol(richmath_System_Power), PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(y)); }

  inline Expr Sqrt(Expr x) { return Power(PMATH_CPP_MOVE(x), Rational(1, 2)); }
  inline Expr Inv(Expr x) {  return Power(PMATH_CPP_MOVE(x), -1); }
  
  inline Expr Times(Expr x1, Expr x2) {                   return Call(Symbol(richmath_System_Times), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2)); }
  inline Expr Times(Expr x1, Expr x2, Expr x3) {          return Call(Symbol(richmath_System_Times), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3)); }
  inline Expr Times(Expr x1, Expr x2, Expr x3, Expr x4) { return Call(Symbol(richmath_System_Times), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3), PMATH_CPP_MOVE(x4)); }

  inline Expr Divide(Expr x, Expr y) { return Times(PMATH_CPP_MOVE(x), Inv(PMATH_CPP_MOVE(y))); }

  inline Expr Plus(Expr x1, Expr x2) {                   return Call(Symbol(richmath_System_Plus), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2)); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3) {          return Call(Symbol(richmath_System_Plus), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3)); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3, Expr x4) { return Call(Symbol(richmath_System_Plus), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3), PMATH_CPP_MOVE(x4)); }

  inline Expr Minus(Expr x) {         return Times(-1, PMATH_CPP_MOVE(x)); }
  inline Expr Minus(Expr x, Expr y) { return Plus(PMATH_CPP_MOVE(x), Minus(PMATH_CPP_MOVE(y))); }
  
  inline Expr Ceiling(Expr x) {         return Call(Symbol(richmath_System_Ceiling), PMATH_CPP_MOVE(x)); }
  inline Expr Ceiling(Expr x, Expr a) { return Call(Symbol(richmath_System_Ceiling), PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  inline Expr Floor(  Expr x) {         return Call(Symbol(richmath_System_Floor),   PMATH_CPP_MOVE(x)); }
  inline Expr Floor(  Expr x, Expr a) { return Call(Symbol(richmath_System_Floor),   PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  inline Expr Round(  Expr x) {         return Call(Symbol(richmath_System_Round),   PMATH_CPP_MOVE(x)); }
  inline Expr Round(  Expr x, Expr a) { return Call(Symbol(richmath_System_Round),   PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  
}

#endif // RICHMATH__UTIL__PMATH_EXTRA_H__INCLUDED
