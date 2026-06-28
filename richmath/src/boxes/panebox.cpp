#include <boxes/panebox.h>
#include <boxes/mathsequence.h>

#include <graphics/context.h>
#include <util/alignment.h>

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
      
      double shrink_scale(Length w, Length h, const Margins<float> &padding, bool line_break_within);
      double grow_scale(Length w, Length h, const Margins<float> &padding, bool line_break_within);
      
    private:
      PaneBox &self;
  };
}

static const LengthConversionFactors PaneBoxMarginFactors = {
  0.0f, // Automatic
   1 / 16.0f, // Tiny     For FontSize->12, 96 dpi: 12 * 1/16 = 0.75pt = 1 pixel
   3 / 16.0f, // Small
   5 / 16.0f, // Medium
  10 / 16.0f, // Large
};

//{ class PaneBox ...

PaneBox::PaneBox(AbstractSequence *content)
  : base(content)
{
}

PaneBox::~PaneBox() {
}

bool PaneBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_PaneBox))
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
  
  float em = context.canvas().get_font_size();
  auto old_w = context.width;
  
  Margins<Length> frame_margins {
    get_own_style(FrameMarginLeft,   SymbolicSize::Automatic),
    get_own_style(FrameMarginRight,  SymbolicSize::Automatic),
    get_own_style(FrameMarginTop,    SymbolicSize::Automatic),
    get_own_style(FrameMarginBottom, SymbolicSize::Automatic)};
  
  bool has_auto_fame = frame_margins == Margins<SymbolicSize>(SymbolicSize::Automatic);
  
  Margins<float> padding {0.0f};
  if(!has_auto_fame) {
    padding.left   = frame_margins.left.resolve(  em, PaneBoxMarginFactors, old_w);
    padding.right  = frame_margins.right.resolve( em, PaneBoxMarginFactors, old_w);
    padding.top    = frame_margins.top.resolve(   em, PaneBoxMarginFactors, old_w);
    padding.bottom = frame_margins.bottom.resolve(em, PaneBoxMarginFactors, old_w);
  }
  
//  context.width -= padding.left + padding.right;
  
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
      double scale = Impl(*this).shrink_scale(w, h, padding, line_break_within);
      
      if(scale < 1) {
        mat.xx = mat.yy = scale;
        base::resize_default_baseline(context);
        
        for(int retry = 3; retry > 0; --retry) {
          double next_scale = Impl(*this).shrink_scale(w, h, padding, line_break_within);
          if(next_scale < mat.xx)
            mat.xx = mat.yy = next_scale;
          else
            break;
          
          base::resize_default_baseline(context);
        }
      }
      else if(image_size_action == ImageSizeActionResizeToFit) {
        scale = Impl(*this).grow_scale(w, h, padding, line_break_within);
        
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
  
  SimpleAlignment alignment = SimpleAlignment::from_pmath(get_own_style(Alignment), SimpleAlignment::TopLeft);
  
  _extents.width = w.explicit_abs_value();
  float inner_w = _extents.width - padding.left - padding.right;
  
  if(content()->extents().width < inner_w && mat.xx != 0 && mat.yy != 0 && mat.xy == 0 && mat.yx == 0) {
    context.canvas().save();
    context.canvas().transform(mat);
    BoxSize scaled_size = _extents;
    scaled_size.ascent  /= mat.yy;
    scaled_size.descent /= mat.yy;
    scaled_size.width   /= mat.xx;
    if(content()->expand(context, scaled_size)) {
      scaled_size       = content()->extents();
      _extents.ascent   = scaled_size.ascent  * mat.yy;
      _extents.descent  = scaled_size.descent * mat.yy;
      _extents.width    = w.explicit_abs_value();
    }
    context.canvas().restore();
  }
  
  float max_cx = inner_w - content()->extents().width;
  if(max_cx > 0) {
    cx = alignment.interpolate_left_to_right(0, max_cx);
  }
  
  cx += padding.left;
  
  float orig_ascent  = _extents.ascent;
  float orig_descent = _extents.descent;
  float new_h        = h.explicit_abs_value() - padding.top - padding.bottom;
  
  _extents.ascent  = padding.top    + alignment.interpolate_bottom_to_top(new_h - orig_descent, orig_ascent);
  _extents.descent = padding.bottom + alignment.interpolate_bottom_to_top(orig_descent,         new_h - orig_ascent);

  context.width = old_w;
}

float PaneBox::allowed_content_width(Context &context) {
  if(get_own_style(LineBreakWithin, true)) {
    Length w = get_own_style(ImageSizeHorizontal, SymbolicSize::Automatic);
    
    if(w.is_explicit_rel())
      w = Length::Absolute(w.explicit_rel_value() * context.width);
      
    if(w.is_explicit_abs_positive()) {
      Length frame_margin_left  = get_own_style(FrameMarginLeft,  SymbolicSize::Automatic);
      Length frame_margin_right = get_own_style(FrameMarginRight, SymbolicSize::Automatic);
      
      float em = context.canvas().get_font_size();
      auto old_w = context.width;
      
      float padding_left  = frame_margin_left.resolve( em, PaneBoxMarginFactors, old_w);
      float padding_right = frame_margin_right.resolve(em, PaneBoxMarginFactors, old_w);
      
      double inner_w = w.explicit_abs_value() - padding_left - padding_right;
      if(inner_w > 0) {
        if(mat.xx > 0)
          return inner_w / mat.xx + (mat.xx > 1 ? 0.75f : 0.0f);
          
        return inner_w;
      }
      
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

double PaneBox::Impl::shrink_scale(Length w, Length h, const Margins<float> &padding, bool line_break_within) {
  double result = 1.0f;
  
  if(line_break_within && w.is_explicit_abs_positive() && h.is_explicit_abs_positive()) {
    double inner_w = w.explicit_abs_value() - padding.left - padding.right;
    double inner_h = h.explicit_abs_value() - padding.top - padding.bottom;
    if(inner_w > 0 && inner_h > 0) {
      double scale_square = inner_w * inner_h / ((double)self._content->extents().width * self._content->extents().height());
      if(0 < scale_square && scale_square < 1.0f)
        result = sqrt(scale_square);
      
      return result;
    }
  }
  
  if(w.is_explicit_abs_positive()) {
    double inner_w = w.explicit_abs_value() - padding.left - padding.right;
    if(inner_w > 0 && self._content->extents().width > inner_w) {
      double scale = inner_w / (double)self._content->extents().width;
      if(0 < scale && scale < result)
        result = scale;
    }
  }
  
  if(h.is_explicit_abs_positive()) {
    double inner_h = h.explicit_abs_value() - padding.top - padding.bottom;
    if(inner_h > 0 && self._content->extents().height() > inner_h) {
      double scale = inner_h / (double)self._content->extents().height();
      if(0 < scale && scale < result)
        result = scale;
    }
  }
  
  return result;
}

double PaneBox::Impl::grow_scale(Length w, Length h, const Margins<float> &padding, bool line_break_within) {
  float abs_tol = 0.75f;
  double result = 1;
  
  if(w.is_explicit_abs() && h.is_explicit_abs()) {
    double inner_w = w.explicit_abs_value() - padding.left - padding.right;
    double inner_h = h.explicit_abs_value() - padding.top - padding.bottom;
    
    if( inner_w > self._content->extents().width    + abs_tol && 
        inner_h > self._content->extents().height() + abs_tol
    ) {
      double scale = std::min(
                       (inner_h - abs_tol) / (double)self._content->extents().width, 
                       (inner_h - abs_tol) / (double)self._content->extents().height());
      if(0 < scale && scale < Infinity) 
        result = scale;
      
      return result;
    }
  }
  
  if(h == SymbolicSize::Automatic && w.is_explicit_abs()) {
    double inner_w = w.explicit_abs_value() - padding.left - padding.right;
  
    if(inner_w > self._content->extents().width + abs_tol) {
      double scale = (inner_w - abs_tol) / (double)self._content->extents().width;
      if(0 < scale && scale < Infinity)
        result = scale;
      return result;
    }
  }
  
  if(w == SymbolicSize::Automatic && h.is_explicit_abs()) {
    double inner_h = h.explicit_abs_value() - padding.top - padding.bottom;
    
    if(inner_h > self._content->extents().height() + abs_tol) {
      double scale = (inner_h - abs_tol) / (double)self._content->extents().height();
      if(0 < scale && scale < Infinity)
        result = scale;
        
      return result;
    }
  }
  
  return result;
}

//} ... class PaneBox::Impl
