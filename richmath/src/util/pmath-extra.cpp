#include <util/pmath-extra.h>

#include <cstring>
#include <cmath>

using namespace richmath;
using namespace pmath;

static int digit_value_or_negative(uint16_t ch) {
  if(ch >= '0' && ch <= '9')
    return ch - '0';
  if(ch >= 'a' && ch <= 'f')
    return 10 + ch - 'a';
  if(ch >= 'A' && ch <= 'F')
    return 10 + ch - 'A';
  return -1;
}

// 0xFFFFFFFFU on error
uint32_t richmath::unicode_to_utf32(String s) {
  if(s[0] == 'x' && s.length() == 3) {
    int h1 = digit_value_or_negative(s[1]);
    int h2 = digit_value_or_negative(s[2]);
    
    if(h1 >= 0 && h2 >= 0)
      return ((uint32_t)h1 << 4) | (uint32_t)h2;
      
    return 0xFFFFFFFFU;
  }
  
  if(s[0] == 'u' && s.length() == 5 && digit_value_or_negative(s[1]) >= 0) {
    int h1 = digit_value_or_negative(s[1]);
    int h2 = digit_value_or_negative(s[2]);
    int h3 = digit_value_or_negative(s[3]);
    int h4 = digit_value_or_negative(s[4]);
    
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
      && s.length() > 2) {
    uint32_t result = 0;
    
    for(int i = 2; i < s.length(); ++i) {
      int h = digit_value_or_negative(s[i]);
      
      if(h < 0)
        return 0xFFFFFFFFU;
        
      result = (result << 4) | (uint16_t)h;
    }
    
    return result;
  }
  
  return 0xFFFFFFFFU;
}

//{ class Tokenizer ...

Expr Tokenizer::expand_string_boxes(String s) {
  const uint16_t *buf = s.buffer();
  const int len = s.length();
  
  Gather list;
  
  int i = 0;
  while(i < len) {
    int j = i;
    
    while(j < len) {
      if(buf[j] == PMATH_CHAR_LEFT_BOX) {
        if(i < j)
          list.emit(s.part(i, j - i));
          
        i = ++j;
        int k = 1;
        while(j < len) {
          if(buf[j] == PMATH_CHAR_LEFT_BOX) {
            ++k;
          }
          else if(buf[j] == PMATH_CHAR_RIGHT_BOX) {
            if(--k == 0) {
              ++j;
              break;
            }
          }
          ++j;
        }
        
        if(k == 0) {
          String code = s.part(i, j - i - 1);
          list.emit(Expr(pmath_parse_string(code.release())));
        }
        
        i = j;
        break;
      }
      ++j;
    }
    
    if(i < j)
      list.emit(s.part(i, j - i));
      
    i = j;
  }
  
  Expr result = list.end();
  if(result.expr_length() == 1)
    return result[1];
  return result;
}

bool Tokenizer::is_left_bracket(String tok) {
  switch(analyse(tok)) {
    case PMATH_TOK_LEFT:
    case PMATH_TOK_LEFTCALL:
      return true;
  }

  return tok.length() == 1 && tok[0] == PMATH_CHAR_PIECEWISE;
}

bool Tokenizer::is_right_bracket(String tok) {
  return analyse(tok) == PMATH_TOK_RIGHT;
}

bool Tokenizer::is_bracket(uint16_t ch) {
  if(ch == PMATH_CHAR_PIECEWISE)
    return true;
  
  switch(pmath_token_analyse(&ch, 1, nullptr)) {
    case PMATH_TOK_LEFT:
    case PMATH_TOK_LEFTCALL:
    case PMATH_TOK_RIGHT:
      return true;
  }

  return false;
}

bool Tokenizer::is_bracket(String tok) {
  switch(analyse(tok)) {
    case PMATH_TOK_LEFT:
    case PMATH_TOK_LEFTCALL:
    case PMATH_TOK_RIGHT:
      return true;
  }

  return tok.length() == 1 && tok[0] == PMATH_CHAR_PIECEWISE;
}

//} ... class Tokenizer

//{ class SpanArray ...

SpanArray::SpanArray(pmath_span_array_t *spans)
  : Base(),
    _array(spans)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

SpanArray::~SpanArray() {
  pmath_span_array_free(_array);
}

pmath_span_array_t *SpanArray::extract_array() {
  pmath_span_array_t *tmp = _array;
  _array = nullptr;
  return tmp;
}

SpanArray &SpanArray::operator=(pmath_span_array_t *spans) {
  if(_array == spans)
    return *this;
    
  pmath_span_array_free(_array);
  _array = spans;
  return *this;
}

//} ... class SpanArray
