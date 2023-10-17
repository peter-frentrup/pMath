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
extern pmath_symbol_t richmath_System_Dashing;
extern pmath_symbol_t richmath_System_Directive;
extern pmath_symbol_t richmath_System_DrawEdges;
extern pmath_symbol_t richmath_System_EdgeForm;
extern pmath_symbol_t richmath_System_GrayLevel;
extern pmath_symbol_t richmath_System_Hue;
extern pmath_symbol_t richmath_System_JoinForm;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_NCache;
extern pmath_symbol_t richmath_System_None;
extern pmath_symbol_t richmath_System_PointSize;
extern pmath_symbol_t richmath_System_RGBColor;
extern pmath_symbol_t richmath_System_Scaled;
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
      
      static void apply_to_style(  Expr directive, Style &style);
      static void apply_to_context(Expr directive, GraphicsDrawingContext &gc);
      static void apply_edgeform_to_style(  Expr directive, Style &style);
      static void apply_edgeform_to_context(Expr directive, GraphicsDrawingContext &gc);
      
      static bool decode_dash_array(Array<double> &dash_array, Expr dashes, float scale_factor);
      static bool decode_dash_offset(double &offset, Expr obj, float scale_factor);
      static void enlarge_zero_dashes(Array<double> &dash_array);
      
      static bool decode_joinform(JoinForm &join_form, float &miter_limit, Expr expr);
      
      bool change_directives(Expr new_directives);
      
    private:
      GraphicsDirective &self;
  };
}

//{ class GraphicsDirective ...

GraphicsDirective::GraphicsDirective()
  : base(),
    _style(new StyleData()),
    _dynamic(this, Expr())
{
}

GraphicsDirective::GraphicsDirective(Expr expr)
  : base(),
    _style(new StyleData()),
    _dynamic(this, expr)
{
}

bool GraphicsDirective::is_graphics_directive(Expr expr) {
  if(expr[0] == richmath_System_Directive) return true;
  if(expr[0] == richmath_System_RGBColor)  return true;
  if(expr[0] == richmath_System_Hue)       return true;
  if(expr[0] == richmath_System_GrayLevel) return true;
  if(expr[0] == richmath_System_CapForm)   return true;
  if(expr[0] == richmath_System_Dashing)   return true;
  if(expr[0] == richmath_System_EdgeForm)  return true;
  if(expr[0] == richmath_System_JoinForm)  return true;
  if(expr[0] == richmath_System_PointSize) return true;
  if(expr[0] == richmath_System_Thickness) return true;
  
  return false;
}

bool GraphicsDirective::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!is_graphics_directive(expr))
    return false;
  
  if(_dynamic.expr() != expr) {
    _style.reset();
    _dynamic = expr;
    _latest_directives = Expr();
    must_update(true);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
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
      Impl(*this).change_directives(PMATH_CPP_MOVE(new_directives));
  }
  
  apply(_latest_directives, gc);
}

void GraphicsDirective::apply(Expr directive, GraphicsDrawingContext &gc) {
  Impl::apply_to_context(directive, gc);
}

void GraphicsDirective::dynamic_updated() {
  must_update(true);
  base::dynamic_updated();
}

void GraphicsDirective::dynamic_finished(Expr info, Expr result) {
  if(Impl(*this).change_directives(_dynamic.finish_dynamic(result)))
    request_repaint_all();
}

bool GraphicsDirective::decode_joinform(enum JoinForm &join_form, float &miter_limit, Expr expr) {
  if(expr.is_string()) {
    if(expr == strings::Bevel) { join_form = JoinFormBevel; return true; }
    if(expr == strings::Miter) { join_form = JoinFormMiter; miter_limit = 10.0f; return true; }
    if(expr == strings::Round) { join_form = JoinFormRound; return true; }
    return false;
  }
  
  if(expr == richmath_System_None) {
    join_form = JoinFormNone;
    return true;
  }
  
  if(expr[0] == richmath_System_List && expr.expr_length() == 2 && expr[1] == strings::Miter) {
    join_form = JoinFormMiter;
    
    Expr miter_limit_obj = expr[2];
    if(miter_limit_obj.is_number()) {
      miter_limit = (float)miter_limit_obj.to_double();
      if(miter_limit >= 0 && isfinite(miter_limit)) {
        return true;
      }
    }
    return false;
  }
  return false;
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
  
  if(directive[0] == richmath_System_EdgeForm) {
    Expr edgeform = directive[1];
    if(edgeform == richmath_System_None || directive.expr_length() != 1) {
      style.set(DrawEdges, false);
    }
    else {
      style.set(DrawEdges, true);
      if(edgeform[0] == richmath_System_List) {
        for(auto item : edgeform.items())
          apply_edgeform_to_style(PMATH_CPP_MOVE(item), style);
      }
      else
        apply_edgeform_to_style(PMATH_CPP_MOVE(edgeform), style);
    }
    return;
  }
  
  if(directive[0] == richmath_System_CapForm) {
    style.set_pmath(CapForm, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_Dashing) {
    // TODO: support Dashing(dashes, offset, capform)
    style.set_pmath(Dashing, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    style.set_pmath(ObjectStyleOptionName::JoinForm, directive[1]);
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
    style.add_pmath(PMATH_CPP_MOVE(directive));
    return;
  }
}

void GraphicsDirective::Impl::apply_edgeform_to_style(Expr directive, Style &style) {
  if(directive[0] == richmath_System_RGBColor || directive[0] == richmath_System_Hue || directive[0] == richmath_System_GrayLevel) {
    if(Color c = Color::from_pmath(directive)) {
      style.set(EdgeColor, c);
    }
    return;
  }
  
  if(directive[0] == richmath_System_Dashing) {
    // TODO: support Dashing(dashes, offset, capform)
    style.set_pmath(EdgeDashing, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    style.set_pmath(EdgeJoinForm, directive[1]);
    return;
  }
  
  if(directive[0] == richmath_System_Thickness) {
    style.set_pmath(EdgeThickness, directive[1]);
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
  
  if(directive[0] == richmath_System_EdgeForm) {
    Expr edgeform = directive[1];
    if(edgeform == richmath_System_None || directive.expr_length() != 1) {
      gc.draw_edges = false;
    }
    else {
      gc.draw_edges = true;
      if(edgeform[0] == richmath_System_List || edgeform[0] == richmath_System_Directive) {
        for(auto item : edgeform.items())
          apply_edgeform_to_context(PMATH_CPP_MOVE(item), gc);
      }
      else
        apply_edgeform_to_context(PMATH_CPP_MOVE(edgeform), gc);
    }
    return;
  }
  
  if(directive[0] == richmath_System_Rule) { // directive -> val  is  like  directive(val) for most directives
    directive = Call(directive[1], directive[2]);
  }
  
  if(directive[0] == richmath_System_CapForm) {
    int val = StyleData::decode_enum(directive[1], CapForm, -1);
    if(val >= 0 && gc.canvas().dash_count() == 0) {
      gc.canvas().cap_form((enum CapForm)val);
    }
    return;
  }
  
  if(directive[0] == richmath_System_Dashing) {
    Array<double> dash_array;
    double offset = 0.0;
    if(decode_dash_array(dash_array, directive[1], gc.plot_range_width)) {
      if(directive.expr_length() < 2 || decode_dash_offset(offset, directive[2], gc.plot_range_width)) {
        int capform_val = (int)CapFormButt;
        if(directive.expr_length() == 3) {
          if(directive[3][0] == richmath_System_CapForm)
            capform_val = StyleData::decode_enum(directive[3][1], CapForm, -1);
          else
            capform_val = StyleData::decode_enum(directive[3], CapForm, -1);
        }
        
        if(capform_val >= 0) {
          gc.canvas().cap_form((enum CapForm)capform_val);
          
          if(capform_val == (int)CapFormButt)
            enlarge_zero_dashes(dash_array);
          
          gc.canvas().set_dashes(dash_array, offset);
        }
      }
    }
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    JoinForm join_form;
    float miter_limit;
    if(GraphicsDirective::decode_joinform(join_form, miter_limit, directive[1])) {
      gc.canvas().join_form(join_form);
      if(join_form == JoinFormMiter)
        gc.canvas().miter_limit(miter_limit);
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
      gc.apply_thickness(len);
    }
    return;
  }
}

void GraphicsDirective::Impl::apply_edgeform_to_context(Expr directive, GraphicsDrawingContext &gc) {
  if(directive[0] == richmath_System_RGBColor || directive[0] == richmath_System_Hue || directive[0] == richmath_System_GrayLevel) {
    if(Color c = Color::from_pmath(directive)) {
      gc.edge_color = c;
    }
    return;
  }
  
  if(directive[0] == richmath_System_Thickness) {
    if(Length len = Length::from_pmath(directive[1])) {
      gc.edge_thickness = len;
    }
    return;
  }
  
  if(directive[0] == richmath_System_JoinForm) {
    JoinForm join_form;
    float miter_limit;
    if(GraphicsDirective::decode_joinform(join_form, miter_limit, directive[1])) {
      gc.edge_joinform = join_form;
      if(join_form == JoinFormMiter)
        gc.edge_miter_limit = miter_limit;
    }
    return;
  }
  
  // TODO: Dashing
}

bool GraphicsDirective::Impl::decode_dash_array(Array<double> &dash_array, Expr dashes, float scale_factor) {
  if(dashes[0] == richmath_System_List && dashes.expr_length() < 1000) {
    int num_dashes = (int)dashes.expr_length();
    dash_array.length(num_dashes);
    for(int i = 0; i < num_dashes; ++i) {
      if(Length len = Length::from_pmath(dashes[i+1])) {
        dash_array[i] = len.resolve(1.0f, LengthConversionFactors::NormalDashingInPt,  scale_factor);
      }
      else
        return false;
    }
  }
  else if(Length len = Length::from_pmath(dashes)) {
    if(len.is_symbolic()) {
      dash_array.length(2);
      dash_array[0] = len.resolve(1.0f, LengthConversionFactors::SimpleDashingOnInPt,  scale_factor);
      dash_array[1] = len.resolve(1.0f, LengthConversionFactors::SimpleDashingOffInPt, scale_factor);
    }
    else {
      dash_array.length(1);
      dash_array[0] = len.resolve(1.0f, LengthConversionFactors::SimpleDashingOnInPt, scale_factor);
    }
  }
  else
    return false;
  
  if(dash_array.length() > 0) {
    bool all_zero = true;
    for(double val : dash_array) {
      if(!isfinite(val) || val < 0)
        return false;
      
      if(val > 0)
        all_zero = false;
    }
    
    if(all_zero)
      return false;
  }
  
  return true;
}

bool GraphicsDirective::Impl::decode_dash_offset(double &offset, Expr obj, float scale_factor) {
  if(obj[0] == richmath_System_NCache)
    obj = obj[2];
  
  if(obj.is_number()) {
    offset = obj.to_double();
    return true;
  }
  
  if(obj[0] == richmath_System_Scaled && obj.expr_length() == 1) {
    Expr scale = obj[1];
    
    if(scale[0] == richmath_System_NCache)
      scale = scale[2];
    
    if(scale.is_number()) {
      offset = obj.to_double() * scale_factor;
      return true;
    }
  }
  
  return false;
}

void GraphicsDirective::Impl::enlarge_zero_dashes(Array<double> &dash_array) {
  const double default_size = 1.0;
  
  for(int i = 0; i < dash_array.length(); ++i) {
    if(dash_array[i] == 0) {
      dash_array[i] = default_size;
      
//      if(i + 1 < dash_array.length()) {
//        if(dash_array[i+1] > default_size)
//          dash_array[i+1] -= default_size;
//        else
//          dash_array[i+1] = 0;
//      }
    }
  }
}

bool GraphicsDirective::Impl::change_directives(Expr new_directives) {
  if(self._latest_directives == new_directives)
    return false;
  
  self._latest_directives = new_directives;
  self._style.reset();
  apply_to_style(PMATH_CPP_MOVE(new_directives), self._style);
  return true;
}

//} ... class GraphicsDirective::Impl
