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
  reset_style();
}

void InterpretationBox::reset_style() {
  if(style)
    style->clear();
  else
    style = new Style;
    
  style->set(Editable, false);
}

bool InterpretationBox::try_load_from_object(Expr expr, BoxOptions opts) {
  if(expr[0] != PMATH_SYMBOL_INTERPRETATIONBOX)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options_expr(pmath_options_extract(expr.get(), 2));
  if(options_expr.is_null())
    return false;
    
  /* now success is guaranteed */
  
  _content->load_from_object(expr[1], opts);
  interpretation = expr[2];
  
  if(options_expr != PMATH_UNDEFINED) {
    if(style) {
      reset_style();
      style->add_pmath(options_expr);
    }
    else
      style = new Style(options_expr);
  }
  
  return true;
}

Expr InterpretationBox::to_pmath(BoxFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  g.emit(interpretation);
  
  int i;
  
  if(style->get(AutoDelete, &i)) {
    g.emit(
      Rule(
        Symbol(PMATH_SYMBOL_EDITABLE),
        Symbol(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
  }
  
  if(style->get(Editable, &i) && i) {
    g.emit(
      Rule(
        Symbol(PMATH_SYMBOL_EDITABLE),
        Symbol(PMATH_SYMBOL_TRUE)));
  }
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_INTERPRETATIONBOX));
  return e;
}

bool InterpretationBox::edit_selection(Context *context) {
  if(!OwnerBox::edit_selection(context))
    return false;
    
  if(get_own_style(AutoDelete)) {
    if(MathSequence *seq = dynamic_cast<MathSequence *>(_parent)) {
      int len = _content->length();
      
      if(context->selection.get() == _content) {
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
