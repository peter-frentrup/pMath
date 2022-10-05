#include <boxes/graphics/graphicsdirective.h>
#include <boxes/graphics/graphicsbox.h>

#include <graphics/context.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Directive;
extern pmath_symbol_t richmath_System_GrayLevel;
extern pmath_symbol_t richmath_System_Hue;
extern pmath_symbol_t richmath_System_RGBColor;

namespace richmath {
  class GraphicsDirective::Impl {
    public:
      Impl(GraphicsDirective &self) : self{self} {}
      
      static void apply_to_style(Expr directive, Style &style);
      static void apply_to_context(Expr directive, Context &context, GraphicsDrawingContext *opt_gc);
      
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

void GraphicsDirective::apply(Expr directive, Context &context) {
  Impl::apply_to_context(directive, context, nullptr);
}

void GraphicsDirective::apply(Expr directive, GraphicsDrawingContext &gc) {
  Impl::apply_to_context(directive, gc.context(), &gc);
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
  
  if(directive.is_rule()) {
    style.add_pmath(std::move(directive));
    return;
  }
}

void GraphicsDirective::Impl::apply_to_context(Expr directive, Context &context, GraphicsDrawingContext *opt_gc) {
  if(directive[0] == richmath_System_Directive) {
    for(auto item : directive.items())
      apply_to_context(item, context, opt_gc);
    return;
  }
  
  if(directive[0] == richmath_System_RGBColor || directive[0] == richmath_System_Hue || directive[0] == richmath_System_GrayLevel) {
    if(Color c = Color::from_pmath(directive)) {
      context.canvas().set_color(c);
    }
    return;
  }
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
