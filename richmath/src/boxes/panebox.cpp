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

extern pmath_symbol_t richmath_System_PaneBox;

namespace richmath {
  namespace strings {
    extern String Pane;
  }
  
  class PaneBox::Impl {
    public:
      Impl(PaneBox &self) : self{self} {}
      
      double shrink_scale(Length w, Length h, bool line_break_within);
      double grow_scale(Length w, Length h, bool line_break_within);
      
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
  if(options != PMATH_UNDEFINED) 
    style->add_pmath(options);
  
  finish_load_from_object(std::move(expr));
  return true;
}

void PaneBox::reset_style() {
  Style::reset(style, strings::Pane);
}

Expr PaneBox::to_pmath_symbol() { 
  return Symbol(richmath_System_PaneBox); 
}

Expr PaneBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s == strings::Pane)
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PaneBox));
  return e;
}

void PaneBox::resize_default_baseline(Context &context) {
  Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
  Length h = get_own_style(ImageSizeVertical,   SymbolicSize::Automatic);
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
  
  if(!w.is_explicit())
    w = Length(_extents.width);
    
  if(!h.is_explicit())
    h = Length(_extents.height());
  
  cx = 0;
  cy = 0;
  mat.y0 += _extents.ascent - h.raw_value();
  
  _extents.width = w.raw_value();
  _extents.ascent = h.raw_value();
  _extents.descent = 0;
}

float PaneBox::allowed_content_width(const Context &context) {
  if(get_own_style(LineBreakWithin, true)) {
    Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
    if(w.is_positive()) {
      if(mat.xx > 0)
        return w.raw_value() / mat.xx;
        
      return w.raw_value();
    }
  }
  
  return Infinity;
}

void PaneBox::paint_content(Context &context) {
  Point pos = context.canvas().current_pos();
  
  context.canvas().save();
  {
    context.canvas().pixrect(_extents.to_rectangle(pos), false);
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
  
  if(line_break_within && w.is_positive() && h.is_positive()) {
    double scale_square = w.raw_value() * (double)h.raw_value() / ((double)self._content->extents().width * self._content->extents().height());
    if(0 < scale_square && scale_square < 1.0f)
      result = sqrt(scale_square);
  }
  else {
    if(w.is_positive() && self._content->extents().width > w.raw_value()) {
      double scale = w.raw_value() / (double)self._content->extents().width;
      if(0 < scale && scale < result)
        result = scale;
    }
    if(h.is_positive() && self._content->extents().height() > h.raw_value()) {
      double scale = h.raw_value() / (double)self._content->extents().height();
      if(0 < scale && scale < result)
        result = scale;
    }
  }
  
  return result;
}

double PaneBox::Impl::grow_scale(Length w, Length h, bool line_break_within) {
  double result = 1;
  if(w.raw_value() > self._content->extents().width && h.raw_value() > self._content->extents().height()) {
    double scale = std::min(
                     w.raw_value() / (double)self._content->extents().width, 
                     h.raw_value() / (double)self._content->extents().height());
    if(0 < scale && scale < Infinity) 
      result = scale;
  }
  else if(h == SymbolicSize::Automatic && w.raw_value() > self._content->extents().width) {
    double scale = w.raw_value() / (double)self._content->extents().width;
    if(0 < scale && scale < Infinity)
      result = scale;
  }
  else if(w == SymbolicSize::Automatic && h.raw_value() > self._content->extents().height()) {
    double scale = h.raw_value() / (double)self._content->extents().height();
    if(0 < scale && scale < Infinity)
      result = scale;
  }
  return result;
}

//} ... class PaneBox::Impl
