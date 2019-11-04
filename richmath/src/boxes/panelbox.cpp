#include <boxes/panelbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <gui/native-widget.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_PanelBox;

static ContainerType parse_panel_appearance(Expr expr);

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
  
  reset_style();
  if(options != PMATH_UNDEFINED) 
    style->add_pmath(options);
  
  finish_load_from_object(std::move(expr));
  return true;
}

ControlState PanelBox::calc_state(Context *context) {
  if(!enabled())
    return Disabled;
  
  if(selection_inside) {
    if(mouse_inside)
      return PressedHovered;
      
    return Pressed;
  }
  
  return base::calc_state(context);
}

bool PanelBox::expand(const BoxSize &size) {
  base::expand(size);
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void PanelBox::resize_default_baseline(Context *context) {
//  float old_width = context->width;
//  context->width = HUGE_VAL;
  
  type = parse_panel_appearance(get_own_style(Appearance));
  
  base::resize_default_baseline(context);
  
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
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("Panel"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PanelBox));
  return e;
}

void PanelBox::reset_style() {
  Style::reset(style, "Panel");
}

void PanelBox::on_enter() {
  if(!ControlPainter::is_static_background(type))
    request_repaint_all();
  
  base::on_enter();
}

void PanelBox::on_exit() {
  if(!ControlPainter::is_static_background(type))
    request_repaint_all();
  
  base::on_exit();
}

//} ... class PanelBox

static ContainerType parse_panel_appearance(Expr expr) {
  if(expr.is_string()) {
    String s = std::move(expr);
    
    if(s.equals("Framed"))
      return PanelControl;
    
    if(s.equals("Frameless"))
      return NoContainerType;
    
    if(s.equals("AddressBand"))
      return AddressBandBackground;
    
    return PanelControl;
  }
  
  if(expr == PMATH_SYMBOL_NONE)
    return NoContainerType;
    
  if(expr == PMATH_SYMBOL_AUTOMATIC)
    return PanelControl;
  
  return PanelControl;
}
