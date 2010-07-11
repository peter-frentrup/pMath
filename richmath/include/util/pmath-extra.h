#ifndef __UTIL__PMATH_EXTRA_H__
#define __UTIL__PMATH_EXTRA_H__

#include <pmath-cpp.h>

#include <util/base.h>

namespace richmath{
  using namespace pmath; // bad style!!!
  
  inline bool is_utf16_high(uint16_t c){
    return (c & 0xFC00) == 0xD800;
  }
  
  inline bool is_utf16_low(uint16_t c){
    return (c & 0xFC00) == 0xDC00;
  }
  
  // 0xFFFFFFFFU on error
  uint32_t unicode_to_utf32(String s);
  
  class Span{
    public:
      Span(pmath_span_t *data): _data(data){}
      operator bool(){ return _data != 0; }
      
      Span next(){ return Span(pmath_span_next(_data)); }
      int  end(){  return pmath_span_end(_data); }
      
      bool operator==(const Span &other){ return _data == other._data; }
      bool operator!=(const Span &other){ return _data != other._data; }
    private:
      pmath_span_t *_data;
  };
  
  class SpanArray: public Base{
    public:
      SpanArray(pmath_span_array_t *spans = 0);
      ~SpanArray();
      
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
        return pmath_span_array_is_operator_start(_array, i);
      }
      
      pmath_span_array_t *array(){ return _array; }
      
    private:
      pmath_span_array_t *_array;
  };
}

#endif // __UTIL__PMATH_EXTRA_H__
