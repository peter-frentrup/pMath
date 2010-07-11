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

pmath_t NumberBox::to_pmath(bool parseable){
  return pmath_ref(_number.get());
}

pmath_t NumberBox::prepare_boxes(pmath_t boxes){
  if(pmath_instance_of(boxes, PMATH_TYPE_STRING)){
    const uint16_t *buf = pmath_string_buffer(boxes);
    const int       len = pmath_string_length(boxes);
    
    if(len > 0 && buf[0] >= '0' && buf[1] <= '9'){
      for(int i = 0;i < len;++i)
        if(buf[i] == '`'){
          return pmath_expr_new_extended(
            pmath_ref(GetSymbol(NumberBoxSymbol)), 1,
            boxes);
        }
    }
    
    return boxes;
  }
  
  if(pmath_instance_of(boxes, PMATH_TYPE_EXPRESSION)){
    pmath_t head = pmath_expr_get_item(boxes, 0);
    pmath_unref(head);
    
    if(head != PMATH_SYMBOL_LIST)
      return boxes;
    
    for(size_t i = 0;i <= pmath_expr_length(boxes);++i){
      boxes = pmath_expr_set_item(boxes, i, 
        prepare_boxes(pmath_expr_get_item(boxes, i)));
    }
  }
  
  return boxes;
}

void NumberBox::set_number(String n){
  _number = n;
  _exponent = 0;
  
  const uint16_t *buf = _number.buffer();
  const int       len = _number.length();
  
  _numend = 0;
  while(_numend < len && buf[_numend] != '`')
    ++_numend;
  
  _content->remove(0, _content->length());
  _content->insert(0, _number.part(0, _numend));
  
  _expstart = _numend;
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
