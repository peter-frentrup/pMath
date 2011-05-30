#include <boxes/numberbox.h>

#include <cfloat>
#include <cmath>

#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <eval/binding.h>
#include <graphics/context.h>

using namespace richmath;

//{ class NumberBox ...

NumberBox::NumberBox(String number)
: OwnerBox()
{
  set_number(number);
}

bool NumberBox::edit_selection(Context *context){
  if(Box::edit_selection(context)){
    MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
    
    if(!seq)
      return false;
    
    Box *selbox = context->selection.get();
    if(_exponent && selbox == _exponent){
      int s = context->selection.start + _index + _expstart;
      int e = context->selection.end   + _index + _expstart;
      context->selection.set(seq, s, e);
      
      if(_number[_number.length() - 1] == '`'){
        if(pmath_char_is_digit(seq->text()[_index + 1])
        ||                     seq->text()[_index + 1] == '-'
        ||                     seq->text()[_index + 1] == '+'){
          seq->insert(_index + 1, " ");
        }
      }
      seq->insert(_index + 1, _number);
      seq->remove(_index, _index + 1); // deletes this
      return true;
    }
    
    if(_base && selbox == _base){
      int s = context->selection.start + _index;
      int e = context->selection.end   + _index;
      context->selection.set(seq, s, e);
      
      if(_number[_number.length() - 1] == '`'){
        if(pmath_char_is_digit(seq->text()[_index + 1])
        ||                     seq->text()[_index + 1] == '-'
        ||                     seq->text()[_index + 1] == '+'){
          seq->insert(_index + 1, " ");
        }
      }
      seq->insert(_index + 1, _number);
      seq->remove(_index, _index + 1); // deletes this
      return true;
    }
    
    if(selbox == _content){
      int s = context->selection.start;
      int e = context->selection.end;
      if(s == _content->length())
        s = _index + _number.length();
      else
        s+= _index + _numstart;
      
      if(e == _content->length())
        e = _index + _number.length();
      else
        e+= _index + _numstart;
      
      context->selection.set(seq, s, e);
      
      if(_number[_number.length() - 1] == '`'){
        if(pmath_char_is_digit(seq->text()[_index + 1])
        ||                     seq->text()[_index + 1] == '-'
        ||                     seq->text()[_index + 1] == '+'){
          seq->insert(_index + 1, " ");
        }
      }
      seq->insert(_index + 1, _number);
      seq->remove(_index, _index + 1); // deletes this
      return true;
    }
  
    if(selbox == seq){
      int s = context->selection.start;
      int e = context->selection.end;
      if(s > _index)
        s+= _number.length();
        
      if(e > _index)
        e+= _number.length();
      
      if(_number[_number.length() - 1] == '`'){
        if(pmath_char_is_digit(seq->text()[_index + 1])
        ||                     seq->text()[_index + 1] == '-'
        ||                     seq->text()[_index + 1] == '+'){
          seq->insert(_index + 1, " ");
        }
      }
      seq->insert(_index + 1, _number);
      seq->remove(_index, _index + 1); // deletes this
      
      context->selection.set(selbox, s, e);
      return true;
    }
  }
  
  return false;
}

void NumberBox::resize(Context *context){
  bool old_math_spacing     = context->math_spacing;
  bool old_show_auto_styles = context->show_auto_styles;
  context->math_spacing     = false;
  context->show_auto_styles = false;
  
  OwnerBox::resize(context);
  
  context->math_spacing     = old_math_spacing;
  context->show_auto_styles = old_show_auto_styles;
}

void NumberBox::paint(Context *context){
  bool old_math_spacing     = context->math_spacing;
  bool old_show_auto_styles = context->show_auto_styles;
  context->math_spacing     = false;
  context->show_auto_styles = false;
  
  OwnerBox::paint(context);
  
  context->math_spacing     = old_math_spacing;
  context->show_auto_styles = old_show_auto_styles;
}

Expr NumberBox::prepare_boxes(Expr boxes){
  if(boxes.is_string()){
    String s = String(boxes);
    
    const uint16_t *buf = s.buffer();
    const int       len = s.length();
    
    if(len > 0 && buf[0] >= '0' && buf[0] <= '9'){
      for(int i = 0;i < len;++i){
        if(buf[i] == '`' || buf[i] == '^'){
          return Call(
            GetSymbol(NumberBoxSymbol),
            s);
        }
      }
    }
    
    return s;
  }
  
  if(boxes[0] == PMATH_SYMBOL_LIST){
    for(size_t i = 0;i <= boxes.expr_length();++i){
      boxes.set(i, prepare_boxes(boxes[i]));
    }
  }
  
  return boxes;
}
  
  static int base36_value(uint16_t ch){
    if('0' <= ch && ch <= '9')
      return (int)(ch - '0');
    
    if('a' <= ch && ch <= 'z')
      return (int)(ch - 'a' + 10);
    
    if('A' <= ch && ch <= 'Z')
      return (int)(ch - 'A' + 10);
    
    return 0;
  }
  
  static String round_digits(String number, int maxdigits, int base){ // xxx.xxx, without sign or exponent or trailing "`"
    static const char alphabet_low[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    
    static Array<uint16_t> newbuf_array;
    
    const uint16_t *buf = number.buffer();
    int             len  = number.length();
    
    if(maxdigits < 1)
      maxdigits = 1;
    
    int decimal_point = 0;
    while(decimal_point < len && pmath_char_is_36digit(buf[decimal_point]))
      ++decimal_point;
    
    if(decimal_point == len || buf[decimal_point] != '.')
      return number;
      
    if(buf[0] == '0')
      ++maxdigits;
    
    newbuf_array.length(len);
    uint16_t *newbuf = newbuf_array.items();
    memcpy(newbuf, buf, decimal_point * sizeof(uint16_t));
    
    int i;
    for(i = decimal_point+1;i < len && pmath_char_is_36digit(buf[i]);++i)
      newbuf[i-1] = buf[i];
    
    if(i != len || len <= maxdigits + 1)
      return number;
    
    bool round_up = base36_value(newbuf[maxdigits]) >= base - base/2; // round base/2 up
    
    i = maxdigits-1;
    if(round_up){
      while(i >= 0){
        int val = base36_value(newbuf[i]);
        
        if(val >= base-1){
          newbuf[i] = '0';
          --i;
        }
        else{
          newbuf[i] = alphabet_low[val + 1];
          break;
        }
      }
    }
    
    while(maxdigits > decimal_point+1 && newbuf[maxdigits-1] == '0')
      --maxdigits;
    
    String num;
    if(maxdigits <= decimal_point)
      num = String::FromUcs2(newbuf, decimal_point) + ".0";
    else
      num = String::FromUcs2(newbuf, decimal_point) + "." + String::FromUcs2(newbuf + decimal_point, maxdigits - decimal_point);
    
    if(i < 0)
      return String("1") + num;
    return num;
  }

void NumberBox::set_number(String n){
  _number = n;
  _base     = 0;
  _exponent = 0;
  int numbase = 0;
  
  const uint16_t *buf = _number.buffer();
  const int       len = _number.length();
  
  _numstart = 0;
  while(_numstart < len && pmath_char_is_digit(buf[_numstart])){
    numbase = 10 * numbase + (int)(buf[_numstart] - '0');
    ++_numstart;
  }
  
  if(_numstart + 1 < len
  && buf[_numstart]     == '^'
  && buf[_numstart + 1] == '^'){
    _numstart+= 2;
  }
  else{
    numbase = 10;
    _numstart = 0;
  }
  
  _numend = _numstart;
  while(_numend < len && buf[_numend] != '`')
    ++_numend;
  
  _expstart = _numend;
  _content->remove(0, _content->length());
  
  if((_numend + 1 == len || (_numend + 1 < len && buf[_numend + 1] != '`'))
  && numbase >= 2 
  && numbase <= 36){
    // machine number: do not show all digits
    int digits = 6;
    
    if(numbase != 10){
      digits = (int)ceil(digits * log(10.0) / log(numbase));
    }
    
    _content->insert(0, round_digits(_number.part(_numstart, _numend - _numstart), digits, numbase));
  }
  else
    _content->insert(0, _number.part(_numstart, _numend - _numstart));
  
  if(_numstart > 2){
    SubsuperscriptBox *bas = new SubsuperscriptBox(new MathSequence, 0);
    _base = bas->subscript();
    
    _base->insert(0, _number.part(0, _numstart-2));
    _content->insert(_content->length(), bas);
  }
    
  while(_expstart < len && buf[_expstart] != '^')
    ++_expstart;
  
  if(_expstart + 1 < len && buf[_expstart] == '^'){
    _expstart++;
    
    SubsuperscriptBox *exp = new SubsuperscriptBox(0, new MathSequence);
    _exponent = exp->superscript();
    
    _exponent->insert(0, _number.part(_expstart, -1));
    
    int i = _content->length();
    _content->insert(i++, 0x2006);
    _content->insert(i++, 0x00D7);
    _content->insert(i++, 0x2006);
    if(_numstart > 2){
      _content->insert(i, _number.part(0, _numstart-2));
      _content->insert(i+_numstart-2, exp);
    }
    else{
      _content->insert(i, "10");
      _content->insert(i+2, exp);
    }
  }
  else
    _expstart = -1;
}

//} ... class NumberBox
