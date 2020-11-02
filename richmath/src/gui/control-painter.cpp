#include <gui/control-painter.h>

#include <boxes/box.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <graphics/context.h>

#include <algorithm>
#include <cmath>


#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif


using namespace richmath;

namespace {
  static const Color ButtonColor = Color::from_rgb24(0xDCDCDC);
  static const Color ButtonHoverColor = Color::from_rgb24(0xE6E6E6);
  static const Color Button3DLightColor = Color::from_rgb24(0xF0F0F0);
  static const Color Button3DDarkColor = Color::from_rgb24(0x787878); // 0xB4B4B4
  static const Color GrayTextColor = Color::from_rgb24(0x808080);
  
  class ControlPainterImpl {
    public:
      static void paint_edge(
        Canvas          &canvas,
        const RectangleF &outer_rect,
        const RectangleF &inner_rect,
        Color           top_left_color,
        Color           bottom_right_color);
      
      static void paint_frame(
        Canvas           &canvas,
        const RectangleF &rect,
        bool              sunken,
        bool              enabled,
        Color             background_color = ButtonColor);
      
      static void paint_popup_panel(Canvas &canvas, RectangleF rect);
      static void paint_checkbox_mark(              Canvas &canvas, const RectangleF &rect, Color color);
      static void paint_checkbox_indeterminate_mark(Canvas &canvas, const RectangleF &rect, Color color);
      static void paint_radio_button_background(    Canvas &canvas, const RectangleF &rect, Color inner_color);
      static void paint_radio_button_mark(          Canvas &canvas, const RectangleF &rect, Color color);
      static void paint_opener_triangle_closed(     Canvas &canvas, const RectangleF &rect, Color color);
      static void paint_opener_triangle_opened(     Canvas &canvas, const RectangleF &rect, Color color);
      static void paint_navigation_back_arrow(Canvas &canvas, const RectangleF &rect, Color stroke_col, Color fill_col);
  };
}


//{ class ControlContext ...
namespace {
  class DummyControlContext: public ControlContext {
    public:
      virtual bool is_foreground_window() override { return true; }
      virtual bool is_focused_widget() override { return false; }
      virtual bool is_using_dark_mode() override { return false; }
      virtual int dpi() override { return 96; }
  };
}

static DummyControlContext dummy_control_context;
ControlContext &ControlContext::dummy = dummy_control_context;

ControlContext &ControlContext::find(Box *box) {
  if(!box)
    return dummy;
  
  if(auto cc = box->find_parent<ControlContext>(true))
    return *cc;
  
  if(auto doc = box->find_parent<Document>(true))
    return *doc->native();
  
  return dummy;
}

//} ... class ControlContext

//{ class ControlPainter ...

ControlPainter ControlPainter::generic_painter;
ControlPainter *ControlPainter::std = &generic_painter;

ControlPainter::ControlPainter()
  : Base()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void ControlPainter::calc_container_size(
  ControlContext &control,
  Canvas         &canvas,
  ContainerType   type,
  BoxSize        *extents // in/out
) {
  switch(type) {
    case NoContainerType:
    case FramelessButton: 
    case TabHeadBackground: break;
    
    case GenericButton:
    case PushButton:
    case PaletteButton:
    case AddressBandGoButton:
    case TooltipWindow:
    case ListViewItem:
    case ListViewItemSelected: {
        if(extents->ascent < canvas.get_font_size() * 0.75f)
          extents->ascent = canvas.get_font_size() * 0.75f;// - extents->ascent;
          
        if(extents->descent < canvas.get_font_size() * 0.25f)
          extents->descent = canvas.get_font_size() * 0.25f;// - extents->descent;
          
        extents->width +=   4.5;
        extents->ascent +=  2.25;
        extents->descent += 2.25;
      } break;
      
    case DefaultPushButton: {
        if(extents->ascent < canvas.get_font_size() * 0.75f)
          extents->ascent = canvas.get_font_size() * 0.75f;// - extents->ascent;
          
        if(extents->descent < canvas.get_font_size() * 0.25f)
          extents->descent = canvas.get_font_size() * 0.25f;// - extents->descent;
          
        extents->width +=   6.0;
        extents->ascent +=  3.0;
        extents->descent += 3.0;
      } break;
      
    case InputField: {
        extents->width +=   6.0;
        extents->ascent +=  3.0;
        extents->descent += 3.0;
      } break;
    
    case AddressBandInputField: {
        extents->width +=   3.0;
        extents->ascent +=  2.25;
        extents->descent += 2.25;
      } break;
    
    case AddressBandBackground: {
        extents->width +=   3.0;
        extents->ascent +=  0.75;
        extents->descent += 0.75;
      } break;
      
    case PopupPanel:
    case PanelControl: {
        extents->width +=   12.0;
        extents->ascent +=  6.0;
        extents->descent += 6.0;
      } break;
    
    case SliderHorzChannel: {
        extents->ascent  = 3.0;
        extents->descent = 1.0;
      } break;
      
    case SliderHorzThumb: {
        extents->width = extents->height() / 2;
      } break;
      
    case ProgressIndicatorBackground: {
        extents->ascent = 1 * canvas.get_font_size();
        extents->descent = 0;
        //extents->width = extents->height() * 15;
      } break;
      
    case ProgressIndicatorBar: {
//      extents->ascent *= 0.5;
//      extents->descent*= 0.5;
//      extents->width = extents->height() * 15;

        extents->ascent -= 0.75;
        extents->descent -= 0.75;
        extents->width  -= 1.5;
      } break;
      
    case CheckboxUnchecked:
    case CheckboxChecked:
    case CheckboxIndeterminate: {
        extents->width   = 13 * 0.75;
        extents->ascent  = extents->width * 0.75;
        extents->descent = extents->width * 0.25;
      } break;
      
    case RadioButtonUnchecked:
    case RadioButtonChecked: {
        extents->width   = 16 * 0.75;
        extents->ascent  = extents->width * 0.75;
        extents->descent = extents->width * 0.25;
      } break;
    
    case OpenerTriangleClosed:
    case OpenerTriangleOpened: {
        extents->width   = 16 * 0.75;
        extents->ascent  = extents->width * 0.75;
        extents->descent = extents->width * 0.25;
      } break;
    
    case NavigationBack:
    case NavigationForward:  {
        extents->width   = std::max(20 * 0.75,        (double)extents->width);
        extents->ascent  = std::max(20 * 0.75 * 0.75, (double)extents->ascent);
        extents->descent = std::max(20 * 0.75 * 0.25, (double)extents->descent);
      } break;
      
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        if(extents->ascent < canvas.get_font_size() * 0.75f)
          extents->ascent = canvas.get_font_size() * 0.75f;// - extents->ascent;
          
        if(extents->descent < canvas.get_font_size() * 0.25f)
          extents->descent = canvas.get_font_size() * 0.25f;// - extents->descent;
          
        extents->width +=   6.0;
        extents->ascent +=  4.5;
        extents->descent += 1.5;
      } break;
    
    case TabBodyBackground: {
        extents->width +=   12.0;
        extents->ascent +=  4.5;
        extents->descent += 6.0;
      } break;
  }
}

void ControlPainter::calc_container_radii(
  ControlContext       &control,
  ContainerType         type,
  BoxRadius            *radii
) {
  *radii = BoxRadius(0);
}


Color ControlPainter::control_font_color(ControlContext &control, ContainerType type, ControlState state) {
  if(is_very_transparent(control, type, state))
    return Color::None;
  
  if(state == Disabled)
    return GrayTextColor;
  
  if(type == ListViewItemSelected)
    return Color::White;
    
  return Color::Black;
}

bool ControlPainter::is_very_transparent(ControlContext &control, ContainerType type, ControlState state) {
  return type == NoContainerType || 
         type == FramelessButton || 
         type == AddressBandInputField || 
         type == OpenerTriangleClosed ||
         type == OpenerTriangleOpened ||
         type == TabHeadBackground;
}

void ControlPainter::draw_container(
  ControlContext &control, 
  Canvas         &canvas,
  ContainerType   type,
  ControlState    state,
  RectangleF      rect
) {
  rect.pixel_align(canvas, false);
  
  if(rect.width <= 0 || rect.height <= 0)
    return;
  
  canvas.save();
  
  switch(type) {
    case NoContainerType:
    case FramelessButton: break;
    
    case GenericButton:
    case PushButton:
    case PaletteButton:
      ControlPainterImpl::paint_frame(canvas, rect, state == PressedHovered, state != Disabled, (state == Hovered || state == PressedHovered) ? ButtonHoverColor : ButtonColor);
      break;
    
    case AddressBandGoButton:
      ControlPainterImpl::paint_frame(canvas, rect.enlarged_by(0, -0.75), state == PressedHovered, state != Disabled, (state == Hovered || state == PressedHovered) ? ButtonHoverColor : ButtonColor);
      break;
      
    case DefaultPushButton: {
        ControlPainterImpl::paint_frame(canvas, rect, state == PressedHovered, state != Disabled, (state == Hovered || state == PressedHovered) ? ButtonHoverColor : ButtonColor);
        
        rect.add_rect_path(canvas, false);
        rect.grow(0.75, 0.75);
        rect.add_rect_path(canvas, true);
        
        Color c = canvas.get_color();
        canvas.set_color(Color::Black);
        canvas.fill();
        canvas.set_color(c);
      } break;
      
    case InputField:
    case AddressBandBackground:
      ControlPainterImpl::paint_frame(canvas, rect, true, state != Disabled, Color::White);
      break;
      
    case TooltipWindow:
      ControlPainterImpl::paint_frame(canvas, rect, false, false, Color::from_rgb24(0xFFF4C1));
      break;
      
    case ListViewItem: {
        Color c = canvas.get_color();
        rect.add_rect_path(canvas);
        canvas.set_color(Color::White);
        canvas.fill();
        canvas.set_color(c);
      } break;
      
    case ListViewItemSelected: {
        Color c = canvas.get_color();
        rect.add_rect_path(canvas);
        canvas.set_color(Color::from_rgb24(0x0099ff));
        canvas.fill();
        canvas.set_color(c);
      } break;
    
    case PanelControl:
      ControlPainterImpl::paint_frame(canvas, rect, false, true);
      break;
    
    case PopupPanel: 
      ControlPainterImpl::paint_popup_panel(canvas, rect);
      break;
    
    case SliderHorzChannel:
      ControlPainterImpl::paint_frame(canvas, rect, true, state != Disabled);
      break;
      
    case SliderHorzThumb:
      ControlPainterImpl::paint_frame(canvas, rect, false, state != Disabled, (state == Hovered || state == PressedHovered) ? ButtonHoverColor : ButtonColor);
      break;
      
    case ProgressIndicatorBackground:
      ControlPainterImpl::paint_frame(canvas, rect, true, true);
      break;
      
    case ProgressIndicatorBar: {
        rect.grow(-1.5, -1.5);
        
        if(rect.width > 0) {
          Color c = canvas.get_color();
          rect.add_rect_path(canvas);
          canvas.set_color(Color::from_rgb24(0, 0, 0x80));
          canvas.fill();
          canvas.set_color(c);
        }
      } break;
      
    case CheckboxUnchecked:
      if(state == Disabled)
        ControlPainterImpl::paint_frame(canvas, rect, true, true);
      else
        ControlPainterImpl::paint_frame(canvas, rect, true, true, Color::White);
        
      break;
      
    case CheckboxChecked: {
        if(state == Disabled) {
          ControlPainterImpl::paint_frame(canvas, rect, true, true);
          ControlPainterImpl::paint_checkbox_mark(canvas, rect, Button3DDarkColor);
        }
        else {
          ControlPainterImpl::paint_frame(canvas, rect, true, true, Color::White);
          ControlPainterImpl::paint_checkbox_mark(canvas, rect, Color::Black);
        }
      } break;
      
    case CheckboxIndeterminate: {
        if(state == Disabled) {
          ControlPainterImpl::paint_frame(canvas, rect, true, true);
          ControlPainterImpl::paint_checkbox_indeterminate_mark(canvas, rect, Button3DDarkColor);
        }
        else {
          ControlPainterImpl::paint_frame(canvas, rect, true, true, Color::White);
          ControlPainterImpl::paint_checkbox_indeterminate_mark(canvas, rect, Color::Black);
        }
        
      } break;
      
    case RadioButtonUnchecked: {
        if(state == Disabled) 
          ControlPainterImpl::paint_radio_button_background(canvas, rect, ButtonColor);
        else 
          ControlPainterImpl::paint_radio_button_background(canvas, rect, Color::White);
      } break;
    
    case RadioButtonChecked: {
        if(state == Disabled) {
          ControlPainterImpl::paint_radio_button_background(canvas, rect, ButtonColor);
          ControlPainterImpl::paint_radio_button_mark(      canvas, rect, Button3DDarkColor);
        }
        else {
          ControlPainterImpl::paint_radio_button_background(canvas, rect, Color::White);
          ControlPainterImpl::paint_radio_button_mark(      canvas, rect, Color::Black);
        }
      } break;
  
    case OpenerTriangleClosed: {
        if(state == Disabled) 
          ControlPainterImpl::paint_opener_triangle_closed(canvas, rect, Button3DDarkColor);
        else 
          ControlPainterImpl::paint_opener_triangle_closed(canvas, rect, Color::Black);
      } break;
      
    case OpenerTriangleOpened: {
        if(state == Disabled) 
          ControlPainterImpl::paint_opener_triangle_opened(canvas, rect, Button3DDarkColor);
        else 
          ControlPainterImpl::paint_opener_triangle_opened(canvas, rect, Color::Black);
      } break;
      
    case NavigationBack:
    case NavigationForward: {
        Point center = rect.center();
        rect.width = rect.height = std::min(rect.width, rect.height);
        rect.x = center.x - rect.width/2;
        rect.y = center.y - rect.height/2;
        
        ControlPainterImpl::paint_frame(canvas, rect, state == PressedHovered, state != Disabled, (state == Hovered || state == PressedHovered) ? ButtonHoverColor : ButtonColor);
        
        if(state == PressedHovered) {
          rect.x+= 0.75;
          rect.y+= 0.75;
        }
        
        if(type == NavigationForward) {
          rect.x+= rect.width;
          rect.width = -rect.width;
        }
        
        if(state == Disabled)
          ControlPainterImpl::paint_navigation_back_arrow(canvas, rect, Button3DDarkColor, ButtonColor);
        else
          ControlPainterImpl::paint_navigation_back_arrow(canvas, rect, Color::Black, Color::White);
      } break;
    
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        if(state != Pressed && state != PressedHovered) {
          rect.grow(0, -1.5);
        }
        
        RectangleF inner = rect;
        inner.y+=      1.5f;
        inner.height-= 1.5f;
        
        float dxleft;
        float dxright;
        if(state == Pressed || state == PressedHovered) {
          dxleft = 1.5f;
          dxright = 1.5f;
        }
        else {
          dxleft  = (type == TabHeadAbuttingLeft  || type == TabHeadAbuttingLeftRight) ? 0.75f : 1.5f;
          dxright = (type == TabHeadAbuttingRight || type == TabHeadAbuttingLeftRight) ? 0.75f : 1.5f;
        }
        
        inner.x+= dxleft;
        inner.width-= dxleft + dxright;
        
        Color old_color = canvas.get_color();
        ControlPainterImpl::paint_edge(canvas, rect, inner, Button3DLightColor, Button3DDarkColor);
        if(state == Hovered)
          canvas.set_color(ButtonHoverColor);
        else
          canvas.set_color(ButtonColor);
        
        inner.add_rect_path(canvas);
        canvas.fill();
        
        canvas.set_color(old_color);
      } break;
    
    case TabHeadBackground: {
        rect.y+= rect.height - 1.5f;
        rect.height = 1.5f;
        
        RectangleF inner = rect;
        inner.y+= inner.height;
        inner.height = 0.0f;
        inner.x+= 1.5f;
        inner.width-= 3.0f;
        ControlPainterImpl::paint_edge(canvas, rect, inner, Button3DLightColor, Button3DDarkColor);
      } break;
    
    case TabBodyBackground: {
        RectangleF inner = rect;
        inner.x+= 1.5f;
        inner.width-= 3.0f;
        inner.height-= 1.5f;
        
        Color old_color = canvas.get_color();
        ControlPainterImpl::paint_edge(canvas, rect, inner, Button3DLightColor, Button3DDarkColor);
        
        canvas.set_color(ButtonColor);
        inner.add_rect_path(canvas);
        canvas.fill();
        
        canvas.set_color(old_color);
      } break;
  }
  
  canvas.restore();
}

SharedPtr<BoxAnimation> ControlPainter::control_transition(
  FrontEndReference  widget_id,
  Canvas            &canvas,
  ContainerType      type1,
  ContainerType      type2,
  ControlState       state1,
  ControlState       state2,
  RectangleF         rect
) {
  return nullptr;
}

Vector2F ControlPainter::container_content_offset(
  ControlContext &control, 
  ContainerType   type,
  ControlState    state
) {
  switch(type) {
    case GenericButton:
    case PushButton:
    case DefaultPushButton: {
        if(state == PressedHovered) 
          return {0.75f, 0.75f};
      } break;
    
    case PaletteButton:
    case AddressBandGoButton: {
        if(state == PressedHovered) 
          return {0.75f, 0.0f};
      } break;
    
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead: {
        if(state == Pressed || state == PressedHovered)
          return {0.0f, -1.5f};
      } break;
    
    default: break;
  }
  
  return {0.0f, 0.0f};
}

bool ControlPainter::container_hover_repaint(ControlContext &control, ContainerType type) {
  switch(type) {
    case DefaultPushButton:
    case GenericButton:
    case NavigationBack:
    case NavigationForward:
    case PaletteButton:
    case PushButton:
    case SliderHorzThumb:
    case TabHeadAbuttingRight:
    case TabHeadAbuttingLeftRight:
    case TabHeadAbuttingLeft:
    case TabHead:
      return true;
    
    default:
      return false;
  }
}

void ControlPainter::paint_scroll_indicator(
  Canvas &canvas,
  Point   pos,
  bool    horz,
  bool    vert
) {
  cairo_pattern_t *pat;
  
  pat = cairo_pattern_create_radial(pos.x, pos.y, 11, pos.x, pos.y, 13);
  cairo_pattern_add_color_stop_rgba(pat, 0,  0.8, 0.8, 0.8, 0.7);
  cairo_pattern_add_color_stop_rgba(pat, 1,  0.5, 0.5, 0.5, 0.7);
  
  cairo_set_source(canvas.cairo(), pat);
  cairo_pattern_destroy(pat);
  
  canvas.arc(pos, 13, 0, 2 * M_PI, false);
  canvas.arc(pos, 11, 0, 2 * M_PI, true);
  canvas.fill();
  
  
  pat = cairo_pattern_create_radial(pos.x - 5, pos.y - 5, 0, pos.x, pos.y, 11);
  cairo_pattern_add_color_stop_rgba(pat, 0,    1,   1,   1,   0.7);
  cairo_pattern_add_color_stop_rgba(pat, 0.5,  1,   1,   1,   0.7);
  cairo_pattern_add_color_stop_rgba(pat, 1,    0.8, 0.8, 0.8, 0.7);
  
  cairo_set_source(canvas.cairo(), pat);
  cairo_pattern_destroy(pat);
  
  canvas.arc(pos, 11, 0, 2 * M_PI, false);
  canvas.fill();
  
  
  canvas.arc(pos, 3, 0, 2 * M_PI, false);
  canvas.close_path();
  
  if(horz) {
    canvas.move_to(pos.x + 10, pos.y);
    canvas.rel_line_to(-4, -3);
    canvas.rel_line_to(0, 6);
    canvas.close_path();
    
    canvas.move_to(pos.x - 10, pos.y);
    canvas.rel_line_to(4, -3);
    canvas.rel_line_to(0, 6);
    canvas.close_path();
  }
  
  if(vert) {
    canvas.move_to(pos.x, pos.y + 10);
    canvas.rel_line_to(-3, -4);
    canvas.rel_line_to(6, 0);
    canvas.close_path();
    
    canvas.move_to(pos.x, pos.y - 10);
    canvas.rel_line_to(-3, 4);
    canvas.rel_line_to(6, 0);
    canvas.close_path();
  }
  
  canvas.set_color(Color::from_rgb24(0x303030));
  canvas.fill();
}

void ControlPainter::system_font_style(ControlContext &control, Style *style) {
}

Color ControlPainter::selection_color(ControlContext &control) {
  return Color::Black;
}

void ControlPainter::paint_scrollbar_part(
  ControlContext     &control, 
  Canvas             &canvas,
  ScrollbarPart       part,
  ScrollbarDirection  dir,
  ControlState        state,
  RectangleF          rect
) {
  rect.pixel_align(canvas, false);
  if(rect.width <= 0 || rect.height <= 0)
    return;
    
  Color c = canvas.get_color();
  
  switch(part) {
    case ScrollbarNowhere:
      return;
      
    case ScrollbarLowerRange:
    case ScrollbarUpperRange: {
        canvas.set_color(ButtonColor);
        rect.add_rect_path(canvas);
        canvas.fill();
        
        canvas.set_color(c);
      } return;
      
    case ScrollbarSizeGrip: {
        canvas.save();
        
        Point p1, p2, p3;
        
        if(!canvas.glass_background) {
          rect.add_rect_path(canvas);
          canvas.set_color(ButtonColor);
          canvas.fill();
        }
        
        p1.x = rect.x + 4 * rect.width / 10;
        p1.y = rect.y + 9 * rect.height / 10;
        
        p2.x = rect.x + 9 * rect.width / 10;
        p2.y = rect.y + 9 * rect.height / 10;
        
        p3.x = rect.x + 9 * rect.width / 10;
        p3.y = rect.y + 4 * rect.height / 10;
        
        p1 = canvas.align_point(p1, true);
        p2 = canvas.align_point(p2, true);
        p3 = canvas.align_point(p3, true);
        
        if(canvas.glass_background) {
          canvas.move_to(p1);
          canvas.line_to(p2);
          canvas.line_to(p3);
          canvas.set_color(ButtonColor);
          canvas.fill();
        }
        
        canvas.move_to(p1);
        canvas.line_to(p2);
        canvas.line_to(p3);
        canvas.set_color(Button3DDarkColor);
        canvas.hair_stroke();
        
        canvas.move_to(p1);
        canvas.line_to(p3);
        canvas.set_color(Color::White);
        canvas.hair_stroke();
        
        canvas.set_color(c);
        canvas.restore();
      } return;
      
    case ScrollbarUpLeft:
    case ScrollbarDownRight:
    case ScrollbarThumb: {
        draw_container(control, canvas, PushButton, state, rect);
      } break;
  }
  
  Point mp = rect.center() + container_content_offset(control, PushButton, state);
  
  canvas.set_color(Color::Black);
  
  if(dir == ScrollbarHorizontal) {
    if(part == ScrollbarUpLeft) {
      mp.x += rect.width / 6;
      canvas.move_to(mp.x - rect.width / 4, mp.y);
      canvas.line_to(mp.x, mp.y - rect.height / 4);
      canvas.line_to(mp.x, mp.y + rect.height / 4);
      canvas.fill();
    }
    else if(part == ScrollbarDownRight) {
      mp.x -= rect.width / 6;
      canvas.move_to(mp.x + rect.width / 4, mp.y);
      canvas.line_to(mp.x, mp.y - rect.height / 4);
      canvas.line_to(mp.x, mp.y + rect.height / 4);
      canvas.fill();
    }
  }
  else {
    if(part == ScrollbarUpLeft) {
      mp.y += rect.width / 6;
      canvas.move_to(mp.x, mp.y - rect.height / 4);
      canvas.line_to(mp.x + rect.width / 4, mp.y);
      canvas.line_to(mp.x - rect.width / 4, mp.y);
      canvas.fill();
    }
    else if(part == ScrollbarDownRight) {
      mp.y -= rect.width / 6;
      canvas.move_to(mp.x, mp.y + rect.height / 4);
      canvas.line_to(mp.x + rect.width / 4, mp.y);
      canvas.line_to(mp.x - rect.width / 4, mp.y);
      canvas.fill();
    }
  }
  
  canvas.set_color(c);
}

void ControlPainter::scrollbar_part_pos(
  float  y,
  float  scrollbar_height,
  float  track_pos,
  float  rel_page_size,
  float *lower_y,
  float *thumb_y,
  float *upper_y,
  float *down_y
) {
  float b = scrollbar_width();
  
  if(scrollbar_height < 2 * b) {
    *lower_y = *thumb_y = *upper_y = *down_y = y + scrollbar_height / 2;
    return;
  }
  
  *lower_y = y + b;
  *down_y  = y + scrollbar_height - b;
  
  if(rel_page_size < 0 || rel_page_size >= 1) {
    *thumb_y = *upper_y = *lower_y;
    return;
  }
  
  float t;
  if(rel_page_size == 0) {
    t = b;
  }
  else {
    t = (scrollbar_height - 2 * b) * rel_page_size;
    if(t < b / 8)
      t = b;
  }
  
  if(scrollbar_height < 2 * b + t) {
    *thumb_y = *upper_y = *lower_y;
  }
  else {
    *thumb_y = y + b + (scrollbar_height - t - 2 * b) * track_pos;
    *upper_y = *thumb_y + t;
  }
}

ScrollbarPart ControlPainter::mouse_to_scrollbar_part(
  float rel_mouse,
  float scrollbar_height,
  float track_pos,
  float rel_page_size
) {
  if(rel_mouse < 0)
    return ScrollbarNowhere;
    
  if(rel_mouse > scrollbar_height)
    return ScrollbarSizeGrip;
    
  float lower, thumb, upper, down;
  scrollbar_part_pos(0, scrollbar_height, track_pos, rel_page_size,
                     &lower, &thumb, &upper, &down);
                     
  if(rel_mouse <= lower)
    return ScrollbarUpLeft;
    
  if(rel_mouse < thumb)
    return ScrollbarLowerRange;
    
  if(rel_mouse <= upper)
    return ScrollbarThumb;
    
  if(rel_mouse < down)
    return ScrollbarUpperRange;
    
  return ScrollbarDownRight;
}

void ControlPainter::paint_scrollbar(
  ControlContext     &control, 
  Canvas             &canvas,
  float               track_pos,
  float               rel_page_size,
  ScrollbarDirection  dir,
  ScrollbarPart       mouseover_part,
  ControlState        state,
  const RectangleF   &rect
) {
  float lower, thumb, upper, down;
  
  ControlState upleft_state    = Normal;
  ControlState lower_state     = Normal;
  ControlState thumb_state     = Normal;
  ControlState upper_state     = Normal;
  ControlState downright_state = Normal;
  
  if(state == Disabled) {
    upleft_state = lower_state = thumb_state = upper_state = downright_state = Disabled;
  }
  else if(mouseover_part != ScrollbarSizeGrip
          && mouseover_part != ScrollbarNowhere) {
    upleft_state = lower_state = thumb_state = upper_state = downright_state = Hovered;
    
    if(state != Pressed)
      state = Hovered;//Hot;
      
    switch(mouseover_part) {
      case ScrollbarUpLeft:     upleft_state    = state; break;
      case ScrollbarLowerRange: lower_state     = state; break;
      case ScrollbarThumb:      thumb_state     = state; break;
      case ScrollbarUpperRange: upper_state     = state; break;
      case ScrollbarDownRight:  downright_state = state; break;
      default: ;
    }
  }
  
  if(dir == ScrollbarHorizontal) {
    scrollbar_part_pos(0, rect.width, track_pos, rel_page_size,
                       &lower, &thumb, &upper, &down);
                       
    paint_scrollbar_part(control, canvas, ScrollbarUpLeft, dir, upleft_state,
                         {rect.x, rect.y, lower, rect.height});
                         
    paint_scrollbar_part(control, canvas, ScrollbarLowerRange, dir, lower_state,
                         {rect.x + lower, rect.y, thumb - lower, rect.height});
                         
    paint_scrollbar_part(control, canvas, ScrollbarThumb, dir, thumb_state,
                         {rect.x + thumb, rect.y, upper - thumb, rect.height});
                         
    paint_scrollbar_part(control, canvas, ScrollbarUpperRange, dir, upper_state,
                         {rect.x + upper, rect.y, down - upper, rect.height});
                         
    paint_scrollbar_part(control, canvas, ScrollbarDownRight, dir, downright_state,
                         {rect.x + down, rect.y, rect.width - down, rect.height});
  }
  else {
    scrollbar_part_pos(0, rect.height, track_pos, rel_page_size,
                       &lower, &thumb, &upper, &down);
                       
    paint_scrollbar_part(control, canvas, ScrollbarUpLeft, dir, upleft_state,
                         {rect.x, rect.y, rect.width, lower});
                         
    paint_scrollbar_part(control, canvas, ScrollbarLowerRange, dir, lower_state,
                         {rect.x, rect.y + lower, rect.width, thumb - lower});
                         
    paint_scrollbar_part(control, canvas, ScrollbarThumb, dir, thumb_state,
                         {rect.x, rect.y + thumb, rect.width, upper - thumb});
                         
    paint_scrollbar_part(control, canvas, ScrollbarUpperRange, dir, upper_state,
                         {rect.x, rect.y + upper, rect.width, down - upper});
                         
    paint_scrollbar_part(control, canvas, ScrollbarDownRight, dir, downright_state,
                         {rect.x, rect.y + down, rect.width, rect.height - down});
  }
}

//} ... class ControlPainter

//{ class ControlPainterImpl ...

void ControlPainterImpl::paint_edge(
  Canvas          &canvas,
  const RectangleF &outer_rect,
  const RectangleF &inner_rect,
  Color           top_left_color,
  Color           bottom_right_color
) {
  Color c = canvas.get_color();
  
  bool has_top_left = false;
  if(outer_rect.top() != inner_rect.top()) {
    has_top_left = true;
    canvas.move_to(outer_rect.top_left());
    canvas.line_to(outer_rect.top_right());
    canvas.line_to(inner_rect.top_right());
    canvas.line_to(inner_rect.top_left());
    canvas.close_path();
  }
  if(outer_rect.left() != inner_rect.left()) {
    has_top_left = true;
    canvas.move_to(outer_rect.top_left());
    canvas.line_to(inner_rect.top_left());
    canvas.line_to(inner_rect.bottom_left());
    canvas.line_to(outer_rect.bottom_left());
    canvas.close_path();
  }
  if(has_top_left) {
    canvas.set_color(top_left_color);
    canvas.fill();
  }
  
  bool has_bottom_right = false;
  if(outer_rect.bottom() != inner_rect.bottom()) {
    has_bottom_right = true;
    canvas.move_to(outer_rect.bottom_left());
    canvas.line_to(inner_rect.bottom_left());
    canvas.line_to(inner_rect.bottom_right());
    canvas.line_to(outer_rect.bottom_right());
    canvas.close_path();
  }
  if(outer_rect.right() != inner_rect.right()) {
    has_bottom_right = true;
    canvas.move_to(outer_rect.top_right());
    canvas.line_to(outer_rect.bottom_right());
    canvas.line_to(inner_rect.bottom_right());
    canvas.line_to(inner_rect.top_right());
    canvas.close_path();
  }
  if(has_bottom_right) {
    canvas.set_color(bottom_right_color);
    canvas.fill();
  }
  
  canvas.set_color(c);
}

void ControlPainterImpl::paint_frame(
  Canvas           &canvas,
  const RectangleF &rect,
  bool              sunken,
  bool              enabled,
  Color             background_color
) {
  Color c1, c3, c;
  
  c = canvas.get_color();
  c1 = Button3DLightColor;
  c3 = Button3DDarkColor;
  
  float d = 1.5f;
  if(enabled) {
    if(sunken) 
      swap(c1, c3);
  }
  else {
    d = 0.75f;
    c1 = Button3DDarkColor;
  }
  
  RectangleF inner = rect;
  inner.grow(-d);
  
  paint_edge(canvas, rect, inner, c1, c3);
  
  if(background_color) {
    inner.add_rect_path(canvas);
    canvas.set_color(background_color);
    canvas.fill();
  }
  
  canvas.set_color(c);
}

void ControlPainterImpl::paint_popup_panel(Canvas &canvas, RectangleF rect) {
  Color c = canvas.get_color();
  rect.add_rect_path(canvas, false);
  canvas.set_color(ButtonColor);
  canvas.fill_preserve();
  rect.grow(-0.75, -0.75);
  rect.add_rect_path(canvas, true);
  canvas.set_color(Color::Black, 0.5);
  canvas.fill();
  canvas.set_color(c);
}

void ControlPainterImpl::paint_checkbox_mark(Canvas &canvas, const RectangleF &rect, Color color) {
  Color c = canvas.get_color();
  canvas.move_to(rect.x +     rect.width / 4, rect.y +     rect.height / 2);
  canvas.line_to(rect.x +     rect.width / 3, rect.y + 3 * rect.height / 4);
  canvas.line_to(rect.x + 3 * rect.width / 4, rect.y +     rect.height / 4);
  
  cairo_set_line_width(canvas.cairo(), 2.0 * 0.75);
  cairo_set_line_cap(canvas.cairo(), CAIRO_LINE_CAP_SQUARE);
  cairo_set_line_join(canvas.cairo(), CAIRO_LINE_JOIN_MITER);
  
  canvas.set_color(color);
  canvas.stroke();
  canvas.set_color(c);
}

void ControlPainterImpl::paint_checkbox_indeterminate_mark(Canvas &canvas, const RectangleF &rect, Color color) {
  Color c = canvas.get_color();
  canvas.move_to(rect.x +     rect.width / 4, rect.y +     rect.height / 4);
  canvas.line_to(rect.x + 3 * rect.width / 4, rect.y +     rect.height / 4);
  canvas.line_to(rect.x + 3 * rect.width / 4, rect.y + 3 * rect.height / 4);
  canvas.line_to(rect.x +     rect.width / 4, rect.y + 3 * rect.height / 4);
  
  canvas.set_color(color);
  canvas.fill();
  canvas.set_color(c);
}

void ControlPainterImpl::paint_radio_button_background(Canvas &canvas, const RectangleF &rect, Color inner_color) {
  Color old_color = canvas.get_color();
  
  canvas.move_to(rect.x + rect.width / 2, rect.y);
  canvas.line_to(rect.x + rect.width,     rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2, rect.y + rect.height);
  canvas.line_to(rect.x,                  rect.y + rect.height / 2);
  
  canvas.set_color(inner_color);
  canvas.fill();
  
  
  canvas.move_to(rect.x,                     rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,    rect.y + rect.height);
  canvas.line_to(rect.x + rect.width,        rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width - 0.75, rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,    rect.y + rect.height - 0.75);
  canvas.line_to(rect.x + 0.75,              rect.y + rect.height / 2);
  
  canvas.set_color(ButtonColor);
  canvas.fill();
  
  
  canvas.move_to(rect.x + 0.75,              rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,    rect.y + rect.height - 0.75);
  canvas.line_to(rect.x + rect.width - 0.75, rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width - 1.5,  rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,    rect.y + rect.height - 1.5);
  canvas.line_to(rect.x + 1.5,               rect.y + rect.height / 2);
  
  canvas.set_color(Button3DLightColor);
  canvas.fill();
  
  
  canvas.move_to(rect.x,                    rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,   rect.y);
  canvas.line_to(rect.x + rect.width,       rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width - 1.5, rect.y + rect.height / 2);
  canvas.line_to(rect.x + rect.width / 2,   rect.y + 1.5);
  canvas.line_to(rect.x + 1.5,              rect.y + rect.height / 2);
  
  canvas.set_color(Button3DDarkColor);
  canvas.fill();
  
  canvas.set_color(old_color);
}

void ControlPainterImpl::paint_radio_button_mark(Canvas &canvas, const RectangleF &rect, Color color) {
  Color old_color = canvas.get_color();

  canvas.move_to(rect.x +     rect.width / 2, rect.y +     rect.height / 4);
  canvas.line_to(rect.x + 3 * rect.width / 4, rect.y +     rect.height / 2);
  canvas.line_to(rect.x +     rect.width / 2, rect.y + 3 * rect.height / 4);
  canvas.line_to(rect.x +     rect.width / 4, rect.y +     rect.height / 2);
  
  canvas.set_color(color);
  canvas.fill();
  
  canvas.set_color(old_color);
}

void ControlPainterImpl::paint_opener_triangle_closed(Canvas &canvas, const RectangleF &rect, Color color) {
  Color old_col = canvas.get_color();
  
  canvas.move_to(rect.x + rect.width * 0.3f, rect.y + rect.height * 0.3f);
  canvas.line_to(rect.x + rect.width * 0.6f, rect.y + rect.height * 0.5f);
  canvas.line_to(rect.x + rect.width * 0.3f, rect.y + rect.height * 0.7f);
  
  canvas.set_color(color);
  canvas.fill();
  canvas.set_color(old_col);
}

void ControlPainterImpl::paint_opener_triangle_opened(Canvas &canvas, const RectangleF &rect, Color color) {
  Color old_col = canvas.get_color();
  
  canvas.move_to(rect.x + rect.width * 0.6f, rect.y + rect.height * 0.3f);
  canvas.line_to(rect.x + rect.width * 0.6f, rect.y + rect.height * 0.6f);
  canvas.line_to(rect.x + rect.width * 0.3f, rect.y + rect.height * 0.6f);
  
  canvas.set_color(color);
  canvas.fill();
  canvas.set_color(old_col);
}

void ControlPainterImpl::paint_navigation_back_arrow(Canvas &canvas, const RectangleF &rect, Color stroke_col, Color fill_col) {
  Color old_col = canvas.get_color();
  
  canvas.move_to(rect.x + rect.width/4, rect.y + rect.height/2);
  canvas.rel_line_to( rect.width/4,                  rect.height/4);
  canvas.rel_line_to( rect.width/6,                  0);
  canvas.rel_line_to(-rect.width/4 + rect.width/12, -rect.height/4 + rect.height/12);
  canvas.rel_line_to( rect.width/4,                  0);
  canvas.rel_line_to(0,                             -rect.height/6);
  canvas.rel_line_to(-rect.width/4,                  0);
  canvas.rel_line_to( rect.width/4 - rect.width/12, -rect.height/4 + rect.height/12);
  canvas.rel_line_to(-rect.width/6,                  0);
  canvas.rel_line_to(-rect.width/4,                  rect.height/4);
  canvas.close_path();
  
  canvas.set_color(fill_col);
  canvas.fill_preserve();
  canvas.set_color(stroke_col);
  canvas.stroke();
  
  canvas.set_color(old_col);
}

//} ... class ControlPainterImpl
