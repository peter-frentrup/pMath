#include <boxes/containerwidgetbox.h>


#include <boxes/mathsequence.h>
#include <eval/dynamic.h>
#include <gui/control-glow.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <boxes/inputfieldbox.h>
#include <boxes/panebox.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>
#include <cmath>


using namespace richmath;

namespace richmath { namespace strings {
  extern String ControlStyle;
}}

static const LengthConversionFactors WidgetBoxMarginFactors = {
  0.0f, // Automatic
   1 / 16.0f, // Tiny     For FontSize->12, 96 dpi: 12 * 1/16 = 0.75pt = 1 pixel
   3 / 16.0f, // Small
   5 / 16.0f, // Medium
  10 / 16.0f, // Large
};

//{ class ContainerWidgetBox ...

ContainerWidgetBox::ContainerWidgetBox(ContainerType _type, AbstractSequence *content)
  : AbstractStyleBox(content),
    margins(0.0f),
    type(_type),
    old_state(ControlState::Normal),
    _unused_u16(0)
{
  reset_style(); // caution: this does not call the derived reset_style(), but our implementation below
}

ControlState ContainerWidgetBox::calc_state(Context &context) {
  if(!enabled())
    return ControlState::Disabled;
  
  if(context.selection.id == id() && context.active)
    return ControlState::Pressed;
    
  if(context.clicked_box_id == id() && mouse_left_down()) {
    if(mouse_inside())
      return ControlState::PressedHovered;
      
    return ControlState::Pressed;
  }
  
  Box *mo = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
  if(mo && mo->mouse_sensitive() == this)
    return ControlState::Hovered;
    
  return ControlState::Normal;
}

void ContainerWidgetBox::resize_default_baseline(Context &context) {
  Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length h = get_own_style(ImageSizeVertical,   SymbolicSize::Invalid);
  
  float em = context.canvas().get_font_size();
  auto old_w = context.width;
  
  float forced_w = 0;
  if(w != SymbolicSize::Automatic) {
    forced_w = w.resolve(em, LengthConversionFactors::ControlWidth, old_w);
    
    if(get_own_style(LineBreakWithin, true))
      context.width = forced_w; //std::min(context.width, forced_w);
    
    if(h == SymbolicSize::Invalid) {
      if(w.is_symbolic())
        h = w; // ImageSize -> Tiny   means   ImageSize -> {Tiny, Tiny} etc. for controls
    }
  }
  
  if(h == SymbolicSize::Invalid)
    h = SymbolicSize::Automatic;
  
  float forced_h = 0;
  if(h != SymbolicSize::Automatic) {
    forced_h = h.resolve(em, LengthConversionFactors::ControlHeight, old_w);
  }
  
  Margins<Length> frame_margins {
    get_own_style(FrameMarginLeft,   SymbolicSize::Automatic),
    get_own_style(FrameMarginRight,  SymbolicSize::Automatic),
    get_own_style(FrameMarginTop,    SymbolicSize::Automatic),
    get_own_style(FrameMarginBottom, SymbolicSize::Automatic)};
  
  bool has_auto_fame = frame_margins == Margins<SymbolicSize>(SymbolicSize::Automatic);
  
  margins = Margins<float>(0.0f);
  if(w != SymbolicSize::Automatic || h != SymbolicSize::Automatic || !has_auto_fame || context.width < HUGE_VAL) {
    BoxSize dummy(1000, 1000, 1000);
    BoxSize expanded = dummy;
    ControlPainter::std->calc_container_size(*this, context.canvas(), type, &expanded);
    expanded.ascent  -= dummy.ascent;
    expanded.descent -= dummy.descent;
    expanded.width   -= dummy.width;
    margins.left   = expanded.width / 2;
    margins.right  = expanded.width / 2;
    margins.top    = expanded.ascent;
    margins.bottom = expanded.ascent;
    //pmath_debug_print("[w: %s(%f), h: %s(%f)]\n", w.is_symbolic() ? "sym" : "val", w.is_symbolic() ? (float)w.symblic_value() : w.raw_value(), 
    //                                              h.is_symbolic() ? "sym" : "val", h.is_symbolic() ? (float)h.symblic_value() : h.raw_value());
  }
  
  Margins<float> extra_padding {0.0f};
  if(!has_auto_fame) {
    // TODO: first remove additional 'Automatic' frame margins applied by ControlPainter
    
    auto auto_padding = ControlPainter::std->container_padding(*this, type);
    
    if(frame_margins.left != SymbolicSize::Automatic)
      extra_padding.left   = frame_margins.left.resolve(  em, WidgetBoxMarginFactors, old_w) - auto_padding.left;
    if(frame_margins.right != SymbolicSize::Automatic)
      extra_padding.right  = frame_margins.right.resolve( em, WidgetBoxMarginFactors, old_w) - auto_padding.right;
    if(frame_margins.top != SymbolicSize::Automatic)
      extra_padding.top    = frame_margins.top.resolve(   em, WidgetBoxMarginFactors, old_w) - auto_padding.top;
    if(frame_margins.bottom != SymbolicSize::Automatic)
      extra_padding.bottom = frame_margins.bottom.resolve(em, WidgetBoxMarginFactors, old_w) - auto_padding.bottom;
  }
  
  if(extra_padding.left + extra_padding.right > context.width - (margins.left + margins.right)) {
     float too_wide = extra_padding.left + extra_padding.right - (context.width - (margins.left + margins.right));
     extra_padding.left-= too_wide/2;
     if(extra_padding.left < 0) extra_padding.left = 0;
     extra_padding.right = context.width - extra_padding.left;
     if(extra_padding.right < 0) extra_padding.right = 0;
  }
  
  context.width -= (margins.left + margins.right + extra_padding.left + extra_padding.right);
  
  base::resize_default_baseline(context);
  
  context.width = old_w;
  
  _extents.width   += extra_padding.left + extra_padding.right;
  _extents.ascent  += extra_padding.top;
  _extents.descent += extra_padding.bottom;
  
  if(w != SymbolicSize::Automatic) {
    _extents.width = forced_w - margins.left - margins.right;
  }
  
  if(h != SymbolicSize::Automatic) {
    if(forced_h <= _extents.height()) {
      margins.top = margins.bottom = 0;
    }
    else if(forced_h < _extents.height() + margins.top + margins.bottom) {
      float max_margin_h = (forced_h - _extents.height());
      margins.top = margins.bottom = max_margin_h / 2;
    }
    
    _extents.ascent -= margins.top;
    _extents.descent = forced_h - margins.top - margins.bottom - _extents.ascent;
  }
  else if(get_own_style(ContentPadding, false)) {
    BoxSize &content_extents = content()->var_extents();
    if(content_extents.ascent < 0.75f * em)
       content_extents.ascent = 0.75f * em;
    if(content_extents.descent < 0.25f * em)
       content_extents.descent = 0.25f * em;
    
    if(_extents.ascent < content_extents.ascent + extra_padding.top) {
      _extents.ascent = content_extents.ascent + extra_padding.top;
    }
    if(_extents.descent < content_extents.descent + extra_padding.bottom) {
      _extents.descent = content_extents.descent + extra_padding.bottom;
    }
  }
  
  auto old_size = _extents;
  
  ControlPainter::std->calc_container_size(
    *this,
    context.canvas(),
    type,
    &_extents);
  
  if(!w.is_symbolic())
    _extents.width = old_size.width + margins.left + margins.right;
  
  if(!h.is_symbolic()) {
    _extents.ascent  = old_size.ascent  + margins.top;
    _extents.descent = old_size.descent + margins.bottom;
  }

  margins.left   = extra_padding.left   + (_extents.width - old_size.width) / 2;
  margins.right  = extra_padding.right  + (_extents.width - old_size.width) / 2;
  margins.top    = extra_padding.top    + _extents.ascent  - old_size.ascent;
  margins.bottom = extra_padding.bottom + _extents.descent - old_size.descent;
  
  apply_alignment();
}

void ContainerWidgetBox::apply_alignment() {
  SimpleAlignment alignment = SimpleAlignment::from_pmath(get_own_style(Alignment), default_alignment());
  
  cx = alignment.interpolate_left_to_right(margins.left, _extents.width - content()->extents().width - margins.right);
  
  float eh = _extents.height();
  float ca = content()->extents().ascent  + margins.top;
  float cd = content()->extents().descent + margins.bottom;
  if(ca + cd < eh) {
    _extents.ascent  = alignment.interpolate_bottom_to_top(eh - cd,      ca);
    _extents.descent = alignment.interpolate_bottom_to_top(     cd, eh - ca);
  }
}

void ContainerWidgetBox::paint(Context &context) {
  Point pos = context.canvas().current_pos();
  
  RectangleF rect = _extents.to_rectangle(pos);
  //rect.pixel_align(context.canvas(), false); // The animation creation functions round outward, others round to nearest. THat could cause pixel jumping
  ControlState state = calc_state(context);
  
  if(animation && !animation->is_compatible(context.canvas(), rect.width, rect.height)) 
    animation = nullptr;
  
  if(state != old_state || !animation) {
    animation = ControlPainter::std->control_transition(
                  id(),
                  context.canvas(),
                  type,
                  type,
                  old_state,
                  state,
                  rect);
    
    old_state = state;
  }
  
  bool need_bg = true;
  if(animation) {
    animation->update(this);
    if(animation->paint(context.canvas())) {
      need_bg = false;
    }
    else {
      animation = ControlPainter::std->control_transition(
                    id(),
                    context.canvas(),
                    type,
                    type,
                    old_state,
                    old_state,
                    rect);
    }
  }
  
  bool very_transparent = ControlPainter::std->is_very_transparent(*this, type, state);
  
  if(need_bg) {
    ControlPainter::std->draw_container(
      *this,
      context.canvas(),
      type,
      state,
      rect);
  }
  
  if(!ControlGlowHook::all_disabled) {
    Margins<float> glow_outer {0.0f};
    Margins<float> glow_inner {0.0f};
    if(ControlPainter::std->control_glow_margins(*this, type, state, &glow_outer, &glow_inner)) {
      Box *hook_anchor = nullptr;
      for(Box *tmp = parent(); tmp; tmp = tmp->parent()) {
        if(dynamic_cast<AbstractSequence*>(tmp)) {
          hook_anchor = tmp;
          continue;
        }
        
        if(dynamic_cast<InputFieldBox*>(tmp))
          break;
        if(dynamic_cast<PaneBox*>(tmp))
          break;
      }
      
      if(hook_anchor) {
        auto hook = new ControlGlowHook(this, ControlPainter::std->control_glow_type(*this, type), state);
        hook->outside = glow_outer;
        hook->inside  = glow_inner;
        context.post_paint_hooks.add(hook_anchor, hook);
      }
    }
  }
  
  
  context.canvas().move_to(pos + ControlPainter::std->container_content_offset(*this, type, state));
  
  Color old_cursor_color = context.cursor_color;
  Color old_color        = context.canvas().get_color();
  if(Color c = ControlPainter::std->control_font_color(*this, type, state)) {
    context.canvas().set_color(c);
    context.cursor_color = c;
  }
  
  if(very_transparent || !context.canvas().show_only_text)
    base::paint(context);
    
  if(type == ContainerType::FramelessButton && state == ControlState::PressedHovered) {
    context.canvas().save();
    {
    /* Workaround for Cairo/win32 1.10.0 bug (fixed in 1.12.0?):
       Fill with CAIRO_OPERATOR_DIFFERENCE crshes the simple rectangle fast path
     */
      rect.add_round_rect_path(context.canvas(), BoxRadius(0.75));
      //rect.add_rect_path(context.canvas());
      
      cairo_set_operator(context.canvas().cairo(), CAIRO_OPERATOR_DIFFERENCE);
      context.canvas().set_color(Color::White);
      context.canvas().fill();
    }
    context.canvas().restore();
  }
  
  context.canvas().set_color(old_color);
  context.cursor_color = old_cursor_color;
}

void ContainerWidgetBox::reset_style() {
  style.reset(strings::ControlStyle);
}

void ContainerWidgetBox::on_mouse_enter() {
  if(!mouse_inside() && ControlPainter::std->container_hover_repaint(*this, type) && enabled())
    request_repaint_all();
    
  mouse_inside(true);
  base::on_mouse_enter();
}

void ContainerWidgetBox::on_mouse_exit() {
  if(/*mouse_inside && */ControlPainter::std->container_hover_repaint(*this, type) && enabled())
    request_repaint_all();
    
  mouse_inside(false);
  base::on_mouse_exit();
}

void ContainerWidgetBox::on_mouse_down(MouseEvent &event) {
  if(!enabled())
    return;
    
  event.set_origin(this);
  
  if(event.left)   mouse_left_down(true);
  if(event.middle) mouse_middle_down(true);
  if(event.right)  mouse_right_down(true);
  mouse_inside(_extents.to_rectangle().contains(event.position));
                 
  request_repaint_all();
}

void ContainerWidgetBox::on_mouse_move(MouseEvent &event) {
  if(!enabled())
    return;
    
  event.set_origin(this);
  
  bool mi = _extents.to_rectangle().contains(event.position);
  if(mi != mouse_inside())
    request_repaint_all();
    
  mouse_inside(mi);
}

void ContainerWidgetBox::on_mouse_up(MouseEvent &event) {
  if(enabled())
    request_repaint_all();
  
  if(event.left)   mouse_left_down(false);
  if(event.middle) mouse_middle_down(false);
  if(event.right)  mouse_right_down(false);
}

void ContainerWidgetBox::on_mouse_cancel() {
  if(enabled())
    request_repaint_all();
  
  mouse_left_down(false);
  mouse_middle_down(false);
  mouse_right_down(false);
}

void ContainerWidgetBox::on_enter() {
  if(!ControlPainter::is_static_background(type))
    request_repaint_all();
  
  selection_inside(true);
  base::on_enter();
}

void ContainerWidgetBox::on_exit() {
  if(!ControlPainter::is_static_background(type))
    request_repaint_all();
  
  selection_inside(false);
  base::on_exit();
}

bool ContainerWidgetBox::is_foreground_window() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_foreground_window();
}

bool ContainerWidgetBox::is_focused_widget() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_focused_widget();
}

bool ContainerWidgetBox::is_using_dark_mode() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_using_dark_mode();
}

int ContainerWidgetBox::dpi() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return 96;
  
  AutoResetCurrentObserver guard;
  return doc->native()->dpi();
}

//} ... class ContainerWidgetBox
