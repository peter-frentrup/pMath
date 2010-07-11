#include <util/pmath-extra.h>

#include <cstring>
#include <cmath>

using namespace richmath;
using namespace pmath;

static int hex(uint16_t ch){
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'f')
    return 10 + ch - 'a';
  if(ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  return -1;
}

// 0xFFFFFFFFU on error
uint32_t richmath::unicode_to_utf32(String s){
  if(s[0] == 'x' && s.length() == 3){
    int h1 = hex(s[1]);
    int h2 = hex(s[2]);
    
    if(h1 >= 0 && h2 >= 0)
      return ((uint32_t)h1 << 4) | (uint32_t)h2;
    
    return 0xFFFFFFFFU;
  }
  
  if(s[0] == 'u' && s.length() == 5 && hex(s[1]) >= 0){
    int h1 = hex(s[1]);
    int h2 = hex(s[2]);
    int h3 = hex(s[3]);
    int h4 = hex(s[4]);
    
    if(h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0)
      return ((uint32_t)h1 << 12) | 
             ((uint32_t)h2 << 8) | 
             ((uint32_t)h3 << 4) | 
              (uint32_t)h4;
    
    return 0xFFFFFFFFU;
  }
  
  if((s[0] == 'U' || s[0] == 'u') 
  && s[1] == '+' 
  && s.length() <= 10
  && s.length() > 2){
    uint32_t result = 0;
    
    for(int i = 2;i < s.length();++i){
      int h = hex(s[i]);
      
      if(h < 0)
        return 0xFFFFFFFFU;
      
      result = (result << 4) |  (uint16_t)h;
    }
    
    return result;
  }
  
  return 0xFFFFFFFFU;
}

//{ class SpanArray ...

SpanArray::SpanArray(pmath_span_array_t *spans)
: _array(spans)
{
}

SpanArray::~SpanArray(){
  pmath_span_array_free(_array);
}
      
SpanArray &SpanArray::operator=(pmath_span_array_t *spans){
  if(_array == spans)
    return *this;
    
  pmath_span_array_free(_array);
  _array = spans;
  return *this;
}

//} ... class SpanArray
