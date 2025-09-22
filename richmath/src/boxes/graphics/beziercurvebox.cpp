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
    
  if(expr.expr_length() != 1)
    return false;
  
  DoubleMatrix new_points;
  if(!DoublePoint::load_line(new_points, expr[1]))
    return false;
  
  /* now success is guaranteed */
  _points_expr = expr[1];
  swap(_points, new_points);
  
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

  // last segment may have lower degree
  if(i + 2 < num_points) {
    double cx = _points.get(i+1, 0);
    double cy = _points.get(i+1, 1);
    gc.canvas().curve_to(cx, cy, cx, cy, _points.get(i+2, 0), _points.get(i+2, 1));
  }
  else if(i + 1 < num_points)
    gc.canvas().line_to(_points.get(i+1, 0), _points.get(i+1, 1));
  else
    gc.canvas().rel_move_to(0, 0);
  
  auto mat = gc.canvas().get_matrix();
  gc.canvas().set_matrix(gc.initial_matrix());
  gc.canvas().stroke();
  gc.canvas().set_matrix(mat);
}

Expr BezierCurveBox::to_pmath_impl(BoxOutputFlags flags) {
  return Call(Symbol(richmath_System_BezierCurveBox), _points_expr);
}

//} ... class BezierCurveBox
