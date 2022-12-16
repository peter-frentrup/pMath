#include <boxes/panelbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <gui/native-widget.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String AddressBand;
  extern String Framed;
  extern String Frameless;
  extern String Panel;
  extern String Popup;
  extern String TabBody;
  extern String TabHead;
  extern String TabPanelTopLeft;
  extern String TabPanelTopCenter;
  extern String TabPanelTopRight;
  extern String TabPanelCenterLeft;
  extern String TabPanelCenter;
  extern String TabPanelCenterRight;
  extern String TabPanelBottomLeft;
  extern String TabPanelBottomCenter;
  extern String TabPanelBottomRight;
  extern String Tooltip;
}}

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_PanelBox;

static ContainerType parse_panel_appearance(Expr expr);

//{ class PanelBox ...

PanelBox::PanelBox(AbstractSequence *content)
  : ContainerWidgetBox(ContainerType::Panel, content)
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
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

ControlState PanelBox::calc_state(Context &context) {
  if(!enabled())
    return ControlState::Disabled;
  
  if(selection_inside()) {
    if(mouse_inside())
      return ControlState::PressedHovered;
      
    return ControlState::Pressed;
  }
  
  if(mouse_inside())
    return ControlState::Hot;
  
  return base::calc_state(context);
}

bool PanelBox::expand(const BoxSize &size) {
  base::expand(size);
  _extents = size;
  cx = (_extents.width - _content->extents().width) / 2;
  return true;
}

void PanelBox::resize_default_baseline(Context &context) {
//  float old_width = context.width;
//  context.width = HUGE_VAL;
  
  type = parse_panel_appearance(get_own_style(Appearance));
  
  base::resize_default_baseline(context);
  
//  context.width = old_width;
  
  // like BaselinePosition -> (Center -> Axis)
  float baseline_adjust = calculate_scaled_baseline(0.5) - 0.25 * context.canvas().get_font_size();
  cy+= baseline_adjust;
  _extents.ascent-= baseline_adjust;
  _extents.descent+= baseline_adjust;
}

Expr PanelBox::to_pmath_symbol() { 
  return Symbol(richmath_System_PanelBox); 
}

Expr PanelBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::Panel)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PanelBox));
  return e;
}

void PanelBox::reset_style() {
  Style::reset(style, strings::Panel);
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
    String s = PMATH_CPP_MOVE(expr);
    
    if(s == strings::Framed)               return ContainerType::Panel;
    if(s == strings::Frameless)            return ContainerType::None;
    if(s == strings::AddressBand)          return ContainerType::AddressBandBackground;
    if(s == strings::Popup)                return ContainerType::PopupPanel;
    if(s == strings::TabBody)              return ContainerType::TabBodyBackground;
    if(s == strings::TabHead)              return ContainerType::TabHeadBackground;
    if(s == strings::TabPanelTopLeft)      return ContainerType::TabPanelTopLeft;
    if(s == strings::TabPanelTopCenter)    return ContainerType::TabPanelTopCenter;
    if(s == strings::TabPanelTopRight)     return ContainerType::TabPanelTopRight;
    if(s == strings::TabPanelCenterLeft)   return ContainerType::TabPanelCenterLeft;
    if(s == strings::TabPanelCenter)       return ContainerType::TabPanelCenter;
    if(s == strings::TabPanelCenterRight)  return ContainerType::TabPanelCenterRight;
    if(s == strings::TabPanelBottomLeft)   return ContainerType::TabPanelBottomLeft;
    if(s == strings::TabPanelBottomCenter) return ContainerType::TabPanelBottomCenter;
    if(s == strings::TabPanelBottomRight)  return ContainerType::TabPanelBottomRight;
    if(s == strings::Tooltip)              return ContainerType::TooltipWindow;
    
    return ContainerType::Panel;
  }
  
  if(expr == richmath_System_None)      return ContainerType::None;
  if(expr == richmath_System_Automatic) return ContainerType::Panel;
  
  return ContainerType::Panel;
}
