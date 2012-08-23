#include <gui/control-painter.h>

#include <cmath>

#include <boxes/box.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <graphics/context.h>

using namespace richmath;

//{ class ControlPainter ...

ControlPainter ControlPainter::generic_painter;
ControlPainter *ControlPainter::std = &generic_painter;

void ControlPainter::calc_container_size(
  Canvas        *canvas,
  ContainerType  type,
  BoxSize       *extents // in/out
) {
  switch(type) {
    case NoContainerType:
    case FramelessButton: break;
    
    case GenericButton:
    case PushButton:
    case PaletteButton:
    case TooltipWindow: 
    case ListViewItem:
    case ListViewItemSelected: {
        if(extents->ascent < canvas->get_font_size() * 0.75f)
          extents->ascent = canvas->get_font_size() * 0.75f;// - extents->ascent;
          
        if(extents->descent < canvas->get_font_size() * 0.25f)
          extents->descent = canvas->get_font_size() * 0.25f;// - extents->descent;
          
        extents->width +=   4.5;
        extents->ascent +=  2.25;
        extents->descent += 2.25;
      } break;
      
    case DefaultPushButton: {
        if(extents->ascent < canvas->get_font_size() * 0.75f)
          extents->ascent = canvas->get_font_size() * 0.75f;// - extents->ascent;
          
        if(extents->descent < canvas->get_font_size() * 0.25f)
          extents->descent = canvas->get_font_size() * 0.25f;// - extents->descent;
          
        extents->width +=   6.0;
        extents->ascent +=  3.0;
        extents->descent += 3.0;
      } break;
      
    case InputField: {
        extents->width +=   6.0;
        extents->ascent +=  3.0;
        extents->descent += 3.0;
      } break;
      
    case SliderHorzChannel: {
        extents->ascent  = 3.0;
        extents->descent = 1.0;
      } break;
      
    case SliderHorzThumb: {
        extents->width = extents->height() / 2;
      } break;
      
    case ProgressIndicatorBackground: {
        extents->ascent *= 0.5;
        extents->descent *= 0.5;
        extents->width = extents->height() * 15;
      } break;
      
    case ProgressIndicatorBar: {
//      extents->ascent *= 0.5;
//      extents->descent*= 0.5;
//      extents->width = extents->height() * 15;

        extents->ascent -= 1.5;
        extents->descent -= 1.5;
        extents->width  -= 3.0;
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
  }
}


int ControlPainter::control_font_color(ContainerType type, ControlState state) {
  if(is_very_transparent(type, state))
    return -1;
    
  if(type == ListViewItemSelected)
    return 0xFFFFFF;
    
  return 0x000000;
}

bool ControlPainter::is_very_transparent(ContainerType type, ControlState state) {
  return type == NoContainerType || type == FramelessButton;
}

static void paint_frame(
  Canvas *canvas,
  float   x,
  float   y,
  float   width,
  float   height,
  bool    sunken,
  int     background_color = 0xDCDCDC
) {
  int c1, c3, c;
  
  c = canvas->get_color();
  c1 = 0xF0F0F0;//0xFFFFFF;
  //c2 = 0xDCDCDC;
  c3 = 0x787878; // 0xB4B4B4
  
  if(sunken) {
    int tmp = c1;
    c1 = c3;
    c3 = tmp;
  }
  
  static const float d = 3 / 2.f;
  float x2 = x + width;
  float y2 = y + height;
  canvas->align_point(&x,  &y,  false);
  canvas->align_point(&x2, &y2, false);
  
  canvas->move_to(x,      y);
  canvas->line_to(x2,     y);
  canvas->line_to(x2 - d, y + d);
  canvas->line_to(x + d,  y + d);
  canvas->line_to(x + d,  y2 - d);
  canvas->line_to(x,      y2);
  canvas->close_path();
  
  canvas->set_color(c1);
  canvas->fill();
  
  canvas->move_to(x2,     y2);
  canvas->line_to(x2,     y);
  canvas->line_to(x2 - d, y + d);
  canvas->line_to(x2 - d, y2 - d);
  canvas->line_to(x + d,  y2 - d);
  canvas->line_to(x,      y2);
  canvas->close_path();
  
  canvas->set_color(c3);
  canvas->fill();
  
  canvas->move_to(x + d,  y + d);
  canvas->line_to(x2 - d, y + d);
  canvas->line_to(x2 - d, y2 - d);
  canvas->line_to(x + d, y2 - d);
  canvas->close_path();
  
  canvas->set_color(background_color);
  canvas->fill();
  
  canvas->set_color(c);
}

void ControlPainter::draw_container(
  Canvas        *canvas,
  ContainerType  type,
  ControlState   state,
  float          x,
  float          y,
  float          width,
  float          height
) {
  canvas->save();
  
  if(canvas->pixel_device) {
    canvas->user_to_device_dist(&width, &height);
    width  = floor(width  + 0.5);
    height = floor(height + 0.5);
    canvas->device_to_user_dist(&width, &height);
  }
  
  switch(type) {
    case NoContainerType:
    case FramelessButton: break;
    
    case GenericButton:
    case PushButton:
    case PaletteButton:
      paint_frame(canvas, x, y, width, height, state == Pressed);
      break;
      
    case DefaultPushButton: {
    
        paint_frame(canvas, x, y, width, height, state == Pressed);
        
        float x2 = x + width;
        float y2 = y + height;
        canvas->align_point(&x,  &y,  false);
        canvas->align_point(&x2, &y2, false);
        
        canvas->move_to(x,  y);
        canvas->line_to(x,  y2);
        canvas->line_to(x2, y2);
        canvas->line_to(x2, y);
        canvas->close_path();
        
        x -= 0.75;
        y -= 0.75;
        x2 += 0.75;
        y2 += 0.75;
        canvas->move_to(x,  y);
        canvas->line_to(x2, y);
        canvas->line_to(x2, y2);
        canvas->line_to(x,  y2);
        
        int c = canvas->get_color();
        canvas->set_color(0x000000);
        canvas->fill();
        canvas->set_color(c);
      } break;
      
    case InputField:
      paint_frame(canvas, x, y, width, height, true, 0xFFFFFF);
      break;
      
    case TooltipWindow:
      paint_frame(canvas, x, y, width, height, false, 0xFFF4C1);
      break;
      
    case ListViewItem: {
        int c = canvas->get_color();
        canvas->set_color(0xffffff);
        canvas->pixrect(x, y, x + width, y + height, false);
        canvas->fill();
        canvas->set_color(c);
      } break;
      
    case ListViewItemSelected: {
        int c = canvas->get_color();
        canvas->set_color(0x0099ff);
        canvas->pixrect(x, y, x + width, y + height, false);
        canvas->fill();
        canvas->set_color(c);
      } break;
      
    case SliderHorzChannel:
      paint_frame(canvas, x, y, width, height, true);
      break;
      
    case SliderHorzThumb:
      paint_frame(canvas, x, y, width, height, false);
      break;
      
    case ProgressIndicatorBackground:
      paint_frame(canvas, x, y, width, height, true);
      break;
      
    case ProgressIndicatorBar: {
        x += 3 / 2.f;
        y += 3 / 2.f;
        width -= 3;
        height -= 3;
        
        if(width > 0) {
          if(state == Normal)
            paint_frame(canvas, x, y, width, height, false);
          else if(state == Hot)
            paint_frame(canvas, x + width / 4, y, width / 2, height, false);
        }
      } break;
      
    case CheckboxUnchecked:
      if(state == Disabled)
        paint_frame(canvas, x, y, width, height, true);
      else
        paint_frame(canvas, x, y, width, height, true, 0xFFFFFF);
        
      break;
      
    case CheckboxChecked: {
        if(state == Disabled)
          paint_frame(canvas, x, y, width, height, true);
        else
          paint_frame(canvas, x, y, width, height, true, 0xFFFFFF);
          
        canvas->save();
        {
          canvas->move_to(x +     width / 4, y +     height / 2);
          canvas->line_to(x +     width / 3, y + 3 * height / 4);
          canvas->line_to(x + 3 * width / 4, y +     height / 4);
          
          cairo_set_line_width(canvas->cairo(), 2.0 * 0.75);
          cairo_set_line_cap(canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
          cairo_set_line_join(canvas->cairo(), CAIRO_LINE_JOIN_MITER);
          cairo_set_source_rgb(canvas->cairo(), 0, 0, 0);
          
          canvas->stroke();
        }
        canvas->restore();
      } break;
      
    case CheckboxIndeterminate: {
        if(state == Disabled)
          paint_frame(canvas, x, y, width, height, true);
        else
          paint_frame(canvas, x, y, width, height, true, 0xFFFFFF);
          
        canvas->save();
        {
          canvas->move_to(x +     width / 4, y +     height / 4);
          canvas->line_to(x + 3 * width / 4, y +     height / 4);
          canvas->line_to(x + 3 * width / 4, y + 3 * height / 4);
          canvas->line_to(x +     width / 4, y + 3 * height / 4);
          
          cairo_set_source_rgb(canvas->cairo(), 0, 0, 0);
          canvas->fill();
        }
        canvas->restore();
      } break;
      
    case RadioButtonUnchecked:
    case RadioButtonChecked: {
        canvas->save();
        {
          int old_color = canvas->get_color();
          int inner_color = 0xFFFFFF;
          
          if(state == Disabled)
            inner_color = 0xDCDCDC;
            
          canvas->move_to(x + width / 2, y);
          canvas->line_to(x + width,   y + height / 2);
          canvas->line_to(x + width / 2, y + height);
          canvas->line_to(x,           y + height / 2);
          
          canvas->set_color(inner_color);
          canvas->fill();
          
          
          canvas->move_to(x,                y + height / 2);
          canvas->line_to(x + width / 2,      y + height);
          canvas->line_to(x + width,        y + height / 2);
          canvas->line_to(x + width - 0.75, y + height / 2);
          canvas->line_to(x + width / 2,      y + height - 0.75);
          canvas->line_to(x + 0.75,         y + height / 2);
          
          canvas->set_color(0xDCDCDC);
          canvas->fill();
          
          
          canvas->move_to(x + 0.75,         y + height / 2);
          canvas->line_to(x + width / 2,      y + height - 0.75);
          canvas->line_to(x + width - 0.75, y + height / 2);
          canvas->line_to(x + width - 1.5,  y + height / 2);
          canvas->line_to(x + width / 2,      y + height - 1.5);
          canvas->line_to(x + 1.5,          y + height / 2);
          
          canvas->set_color(0xF0F0F0);
          canvas->fill();
          
          
          canvas->move_to(x,               y + height / 2);
          canvas->line_to(x + width / 2,     y);
          canvas->line_to(x + width,       y + height / 2);
          canvas->line_to(x + width - 1.5, y + height / 2);
          canvas->line_to(x + width / 2,     y + 1.5);
          canvas->line_to(x + 1.5,         y + height / 2);
          
          canvas->set_color(0x787878);
          canvas->fill();
          
          
          if(type == RadioButtonChecked) {
            canvas->move_to(x +     width / 2, y +     height / 4);
            canvas->line_to(x + 3 * width / 4, y +     height / 2);
            canvas->line_to(x +     width / 2, y + 3 * height / 4);
            canvas->line_to(x +     width / 4, y +     height / 2);
            
            canvas->set_color(0x000000);
            canvas->fill();
          }
          
          canvas->set_color(old_color);
        }
        canvas->restore();
      } break;
  }
  
  canvas->restore();
}

SharedPtr<BoxAnimation> ControlPainter::control_transition(
  int                          widget_id,
  Canvas                      *canvas,
  ContainerType                type1,
  ContainerType                type2,
  ControlState                 state1,
  ControlState                 state2,
  float                        x,
  float                        y,
  float                        width,
  float                        height
) {
  return 0;
}

void ControlPainter::container_content_move(
  ContainerType  type,
  ControlState   state,
  float         *x,
  float         *y
) {
  switch(type) {
    case GenericButton:
    case PaletteButton:
    case PushButton:
    case DefaultPushButton: {
        if(state == PressedHovered) {
          *x += 0.75f;
          *y += 0.75f;
        }
      } break;
      
    default: ;
  }
}

bool ControlPainter::container_hover_repaint(ContainerType type) {
  return false;
}

void ControlPainter::paint_scroll_indicator(
  Canvas *canvas,
  float   x,
  float   y,
  bool    horz,
  bool    vert
) {
  cairo_pattern_t *pat;
  
  pat = cairo_pattern_create_radial(x, y, 11, x, y, 13);
  cairo_pattern_add_color_stop_rgba(pat, 0,  0.8, 0.8, 0.8, 0.7);
  cairo_pattern_add_color_stop_rgba(pat, 1,  0.5, 0.5, 0.5, 0.7);
  
  cairo_set_source(canvas->cairo(), pat);
  cairo_pattern_destroy(pat);
  
  canvas->arc(x, y, 13, 0, 2 * M_PI, false);
  canvas->arc(x, y, 11, 0, 2 * M_PI, true);
  canvas->fill();
  
  
  pat = cairo_pattern_create_radial(x - 5, y - 5, 0, x, y, 11);
  cairo_pattern_add_color_stop_rgba(pat, 0,    1,   1,   1,   0.7);
  cairo_pattern_add_color_stop_rgba(pat, 0.5,  1,   1,   1,   0.7);
  cairo_pattern_add_color_stop_rgba(pat, 1,    0.8, 0.8, 0.8, 0.7);
  
  cairo_set_source(canvas->cairo(), pat);
  cairo_pattern_destroy(pat);
  
  canvas->arc(x, y, 11, 0, 2 * M_PI, false);
  canvas->fill();
  
  
  canvas->arc(x, y, 3, 0, 2 * M_PI, false);
  canvas->close_path();
  
  if(horz) {
    canvas->move_to(x + 10, y);
    canvas->rel_line_to(-4, -3);
    canvas->rel_line_to(0, 6);
    canvas->close_path();
    
    canvas->move_to(x - 10, y);
    canvas->rel_line_to(4, -3);
    canvas->rel_line_to(0, 6);
    canvas->close_path();
  }
  
  if(vert) {
    canvas->move_to(x, y + 10);
    canvas->rel_line_to(-3, -4);
    canvas->rel_line_to(6, 0);
    canvas->close_path();
    
    canvas->move_to(x, y - 10);
    canvas->rel_line_to(-3, 4);
    canvas->rel_line_to(6, 0);
    canvas->close_path();
  }
  
  canvas->set_color(0x303030);
  canvas->fill();
}

void ControlPainter::system_font_style(Style *style) {
}

int ControlPainter::selection_color() {
  return 0x000000;
}

void ControlPainter::paint_scrollbar_part(
  Canvas             *canvas,
  ScrollbarPart       part,
  ScrollbarDirection  dir,
  ControlState        state,
  float               x,
  float               y,
  float               width,
  float               height
) {
  if(width <= 0 || height <= 0)
    return;
    
  int c = canvas->get_color();
  
  switch(part) {
    case ScrollbarNowhere:
      return;
      
    case ScrollbarLowerRange:
    case ScrollbarUpperRange: {
        canvas->set_color(0xDCDCDC);
        canvas->pixrect(x, y, x + width, y + height, false);
        canvas->fill();
        
        canvas->set_color(c);
      } return;
      
    case ScrollbarSizeGrip: {
        canvas->save();
        
        float x1, y1;
        float x2, y2;
        float x3, y3;
        
        if(!canvas->glass_background) {
          canvas->pixrect(x, y, x + width, y + height, false);
          canvas->set_color(0xDCDCDC);
          canvas->fill();
        }
        
        x1 = x + 4 * width / 10;
        y1 = y + 9 * height / 10;
        
        x2 = x + 9 * width / 10;
        y2 = y + 9 * height / 10;
        
        x3 = x + 9 * width / 10;
        y3 = y + 4 * height / 10;
        
        canvas->align_point(&x1, &y1, true);
        canvas->align_point(&x2, &y2, true);
        canvas->align_point(&x3, &y3, true);
        
        if(canvas->glass_background) {
          canvas->move_to(x1, y1);
          canvas->line_to(x2, y2);
          canvas->line_to(x3, y3);
          canvas->set_color(0xDCDCDC);
          canvas->fill();
        }
        
        canvas->move_to(x1, y1);
        canvas->line_to(x2, y2);
        canvas->line_to(x3, y3);
        canvas->set_color(0x787878);
        canvas->hair_stroke();
        
        canvas->move_to(x1, y1);
        canvas->line_to(x3, y3);
        canvas->set_color(0xFFFFFF);
        canvas->hair_stroke();
        
        canvas->set_color(c);
        canvas->restore();
      } return;
      
    case ScrollbarUpLeft:
    case ScrollbarDownRight:
    case ScrollbarThumb: {
        draw_container(canvas, PushButton, state, x, y, width, height);
      } break;
  }
  
  float mx = x + width / 2;
  float my = y + height / 2;
  
  container_content_move(PushButton, state, &mx, &my);
  
  canvas->set_color(0);
  
  if(dir == ScrollbarHorizontal) {
    if(part == ScrollbarUpLeft) {
      mx += width / 6;
      canvas->move_to(mx - width / 4, my);
      canvas->line_to(mx, my - height / 4);
      canvas->line_to(mx, my + height / 4);
      canvas->fill();
    }
    else if(part == ScrollbarDownRight) {
      mx -= width / 6;
      canvas->move_to(mx + width / 4, my);
      canvas->line_to(mx, my - height / 4);
      canvas->line_to(mx, my + height / 4);
      canvas->fill();
    }
  }
  else {
    if(part == ScrollbarUpLeft) {
      my += width / 6;
      canvas->move_to(mx, my - height / 4);
      canvas->line_to(mx + width / 4, my);
      canvas->line_to(mx - width / 4, my);
      canvas->fill();
    }
    else if(part == ScrollbarDownRight) {
      my -= width / 6;
      canvas->move_to(mx, my + height / 4);
      canvas->line_to(mx + width / 4, my);
      canvas->line_to(mx - width / 4, my);
      canvas->fill();
    }
  }
  
  canvas->set_color(c);
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
  Canvas             *canvas,
  float               track_pos,
  float               rel_page_size,
  ScrollbarDirection  dir,
  ScrollbarPart       mouseover_part,
  ControlState        state,
  float               x,
  float               y,
  float               width,
  float               height
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
    scrollbar_part_pos(0, width, track_pos, rel_page_size,
                       &lower, &thumb, &upper, &down);
                       
    paint_scrollbar_part(canvas, ScrollbarUpLeft, dir, upleft_state,
                         x, y,
                         lower, height);
                         
    paint_scrollbar_part(canvas, ScrollbarLowerRange, dir, lower_state,
                         x + lower, y,
                         thumb - lower, height);
                         
    paint_scrollbar_part(canvas, ScrollbarThumb, dir, thumb_state,
                         x + thumb, y,
                         upper - thumb, height);
                         
    paint_scrollbar_part(canvas, ScrollbarUpperRange, dir, upper_state,
                         x + upper, y,
                         down - upper, height);
                         
    paint_scrollbar_part(canvas, ScrollbarDownRight, dir, downright_state,
                         x + down, y,
                         width - down, height);
  }
  else {
    scrollbar_part_pos(0, height, track_pos, rel_page_size,
                       &lower, &thumb, &upper, &down);
                       
    paint_scrollbar_part(canvas, ScrollbarUpLeft, dir, upleft_state,
                         x, y,
                         width, lower);
                         
    paint_scrollbar_part(canvas, ScrollbarLowerRange, dir, lower_state,
                         x, y + lower,
                         width, thumb - lower);
                         
    paint_scrollbar_part(canvas, ScrollbarThumb, dir, thumb_state,
                         x, y + thumb,
                         width, upper - thumb);
                         
    paint_scrollbar_part(canvas, ScrollbarUpperRange, dir, upper_state,
                         x, y + upper,
                         width, down - upper);
                         
    paint_scrollbar_part(canvas, ScrollbarDownRight, dir, downright_state,
                         x, y + down,
                         width, height - down);
  }
}

//} ... class ControlPainter
