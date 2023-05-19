#include <boxes/tooltipbox.h>

#include <boxes/mathsequence.h>

#include <gui/document.h>
#include <gui/native-widget.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_TooltipBox;

//{ class TooltipBox ...

TooltipBox::TooltipBox(AbstractSequence *content)
  : base(content)
{
}

bool TooltipBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_TooltipBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new StyleData(options);
    
  _content->load_from_object(expr[1], opts);
  
  _tooltip_boxes = expr[2];
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

Expr TooltipBox::to_pmath_symbol() {
  return Symbol(richmath_System_TooltipBox);
}

Expr TooltipBox::to_pmath_impl(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Parseable) && get_own_style(StripOnInput, true)) {
    return _content->to_pmath(flags);
  }
  
  Gather g;
  
  Gather::emit(content()->to_pmath(flags));
  Gather::emit(_tooltip_boxes);
  
  if(style)
    style->emit_to_pmath(false);
    
  Expr result = g.end();
  result.set(0, Symbol(richmath_System_TooltipBox));
  return result;
}

void TooltipBox::on_mouse_enter() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->show_tooltip(this, _tooltip_boxes);
}

void TooltipBox::on_mouse_exit() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->hide_tooltip();
}

//} ... class TooltipBox
