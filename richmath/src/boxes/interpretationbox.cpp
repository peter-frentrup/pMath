#include <boxes/interpretationbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_InterpretationBox;

//{ class InterpretationBox ...

InterpretationBox::InterpretationBox()
  : ExpandableAbstractStyleBox()
{
  style = new Style;
  style->set(Editable, false);
}

InterpretationBox::InterpretationBox(MathSequence *content)
  : ExpandableAbstractStyleBox(content)
{
  style = new Style;
  style->set(Editable, false);
}

InterpretationBox::InterpretationBox(MathSequence *content, Expr _interpretation)
  : ExpandableAbstractStyleBox(content),
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

bool InterpretationBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_InterpretationBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options_expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
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
  
  finish_load_from_object(std::move(expr));
  return true;
}

Expr InterpretationBox::to_pmath_symbol() {
  return Symbol(richmath_System_InterpretationBox);
}

Expr InterpretationBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  g.emit(interpretation);
  style->emit_to_pmath();
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_InterpretationBox));
  return e;
}

bool InterpretationBox::edit_selection(Context *context) {
  if(!base::edit_selection(context))
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
