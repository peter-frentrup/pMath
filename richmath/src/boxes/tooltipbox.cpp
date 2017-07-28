#include <boxes/tooltipbox.h>

#include <boxes/mathsequence.h>

#include <gui/document.h>
#include <gui/native-widget.h>


using namespace richmath;

//{ class TooltipBox ...

TooltipBox::TooltipBox()
  : ExpandableAbstractStyleBox(0)
{
}

bool TooltipBox::try_load_from_object(Expr expr, int opts) {
  if(expr[0] != PMATH_SYMBOL_TOOLTIPBOX)
    return false;
  
  if(expr.expr_length() < 2)
    return false;
    
  Expr options(pmath_options_extract(expr.get(), 2));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  
  if(style){
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
  
  _content->load_from_object(expr[1], opts);
  
  tooltip_boxes = expr[2];
  
  return true;
}

Expr TooltipBox::to_pmath(BoxFlags flags) {
  if(has(flags, BoxFlags::Parseable) && get_own_style(StripOnInput, true)) {
    return _content->to_pmath(flags);
  }
  
  Gather g;
  
  Gather::emit(content()->to_pmath(flags));
  Gather::emit(tooltip_boxes);
  
  if(style)
    style->emit_to_pmath(false);
    
  Expr result = g.end();
  result.set(0, Symbol(PMATH_SYMBOL_TOOLTIPBOX));
  return result;
}

void TooltipBox::on_mouse_enter() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->show_tooltip(tooltip_boxes);
}

void TooltipBox::on_mouse_exit() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->hide_tooltip();
}

//} ... class TooltipBox
