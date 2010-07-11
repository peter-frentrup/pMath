#include <boxes/interpretationbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class InterpretationBox ...

InterpretationBox::InterpretationBox()
: OwnerBox()
{
  style = new Style;
  style->set(Editable, false);
}

InterpretationBox::InterpretationBox(MathSequence *content)
: OwnerBox(content)
{
  style = new Style;
  style->set(Editable, false);
}

InterpretationBox::InterpretationBox(MathSequence *content, Expr _interpretation)
: OwnerBox(content),
  interpretation(_interpretation)
{
  style = new Style;
  style->set(Editable, false);
}

pmath_t InterpretationBox::to_pmath(bool parseable){
  pmath_gather_begin(0);
  
  pmath_emit(_content->to_pmath(parseable), 0);
  
  pmath_emit(pmath_ref(interpretation.get()), 0);
  
  int i;
  
  if(style->get(AutoDelete, &i)){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_EDITABLE),
        pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)),
      NULL);
  }
  
  if(style->get(Editable, &i) && i){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_EDITABLE),
        pmath_ref(PMATH_SYMBOL_TRUE)),
      NULL);
  }
  
  return pmath_expr_set_item(
    pmath_gather_end(), 0,
    pmath_ref(PMATH_SYMBOL_INTERPRETATIONBOX));
}

bool InterpretationBox::edit_selection(Context *context){
  if(!OwnerBox::edit_selection(context))
    return false;
  
  if(get_own_style(AutoDelete)){
    MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
    
    if(seq){
      int len = _content->length();
      
      if(context->selection.get() == _content){
        int s = context->selection.start + _index;
        int e = context->selection.end + _index;
        context->selection.set(seq, s, e);
      }
      
      seq->insert(_index + 1, _content, 0, len);
      seq->remove(_index, _index + 1);
    }
  }
  
  return true;
}

//} ... class InterpretationBox
