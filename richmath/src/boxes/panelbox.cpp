#include <boxes/panelbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <gui/native-widget.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_PanelBox;

//{ class PanelBox ...

PanelBox::PanelBox(MathSequence *content)
  : ContainerWidgetBox(PanelControl, content)
{
}

bool PanelBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_PanelBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
  
  /* now success is guaranteed */
  
  content()->load_from_object(expr[1], opts);
  
  if(options != PMATH_UNDEFINED) {
    if(style){
      reset_style();
      style->add_pmath(options);
    }
    else
      style = new Style(options);
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

bool PanelBox::expand(const BoxSize &size) {
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void PanelBox::resize_default_baseline(Context *context) {
//  float old_width = context->width;
//  context->width = HUGE_VAL;
  
  ContainerWidgetBox::resize_default_baseline(context);
  
//  context->width = old_width;
  
  // like BaselinePosition -> (Center -> Axis)
  float baseline_adjust = calculate_scaled_baseline(0.5) - 0.25 * context->canvas->get_font_size();
  cy+= baseline_adjust;
  _extents.ascent-= baseline_adjust;
  _extents.descent+= baseline_adjust;
}

Expr PanelBox::to_pmath_symbol() { 
  return Symbol(richmath_System_PanelBox); 
}

Expr PanelBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style)
    style->emit_to_pmath(false);
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PanelBox));
  return e;
}

//} ... class PanelBox
