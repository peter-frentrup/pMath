#include <boxes/numberbox.h>

#include <cfloat>

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
    if(selbox == _exponent){
      int s = context->selection.start + _index + _expstart;
      int e = context->selection.end   + _index + _expstart;
      context->selection.set(seq, s, e);
      
      if(_number[_number.length() - 1] == '`'){
        if(seq->text()[_index + 1] == '-'
        || seq->text()[_index + 1] == '+'){
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
        s+= _index;
      
      if(e == _content->length())
        e = _index + _number.length();
      else
        e+= _index;
      
      context->selection.set(seq, s, e);
      
      if(_number[_number.length() - 1] == '`'){
        if(seq->text()[_index + 1] == '-'
        || seq->text()[_index + 1] == '+'){
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
        if(seq->text()[_index + 1] == '-'
        || seq->text()[_index + 1] == '+'){
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

Expr NumberBox::prepare_boxes(Expr boxes){
  if(boxes.is_string()){
    String s = String(boxes);
    
    const uint16_t *buf = s.buffer();
    const int       len = s.length();
    
    if(len > 0 && buf[0] >= '0' && buf[1] <= '9'){
      for(int i = 0;i < len;++i)
        if(buf[i] == '`'){
          return Call(
            GetSymbol(NumberBoxSymbol),
            s);
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

  static String round_digits(String number, int maxdigits){ // xxx.xxx, without sign or xponent or trailing "`"
    
    static Array<uint16_t> newbuf_array;
    
    const uint16_t *buf = number.buffer();
    int             len  = number.length();
    
    if(maxdigits < 1)
      maxdigits = 1;
    
    int decimal_point = 0;
    while(decimal_point < len && '0' <= buf[decimal_point] && buf[decimal_point] <= '9')
      ++decimal_point;
    
    if(decimal_point == len || buf[decimal_point] != '.')
      return number;
    
    newbuf_array.length(len);
    uint16_t *newbuf = newbuf_array.items();
    memcpy(newbuf, buf, decimal_point * sizeof(uint16_t));
    
    int i;
    for(i = decimal_point+1;i < len && '0' <= buf[i] && buf[i] <= '9';++i)
      newbuf[i-1] = buf[i];
    
    if(i != len || len <= maxdigits + 1)
      return number;
    
    bool round_up = newbuf[maxdigits] >= '5';
    
    i = maxdigits-1;
    if(round_up){
      while(i >= 0){
        if(newbuf[i] == '9'){
          newbuf[i] = '0';
          --i;
        }
        else{
          newbuf[i] = newbuf[i] + 1;
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
  _exponent = 0;
  
  const uint16_t *buf = _number.buffer();
  const int       len = _number.length();
  
  _numend = 0;
  while(_numend < len && buf[_numend] != '`')
    ++_numend;
  
  _expstart = _numend;
  _content->remove(0, _content->length());
  
  if(_numend + 1 == len){
    // machine number: do not show all digits
    _content->insert(0, round_digits(_number.part(0, _numend), 6));
  }
  else
    _content->insert(0, _number.part(0, _numend));
    
  while(_expstart < len && buf[_expstart] != '^')
    ++_expstart;
  
  if(_expstart + 1 < len && buf[_expstart] == '^'){
    _expstart++;
    
    SubsuperscriptBox *exp = new SubsuperscriptBox(0, new MathSequence);
    _exponent = exp->superscript();
    
    _exponent->insert(0, _number.part(_expstart, -1));
    
    int i = _content->length();
    _content->insert(i,   0x00D7); // " "
    _content->insert(i+1, "10");
    _content->insert(i+3, exp);
  }
  else
    _expstart = -1;
}

//} ... class NumberBox
