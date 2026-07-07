#include <boxes/graphics/rectanglebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>

#include <algorithm>
#include <cmath>


#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif


namespace richmath {
  class RectangleBox::Impl {
    public:
      explicit Impl(RectangleBox &self);
      
      void load_points(Expr args);
      
    private:
      RectangleBox &self;
  };
}

using namespace richmath;

extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_RectangleBox;

//{ class RectangleBox ...

RectangleBox::RectangleBox()
  : base(),
    p0{0.0, 0.0},
    p1{1.0, 1.0},
    _dynamic_args(this, Expr())
{
}

RectangleBox::~RectangleBox() {
}

bool RectangleBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_RectangleBox))
    return false;
  
  size_t exprlen = expr.expr_length();
  size_t last_nonopt = exprlen < 2 ? exprlen : 2;
  while(last_nonopt > 0 && expr[last_nonopt].is_rule())
    --last_nonopt;
  
  Expr options_expr(pmath_options_extract_ex(expr.get(), last_nonopt, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options_expr.is_null())
    return false;
    
  Expr args = Expr(pmath_expr_get_item_range(pmath_ref(expr.get()), 1, last_nonopt));
  
  reset_style();
  _style.add_pmath(options_expr);
  
  if(_dynamic_args.expr() != args) {
    _dynamic_args = PMATH_CPP_MOVE(args);
    must_update(true);
  }
  
  if(!_dynamic_args.has_dynamic() && _dynamic_args.get_value(&args, Expr())) {
    must_update(false);
    Impl(*this).load_points(PMATH_CPP_MOVE(args));
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

RectangleBox *RectangleBox::try_create(Expr expr, BoxInputFlags opts) {
  RectangleBox *box = new RectangleBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void RectangleBox::find_extends(GraphicsBounds &bounds) {
  bounds.add_point(p0.x, p0.y);
  bounds.add_point(p0.x, p1.y);
  bounds.add_point(p1.x, p1.y);
  bounds.add_point(p1.x, p0.y);
}

void RectangleBox::dynamic_updated() {
  must_update(true);
  base::dynamic_updated();
}

void RectangleBox::dynamic_finished(Expr info, Expr result) {
  Impl(*this).load_points(_dynamic_args.finish_dynamic(result));
  request_repaint_all();
}

void RectangleBox::paint(GraphicsDrawingContext &gc) {
  if(must_update()) {
    must_update(false);
    
    Expr args;
    if(_dynamic_args.get_value(&args, Expr()))
      Impl(*this).load_points(PMATH_CPP_MOVE(args));
  }
  
  auto mat = gc.canvas().get_matrix();
  
  BoxRadius radii;
  if(Expr expr = get_own_style(BorderRadius)) 
    radii = BoxRadius(expr);
  
  Interval<double> xrange(std::min(p0.x, p1.x), std::max(p0.x, p1.x));
  Interval<double> yrange(std::min(p0.y, p1.y), std::max(p0.y, p1.y));
  
  radii.normalize(xrange.length(), yrange.length());
  gc.canvas().ellipse_arc(
    xrange.from + radii.bottom_left_x,
    yrange.from + radii.bottom_left_y,
    radii.bottom_left_x,
    radii.bottom_left_y,
    -M_PI,
    -M_PI / 2.0,
    false);
    
  gc.canvas().ellipse_arc(
    xrange.to   - radii.bottom_right_x,
    yrange.from + radii.bottom_right_y,
    radii.bottom_right_x,
    radii.bottom_right_y,
    -M_PI / 2.0,
    0.0,
    false);
    
  gc.canvas().ellipse_arc(
    xrange.to - radii.top_right_x,
    yrange.to - radii.top_right_y,
    radii.top_right_x,
    radii.top_right_y,
    0.0,
    M_PI / 2.0,
    false);
    
  gc.canvas().ellipse_arc(
    xrange.from + radii.top_left_x,
    yrange.to   - radii.top_left_y,
    radii.top_left_x,
    radii.top_left_y,
    M_PI / 2.0,
    M_PI,
    false);
  
  gc.canvas().close_path();
  
  gc.canvas().set_matrix(gc.initial_matrix());
  gc.fill_with_edgeform();
  gc.canvas().set_matrix(mat);
}

Expr RectangleBox::to_pmath_impl(BoxOutputFlags flags) {
  Expr expr = _dynamic_args.expr();
  
  size_t num_args = expr.expr_length();
  
  Gather g;
  for(size_t i = 1; i <= num_args; ++i) {
    Gather::emit(expr[i]);
  }
  
  _style.emit_to_pmath();
  
  expr = g.end();
  expr.set(0, Symbol(richmath_System_RectangleBox));
  
  return expr;
}

//} ... class RectangleBox

//{ class RectangleBox::Impl ...

RectangleBox::Impl::Impl(RectangleBox &self)
: self{self}
{
}

void RectangleBox::Impl::load_points(Expr args) {
  self.p0 = DoublePoint{0.0, 0.0};
  if(args.expr_length() >= 1) {
    DoublePoint::load_point(self.p0, args[1]); // TODO: message if load_point() returns false
  }
  
  self.p1 = DoublePoint{self.p0.x + 1.0, self.p0.y + 1.0};
  
  if(args.expr_length() >= 2) {
    DoublePoint::load_point(self.p1, args[2]); // TODO: message if load_point() returns false
  }
}

//} ... class RectangleBox::Impl
