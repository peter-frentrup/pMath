#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/graphicsdrawingcontext.h>

#include <graphics/context.h>

#include <cmath>
#include <limits>

#ifdef max
#  undef max
#endif

#ifdef _MSC_VER
#  define isfinite(x)  (_finite(x))
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif


using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_CapForm;
extern pmath_symbol_t richmath_System_Directive;
extern pmath_symbol_t richmath_System_GrayLevel;
extern pmath_symbol_t richmath_System_Hue;
extern pmath_symbol_t richmath_System_JoinForm;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_PointSize;
extern pmath_symbol_t richmath_System_RGBColor;
extern pmath_symbol_t richmath_System_Thickness;

namespace richmath { namespace strings {
  extern String Bevel;
  extern String Miter;
  extern String Round;
}}

namespace richmath {
  class GraphicsDirective::Impl {
    public:
      Impl(GraphicsDirective &self) : self{self} {}
      
      static void apply_to_style(Expr directive, Style &style);
      static void apply_to_context(Expr directive, GraphicsDrawingContext &gc);
      static void apply_thickness_to_context(Length thickness, GraphicsDrawingContext &gc);
      
      bool change_directives(Expr new_directives);
      
    private:
      GraphicsDirective &self;
  };
}

//{ class GraphicsDirective ...

GraphicsDirective::GraphicsDirective()
  : base(),
    _style(new Style()),
    _dynamic(this, Expr())
{
}

GraphicsDirective::GraphicsDirective(Expr expr)
  : base(),
    _style(new Style()),
    _dynamic(this, expr)
{
}

bool GraphicsDirective::is_graphics_directive(Expr expr) {
  if(expr[0] == richmath_System_Directive)
    return true;
  
  if(expr[0] == richmath_System_RGBColor)
    return true;
  
  if(expr[0] == richmath_System_Hue)
    return true;
  
  if(expr[0] == richmath_System_GrayLevel)
    return true;
  
  if(expr[0] == richmath_System_CapForm)
    return true;
  
  if(expr[0] == richmath_System_JoinForm)
    return true;
  
  if(expr[0] == richmath_System_PointSize)
    return true;
  
  if(expr[0] == richmath_System_Thickness)
    return true;
  
  return false;
}

bool GraphicsDirective::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!is_graphics_directive(expr))
    return false;
  
  if(_dynamic.expr() != expr) {
    _style->clear();
    _dynamic = expr;
    _latest_directives = Expr();
    must_update(true);
  }
  
  finish_load_from_object(std::move(expr));
  return true;
}

GraphicsDirective *GraphicsDirective::try_create(Expr expr, BoxInputFlags opts) {
  GraphicsDirective *box = new GraphicsDirective();
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void GraphicsDirective::paint(GraphicsDrawingContext &gc) {
  if(must_update()) {
    must_update(false);
    
    Expr new_directives;
    if(_dynamic.get_value(&new_directives, Expr())) 
      Impl(*this).change_directives(std::move(new_directives));
  }
  
  apply(_latest_directives, gc);
}

void GraphicsDirective::apply(Expr directive, GraphicsDrawingContext &gc) {
  Impl::apply_to_context(directive, gc);
}

void GraphicsDirective::apply_thickness(Length thickness, GraphicsDrawingContext &gc) {
  Impl::apply_thickness_to_context(thickness, gc);
}

void GraphicsDirective::dynamic_updated() {
  must_update(true);
  base::dynamic_updated();
}

void GraphicsDirective::dynamic_finished(Expr info, Expr result) {
  if(Impl(*this).change_directives(_dynamic.finish_dynamic(result)))
    request_repaint_all();
}

//} ... class GraphicsDirective

//{ class GraphicsDirective::Impl ...

void GraphicsDirective::Impl::apply_to_style(Expr directive, Style &style) {
  if(directive[0] == richmath_System_Directive) {
    for(auto item : directive.items())
      apply_to_style(item, style);
    return;
  }
  
  if(directive[0] == richmath_System_RGBColor || directive[0] == richmath_System_Hue || directive[0] == richmath_System_GrayLevel) {
    if(Color c = Color::from_pmath(directive)) {
      style.set(ColorForGraphics, c);
      style.set(FontColor,        c);
    }
    return;
  }
  
  if(directive[0] == richmath_System_CapForm) {
    style.set_pmath(CapForm, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    style.set_pmath(JoinForm, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_PointSize) {
    style.set_pmath(PointSize, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_Thickness) {
    style.set_pmath(Thickness, directive[1]);
    return;
  }
  
  if(directive.is_rule()) {
    style.add_pmath(std::move(directive));
    return;
  }
}

void GraphicsDirective::Impl::apply_to_context(Expr directive, GraphicsDrawingContext &gc) {
  if(directive[0] == richmath_System_Directive) {
    for(auto item : directive.items())
      apply_to_context(item, gc);
    return;
  }
  
  if(directive[0] == richmath_System_RGBColor || directive[0] == richmath_System_Hue || directive[0] == richmath_System_GrayLevel) {
    if(Color c = Color::from_pmath(directive)) {
      gc.canvas().set_color(c);
    }
    return;
  }
  
  if(directive[0] == richmath_System_Rule) { // directive -> val  is  like  directive(val) for non-colors
    directive = Call(directive[1], directive[2]);
  }
  
  if(directive[0] == richmath_System_CapForm) {
    int val = Style::decode_enum(directive[1], CapForm, -1);
    if(val >= 0) {
      gc.canvas().cap_form((enum CapForm)val);
    }
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    Expr item = directive[1];
    if(item.is_string()) {
      if(item == strings::Bevel) { gc.canvas().join_form(JoinFormBevel); return; }
      if(item == strings::Miter) { gc.canvas().join_form(JoinFormMiter); return; }
      if(item == strings::Round) { gc.canvas().join_form(JoinFormRound); return; }
      return;
    }
    
    if(item == richmath_System_None) {
      gc.canvas().join_form(JoinFormNone);
      return;
    }
    
    if(item[0] == richmath_System_List && item.expr_length() == 2 && item[1] == strings::Miter) {
      gc.canvas().join_form(JoinFormMiter);
      
      Expr miter_limit_obj = item[2];
      if(miter_limit_obj.is_number()) {
        float miter_limit = (float)miter_limit_obj.to_double();
        if(miter_limit >= 0 && isfinite(miter_limit)) {
          gc.canvas().miter_limit(miter_limit);
        }
      }
      return;
    }

    return;
  }
  
  if(directive[0] == richmath_System_PointSize) {
    if(Length len = Length::from_pmath(directive[1])) {
      gc.point_size = len;
    }
    return;
  }
  
  if(directive[0] == richmath_System_Thickness) {
    if(Length len = Length::from_pmath(directive[1])) {
      apply_thickness_to_context(len, gc);
    }
    return;
  }
}

void GraphicsDirective::Impl::apply_thickness_to_context(Length thickness, GraphicsDrawingContext &gc) {
  gc.canvas().line_width(thickness.resolve(1.0f, LengthConversionFactors::ThicknessInPt, gc.plot_range_width));
}

bool GraphicsDirective::Impl::change_directives(Expr new_directives) {
  if(self._latest_directives == new_directives)
    return false;
  
  self._latest_directives = new_directives;
  self._style->clear();
  apply_to_style(std::move(new_directives), *self._style.ptr());
  return true;
}

//} ... class GraphicsDirective::Impl
