#include <boxes/panebox.h>
#include <boxes/mathsequence.h>

#include <graphics/context.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>
#include <cmath>


using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Bottom;
extern pmath_symbol_t richmath_System_Center;
extern pmath_symbol_t richmath_System_Left;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Right;
extern pmath_symbol_t richmath_System_Top;
extern pmath_symbol_t richmath_System_PaneBox;

namespace richmath {
  namespace strings {
    extern String Pane;
  }
  
  struct SimpleAlignment {
    float horizontal; // 0 = left .. 1 = right
    float vertical;   // 0 = bottom .. 1 = top
  };
  
  class PaneBox::Impl {
    public:
      Impl(PaneBox &self) : self{self} {}
      
      double shrink_scale(Length w, Length h, bool line_break_within);
      double grow_scale(Length w, Length h, bool line_break_within);
      
      static SimpleAlignment decode_simple_alignment(Expr alignment);
      static float decode_simple_alignment_horizontal(Expr horizontal_alignment);
      static float decode_simple_alignment_vertical(Expr vertical_alignment);
      
    private:
      PaneBox &self;
  };
}


//{ class PaneBox ...

PaneBox::PaneBox(AbstractSequence *content)
  : base(content)
{
}

PaneBox::~PaneBox() {
}

bool PaneBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_PaneBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
  
  /* now success is guaranteed */
  
  content()->load_from_object(expr[1], opts);
  
  reset_style();
  style.add_pmath(options);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

void PaneBox::reset_style() {
  style.reset(strings::Pane);
}

Expr PaneBox::to_pmath_symbol() { 
  return Symbol(richmath_System_PaneBox); 
}

Expr PaneBox::to_pmath_impl(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style.get(BaseStyleName, &s) && s == strings::Pane)
      with_inherited = false;
    
    style.emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PaneBox));
  return e;
}

void PaneBox::resize_default_baseline(Context &context) {
  Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic).resolve_scaled(context.width);
  Length h = get_own_style(ImageSizeVertical,   SymbolicSize::Automatic).resolve_scaled(-1);
  bool line_break_within = get_own_style(LineBreakWithin, true);
  
  mat.xx = 1;
  mat.xy = 0;
  mat.yx = 0;
  mat.yy = 1;
  base::resize_default_baseline(context);
  
  // TODO: do not perform automatic scaling while mouse is pressed inside content, e.g. when dragging graphics corner
  auto image_size_action = static_cast<ImageSizeActionValues>(get_own_style(ImageSizeAction, ImageSizeActionClip));
  switch(image_size_action) {
    case ImageSizeActionClip:
      break;
    
    case ImageSizeActionShrinkToFit:
    case ImageSizeActionResizeToFit: {
      double scale = Impl(*this).shrink_scale(w, h, line_break_within);
      
      if(scale < 1) {
        mat.xx = mat.yy = scale;
        base::resize_default_baseline(context);
        
        for(int retry = 3; retry > 0; --retry) {
          double next_scale = Impl(*this).shrink_scale(w, h, line_break_within);
          if(next_scale < mat.xx)
            mat.xx = mat.yy = next_scale;
          else
            break;
          
          base::resize_default_baseline(context);
        }
      }
      else if(image_size_action == ImageSizeActionResizeToFit) {
        scale = Impl(*this).grow_scale(w, h, line_break_within);
        
        if(scale > 1) {
          mat.xx = mat.yy = scale;
          base::resize_default_baseline(context);
        }
      }
    } break;
  }
  
  if(!w.is_explicit_abs())
    w = Length::Absolute(_extents.width);
    
  if(!h.is_explicit_abs())
    h = Length::Absolute(_extents.height());
  
  cx = 0;// Left aligned
  cy = 0;
  
  SimpleAlignment alignment = Impl::decode_simple_alignment(get_own_style(Alignment));
  
  _extents.width = w.explicit_abs_value();
  
  float max_cx = _extents.width - content()->extents().width;
  if(max_cx > 0) {
    cx = max_cx * alignment.horizontal;
  }
  
  float orig_ascent  = _extents.ascent;
  float orig_descent = _extents.descent;
  float new_h        = h.explicit_abs_value();
  
//  // Top:
//  _extents.ascent  = orig_ascent;
//  _extents.descent = new_h - orig_ascent;
//  
//  // Bottom:
//  _extents.ascent  = new_h - orig_descent; 
//  _extents.descent = orig_descent;
  
  _extents.ascent  = alignment.vertical * orig_ascent           + (1 - alignment.vertical) * (new_h - orig_descent);
  _extents.descent = alignment.vertical * (new_h - orig_ascent) + (1 - alignment.vertical) * orig_descent;
}

float PaneBox::allowed_content_width(const Context &context) {
  if(get_own_style(LineBreakWithin, true)) {
    Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
    
    if(w.is_explicit_rel())
      w = Length::Absolute(w.explicit_rel_value() * context.width);
      
    if(w.is_explicit_abs_positive()) {
      if(mat.xx > 0)
        return w.explicit_abs_value() / mat.xx;
        
      return w.explicit_abs_value();
    }
  }
  
  return Infinity;
}

void PaneBox::paint_content(Context &context) {
  Point pos = context.canvas().current_pos();
  
  context.canvas().save();
  {
    RectangleF rect = _extents.to_rectangle(pos);
    rect.pixel_align(context.canvas(), false, +1);
    rect.add_rect_path(context.canvas(), false);
    context.canvas().clip();
    
    context.canvas().move_to(pos);
    base::paint_content(context);
  }
  context.canvas().restore();
}

//} ... class PaneBox

//{ class PaneBox::Impl ...

double PaneBox::Impl::shrink_scale(Length w, Length h, bool line_break_within) {
  double result = 1.0f;
  
  if(line_break_within && w.is_explicit_abs_positive() && h.is_explicit_abs_positive()) {
    double scale_square = w.explicit_abs_value() * (double)h.explicit_abs_value() / ((double)self._content->extents().width * self._content->extents().height());
    if(0 < scale_square && scale_square < 1.0f)
      result = sqrt(scale_square);
  }
  else {
    if(w.is_explicit_abs_positive() && self._content->extents().width > w.explicit_abs_value()) {
      double scale = w.explicit_abs_value() / (double)self._content->extents().width;
      if(0 < scale && scale < result)
        result = scale;
    }
    if(h.is_explicit_abs_positive() && self._content->extents().height() > h.explicit_abs_value()) {
      double scale = h.explicit_abs_value() / (double)self._content->extents().height();
      if(0 < scale && scale < result)
        result = scale;
    }
  }
  
  return result;
}

double PaneBox::Impl::grow_scale(Length w, Length h, bool line_break_within) {
  double result = 1;
  if( w.is_explicit_abs() && w.explicit_abs_value() > self._content->extents().width && 
      h.is_explicit_abs() && h.explicit_abs_value() > self._content->extents().height()
  ) {
    double scale = std::min(
                     w.explicit_abs_value() / (double)self._content->extents().width, 
                     h.explicit_abs_value() / (double)self._content->extents().height());
    if(0 < scale && scale < Infinity) 
      result = scale;
  }
  else if(h == SymbolicSize::Automatic && 
      w.is_explicit_abs() && w.explicit_abs_value() > self._content->extents().width
  ) {
    double scale = w.explicit_abs_value() / (double)self._content->extents().width;
    if(0 < scale && scale < Infinity)
      result = scale;
  }
  else if(w == SymbolicSize::Automatic && 
      h.is_explicit_abs() && h.explicit_abs_value() > self._content->extents().height()
  ) {
    double scale = h.explicit_abs_value() / (double)self._content->extents().height();
    if(0 < scale && scale < Infinity)
      result = scale;
  }
  return result;
}

SimpleAlignment PaneBox::Impl::decode_simple_alignment(Expr alignment) {
  if(alignment.is_symbol()) {
    return {
      decode_simple_alignment_horizontal(alignment), 
      decode_simple_alignment_vertical(  alignment)};
  }
  else if(alignment.is_number()) {
    return { decode_simple_alignment_horizontal(alignment), 1.0f};
  }
  else if(alignment.expr_length() == 2 && alignment[0] == richmath_System_List) {
    return {
      decode_simple_alignment_horizontal(alignment[1]),
      decode_simple_alignment_vertical(  alignment[2])};
  }
  return { 0.0f, 1.0f };
}

float PaneBox::Impl::decode_simple_alignment_horizontal(Expr horizontal_alignment) {
  if(horizontal_alignment.is_symbol()) {
    if(horizontal_alignment == richmath_System_Automatic) return 0;
    if(horizontal_alignment == richmath_System_Left)      return 0;
    if(horizontal_alignment == richmath_System_Right)     return 1;
    if(horizontal_alignment == richmath_System_Center)    return 0.5f;
  }
  else if(horizontal_alignment.is_number()) {
    double val = horizontal_alignment.to_double();
    if(-1.0 <= val && val <= 1.0)
      return (float)((val - (-1.0)) / 2);
    
    if(val < -1.0)
      return 0;
    if(val > 1.0)
      return 1;
  }
  
  return -1;
}

float PaneBox::Impl::decode_simple_alignment_vertical(Expr vertical_alignment) {
  if(vertical_alignment.is_symbol()) {
    if(vertical_alignment == richmath_System_Automatic) return 1;
    if(vertical_alignment == richmath_System_Center)    return 0.5f;
    if(vertical_alignment == richmath_System_Top)       return 1;
    if(vertical_alignment == richmath_System_Bottom)    return 0;
  }
  else if(vertical_alignment.is_number()) {
    double val = vertical_alignment.to_double();
    if(-1.0 <= val && val <= 1.0)
      return (float)((val - (-1.0)) / 2);
    
    if(val < -1.0)
      return 0;
    if(val > 1.0)
      return 1;
  }
  
  return 1;
}

//} ... class PaneBox::Impl
