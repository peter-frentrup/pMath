#include <boxes/graphics/beziercurvebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>
#include <util/double-point.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BezierCurveBox;
extern pmath_symbol_t richmath_System_List;

namespace richmath {
  class BezierCurveBox::Impl {
    public:
      static void add_segment_to(Canvas &canvas, size_t degree, const DoubleMatrix &control_points, size_t index, DoublePoint pt);
  };
}

//{ class BezierCurveBox ...

BezierCurveBox::BezierCurveBox()
  : GraphicsElement()
{
}

bool BezierCurveBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_System_BezierCurveBox))
    return false;
    
  if(expr.expr_length() < 1)
    return false;
  
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
  
  DoubleMatrix new_points;
  if(!DoublePoint::load_line(new_points, expr[1]))
    return false;
  
  /* now success is guaranteed */
  _points_expr = expr[1];
  swap(_points, new_points);
  
  if(style) {
    reset_style();
    style.add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = Style(options);
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

BezierCurveBox *BezierCurveBox::try_create(Expr expr, BoxInputFlags opts) {
  BezierCurveBox *box = new BezierCurveBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void BezierCurveBox::find_extends(GraphicsBounds &bounds) {
  // TODO: use tight bound instead of control points
  for(size_t i = 0; i < _points.rows(); ++i) 
    bounds.add_point(_points.get(i, 0), _points.get(i, 1));
}

void BezierCurveBox::paint(GraphicsDrawingContext &gc) {
  size_t num_points = _points.rows();
  if(num_points == 0)
    return;
  
  bool closed = !!get_own_style(SplineClosed);
  
  int degree_option = get_own_style(SplineDegree, 3);
  if(degree_option <= 0)
    return;
  
  Canvas &canvas = gc.canvas();
  canvas.move_to(_points.get(0, 0), _points.get(0, 1));
  
  size_t degree = (size_t)degree_option;
  // TODO: check SplineDegree option
  size_t i = 0;
  if(num_points > degree) {
    for(i = 0; i < num_points - degree; i+= degree) {
      Impl::add_segment_to(
        canvas, degree, _points, i+1, 
        DoublePoint{_points.get(i + degree, 0), _points.get(i + degree, 1)});
    }
  }
  // Now have  num_points     - degree <= i < num_points
  // Hence     num_points - i - degree <= 0 < num_points - i
  // Hence     0 < num_points - i <= degree
  
  if(closed) {
    Impl::add_segment_to(canvas, num_points - i, _points, i+1, DoublePoint{_points.get(0, 0), _points.get(0, 1)});
    canvas.close_path();
  }
  else {
    Impl::add_segment_to(canvas, num_points - i - 1, _points, i+1, DoublePoint{_points.get(num_points - 1, 0), _points.get(num_points - 1, 1)});
  }
  
  auto mat = canvas.get_matrix();
  canvas.set_matrix(gc.initial_matrix());
  canvas.stroke();
  canvas.set_matrix(mat);
}

Expr BezierCurveBox::to_pmath_impl(BoxOutputFlags flags) { 
  Gather g;
  g.emit(_points_expr);
  style.emit_to_pmath(false);

  Expr expr = g.end();
  expr.set(0, Symbol(richmath_System_BezierCurveBox));
  return expr;
}

Expr BezierCurveBox::update_cause() {
  if(!style)
    return Expr();
  
  return get_own_style(InternalUpdateCause);
}

void BezierCurveBox::update_cause(Expr cause) {
  if(!style && !cause)
    return;
  
  style.set(InternalUpdateCause, PMATH_CPP_MOVE(cause));
}

//} ... class BezierCurveBox

//{ class BezierCurveBox::Impl ...

void BezierCurveBox::Impl::add_segment_to(Canvas &canvas, size_t degree, const DoubleMatrix &control_points, size_t index, DoublePoint pt) {
  switch(degree) {
    case 0: return;
    case 1: canvas.line_to(pt.x, pt.y); return;
    case 2: {
      DoublePoint c { control_points.get(index, 0), control_points.get(index, 1) };
      
      DoublePoint p0;
      canvas.current_pos(&p0.x, &p0.y);
      
      DoublePoint c1 = { c.x - (c.x - p0.x) / 3.0,
                         c.y - (c.y - p0.y) / 3.0};
      DoublePoint c2 = { pt.x - (pt.x - c.x) * (2/3.0),
                         pt.y - (pt.y - c.y) * (2/3.0)};
      
      canvas.curve_to(c1.x, c1.y, c2.x, c2.y, pt.x, pt.y);
    } return;
    case 3: 
      canvas.curve_to(
        control_points.get(index,     0), control_points.get(index,     1),
        control_points.get(index + 1, 0), control_points.get(index + 1, 1),
        pt.x, pt.y);
      return;
    
    default: canvas.move_to(pt.x, pt.y); return; // TODO: use De Casteljau algorithm for higher orders
  }
}

//} ... class BezierCurveBox::Impl
