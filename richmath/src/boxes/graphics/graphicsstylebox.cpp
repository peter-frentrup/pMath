#include <boxes/graphics/graphicsstylebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <boxes/graphics/graphicsdirective.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BaseStyle;
extern pmath_symbol_t richmath_System_Directive;
extern pmath_symbol_t richmath_System_Rule;
extern pmath_symbol_t richmath_System_StyleBox;

namespace richmath {
  class GraphicsStyleBox::Impl {
    public:
      Impl(GraphicsStyleBox &self) : self{self} {}
      
      bool change_directives(Expr new_directives);
      
    private:
      GraphicsStyleBox &self;
  };
}

//{ class GraphicsStyleBox ...

GraphicsStyleBox::GraphicsStyleBox()
  : base(),
    _content(nullptr),
    _style(new StyleData())
{
}

GraphicsStyleBox::~GraphicsStyleBox() {
  if(_content) delete_owned(_content);
}

bool GraphicsStyleBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_StyleBox))
    return false;
    
  if(expr.expr_length() < 1)
    return false;
  
  if(!_content || !_content->try_load_from_object(expr[1], opts)) {
    if(_content) delete_owned(_content);
    _content = GraphicsElement::create(expr[1], opts);
    set_style_parent_of_child(_content, this);
  }
  
  Expr directives = expr.rest();
  directives.set(0, Symbol(richmath_System_Directive));
  
  Expr tag = directives[1];
  if(tag.is_string()) {
    directives.set(1, Rule(Symbol(richmath_System_BaseStyle), tag));
  }
  
  if(_dynamic_directives.expr() != directives) {
    _style.reset();
    GraphicsDirective::apply_to_style(directives, _style);
    _dynamic_directives = directives;
    _latest_directives = Expr();
    must_update(true);
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

GraphicsStyleBox *GraphicsStyleBox::try_create(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_StyleBox))
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  GraphicsStyleBox *box = new GraphicsStyleBox();
  if(!box->try_load_from_object(PMATH_CPP_MOVE(expr), opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void GraphicsStyleBox::find_extends(GraphicsBounds &bounds) {
  if(_content)
    _content->find_extends(bounds);
}

void GraphicsStyleBox::dynamic_updated() {
  must_update(true);
  base::dynamic_updated();
}

void GraphicsStyleBox::dynamic_finished(Expr info, Expr result) {
  if(Impl(*this).change_directives(_dynamic_directives.finish_dynamic(result)))
    request_repaint_all();
}

void GraphicsStyleBox::paint(GraphicsDrawingContext &gc) {
  gc.canvas().save();
  Color old_color       = gc.canvas().get_color();
  Length old_point_size = gc.point_size;
  
  if(must_update()) {
    must_update(false);
    
    Expr new_directives;
    if(_dynamic_directives.get_value(&new_directives, Expr()))
      Impl(*this).change_directives(PMATH_CPP_MOVE(new_directives));
  }
  // TODO: apply directives from _style, that are derived via BaseStyle
  GraphicsDirective::apply(_latest_directives, gc);
  
  if(_content) {
    _content->paint(gc);
  }
  
  gc.point_size = old_point_size;
  gc.canvas().set_color(old_color);
  gc.canvas().restore();
}

Expr GraphicsStyleBox::to_pmath_impl(BoxOutputFlags flags) {
  size_t num_opts = _latest_directives.expr_length();
  Expr expr = MakeCall(Symbol(richmath_System_StyleBox), num_opts + 1);
  
  if(_content)
    expr.set(1, _content->to_pmath(flags));
  
  for(size_t i = 1; i <= num_opts; ++i)
    expr.set(i + 1, _latest_directives[i]);
  
  Expr tag = expr[2];
  if(tag.is_rule() && tag.item_equals(1, richmath_System_BaseStyle)) {
    Expr tag_rhs = tag[2];
    if(tag_rhs.is_string())
      expr.set(2, tag_rhs);
  } 
  
  return expr;
}

//} ... class GraphicsStyleBox

//{ class GraphicsStyleBox::Impl ...

bool GraphicsStyleBox::Impl::change_directives(Expr new_directives) {
  if(self._latest_directives == new_directives)
    return false;
  
  self._latest_directives = new_directives;
  self._style.reset();
  GraphicsDirective::apply_to_style(PMATH_CPP_MOVE(new_directives), self._style);
  return true;
}

//} ... class GraphicsStyleBox::Impl
