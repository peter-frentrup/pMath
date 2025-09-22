#include <boxes/graphics/beziercurvebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>
#include <util/double-point.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BezierCurveBox;
extern pmath_symbol_t richmath_System_List;

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
  for(size_t i = 0; i < _points.rows(); ++i) 
    bounds.add_point(_points.get(i, 0), _points.get(i, 1));
}

void BezierCurveBox::paint(GraphicsDrawingContext &gc) {
  size_t num_points = _points.rows();
  if(num_points == 0)
    return;
  
  gc.canvas().move_to(_points.get(0, 0), _points.get(0, 1));
  
  bool closed = !!get_own_style(SplineClosed);
  
  //size_t degree = 3;
  // TODO: check SplineDegree option
  size_t i = 0;
  if(num_points > 3) { // && degree == 3
    for(i = 0; i < num_points - 3; i+= 3) {
      gc.canvas().curve_to(
        _points.get(i+1, 0), _points.get(i+1, 1), 
        _points.get(i+2, 0), _points.get(i+2, 1), 
        _points.get(i+3, 0), _points.get(i+3, 1));
    }
  }
  
  if(closed) {
    if(i + 2 < num_points) {
      gc.canvas().curve_to(
        _points.get(i+1, 0), _points.get(i+1, 1), 
        _points.get(i+2, 0), _points.get(i+2, 1),
        _points.get(0,   0), _points.get(0,   1));
    }
    else if(i + 1 < num_points) {
      // Note that this is different from a quadratic Bézier curve with control point (cx,cy).
      double cx = _points.get(i+1, 0);
      double cy = _points.get(i+1, 1);
      gc.canvas().curve_to(cx, cy, cx, cy, _points.get(0, 0), _points.get(0, 1));
    }
    else 
      gc.canvas().line_to(_points.get(0, 0), _points.get(0, 1));
    
    gc.canvas().close_path();
  }
  else {
    if(i + 2 < num_points) {
      // Note that this is different from a quadratic Bézier curve with control point (cx,cy).
      double cx = _points.get(i+1, 0);
      double cy = _points.get(i+1, 1);
      gc.canvas().curve_to(cx, cy, cx, cy, _points.get(i+2, 0), _points.get(i+2, 1));
    }
    else if(i + 1 < num_points)
      gc.canvas().line_to(_points.get(i+1, 0), _points.get(i+1, 1));
    else
      gc.canvas().rel_move_to(0, 0);
  }
  
  auto mat = gc.canvas().get_matrix();
  gc.canvas().set_matrix(gc.initial_matrix());
  gc.canvas().stroke();
  gc.canvas().set_matrix(mat);
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
