#include <boxes/graphics/beziercurvebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>
#include <util/double-point.h>

#include <math.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BezierCurveBox;
extern pmath_symbol_t richmath_System_List;

namespace richmath {
  class BezierCurveBox::Impl {
    public:
      static void add_segment_interior(GraphicsBounds &bounds, DoublePoint p0, size_t degree, const DoubleMatrix &control_points, size_t index, DoublePoint pt);
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
  size_t num_points = _points.rows();
  if(num_points == 0)
    return;
  
  int degree_option = get_own_style(SplineDegree, 3);
  if(degree_option <= 0)
    return;
  
  size_t degree = (size_t)degree_option;
  if(degree > 1) {
    bool closed = !!get_own_style(SplineClosed);
    
    bounds.add_point(_points.get(0, 0), _points.get(0, 1));
    
    size_t i = 0;
    if(num_points > degree) {
      for(; i < num_points - degree; i+= degree) {
        bounds.add_point(_points.get(i + degree, 0), _points.get(i + degree, 1));
      }
    }
    
    if(!closed && i != num_points - 1) {
      bounds.add_point(_points.get(num_points - 1, 0), _points.get(num_points - 1, 1));
    }
    
    DoublePoint p0 { _points.get(0, 0), _points.get(0, 1) };
    i = 0;
    if(num_points > degree) {
      for(i; i < num_points - degree; i+= degree) {
        DoublePoint pt { _points.get(i + degree, 0), _points.get(i + degree, 1) };
        Impl::add_segment_interior(bounds, p0, degree, _points, i+1, pt);
        p0 = pt;
      }
    }
    
    if(closed) {
      Impl::add_segment_interior(bounds, p0, num_points - i, _points, i+1, DoublePoint{_points.get(0, 0), _points.get(0, 1)});
    }
    else {
      Impl::add_segment_interior(bounds, p0, num_points - i - 1, _points, i+1, DoublePoint{_points.get(num_points - 1, 0), _points.get(num_points - 1, 1)});
    }
  }
  else {
    for(size_t i = 0; i < num_points; ++i) 
      bounds.add_point(_points.get(i, 0), _points.get(i, 1));
  }
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

void BezierCurveBox::Impl::add_segment_interior(GraphicsBounds &bounds, DoublePoint p0, size_t degree, const DoubleMatrix &control_points, size_t index, DoublePoint pt) {
  //bounds.add_point(p0);
  //bounds.add_point(pt);
  
  switch(degree) {
    case 2: {
      DoublePoint p1 = { control_points.get(index, 0), control_points.get(index, 1) };
      
      if(!bounds.contains(p1)) {
        // c(t)  = (1-t)^2 P0 + 2 (1-t)t P1 + t^2 P2               [separately for x and y coords]
        // c'(t) = -2(1-t) P0 + 2 (-t + (1-t)) P1 + 2 t P2
        //       = 2 (P1 - P0) + 2 t (P0 - 2 P1 + P2)
        // c'(t) = 0 iff
        //       t = (P0 - P1) / (P0 - 2 P1 + P2)
        
        double tx = (p0.x - p1.x) / (p0.x - 2*p1.x + pt.x);
        double ty = (p0.y - p1.y) / (p0.y - 2*p1.y + pt.y);
        
        if(0 < tx && tx < 1) {
          double a = (1-tx)*(1-tx);
          double b = 2 * (1-tx)*tx;
          double c = tx*tx;
          
          bounds.add_point(a * p0.x + b * p1.x + c * pt.x,
                           a * p0.y + b * p1.y + c * pt.y);
        }
        
        if(0 < ty && ty < 1) {
          double a = (1-ty)*(1-ty);
          double b = 2 * (1-ty)*ty;
          double c = ty*ty;
          
          bounds.add_point(a * p0.x + b * p1.x + c * pt.x,
                           a * p0.y + b * p1.y + c * pt.y);
        }
      }
    } return;
    
    case 3: {
      DoublePoint p1 = { control_points.get(index,     0), control_points.get(index,     1) };
      DoublePoint p2 = { control_points.get(index + 1, 0), control_points.get(index + 1, 1) };
      
      if(!bounds.contains(p1) || !bounds.contains(p2)) {
        // c(t)  = (1-t)^3 P0 + 3 (1-t)^2 t P1 + 3 (1-t) t^2 P2 + t^3 P3
        //       = P0 + 3 (P1 - P0) t + 3 (P0 - 2 P1 + P2) t^2 + (P3 - P0 + 3 (P1 - P2)) t^3
        // c'(t) = -3 (1-t)^2 P0 + (-6(1-t)t + 3(1-t)^2) P1 + (-3 t^2 + 6 (1-t)t) P2 + 3 t^2 P3
        //       = 3 (1-t)^2 (P1 - P0) + 6 (1-t) t (P2 - P1) + 3 t^2 (P3 - P2)
        //       = 3 (1 - 2t + t^2) (P1 - P0) + 6 (t - t^2) (P2 - P1) + 3 t^2 (P3 - P2)
        //       = 3 (P1 - P0) + t 6 (P0 - 2 P1 + P2) + t^2 3 (P3 - P0 + 3 (P1 - P2))
        //         ~~~~~~~~~~~     ~~~~~~~~~~~~~~~~~~       ~~~~~~~~~~~~~~~~~~~~~~~~~
        //             3 c                3 b                          3 a
        // c'(t) = 0 iff
        //     t = (-b +/- Sqrt(b^2 - 4 a c)) / 2a   (for a != 0)
        // or
        //     t = -c/b   (for a = 0)
        //
        // Note that c(t) = P0 + 3 c t + 3/2 b t^2 + a t^3
        
        DoublePoint a { pt.x - p0.x + 3 * (p1.x - p2.x),
                        pt.y - p0.y + 3 * (p1.y - p2.y) };
        DoublePoint b { 2 * (p0.x - 2 * p1.x + p2.x),
                        2 * (p0.y - 2 * p1.y + p2.y) };
        DoublePoint c { p1.x - p0.x,
                        p1.y - p0.y };
        for(int i = 0; i < 2; ++i) {
          if(a[i] != 0) {
            double dd = b[i] * b[i] - 4 * a[i] * c[i];
            if(dd == 0) {
              double t = -0.5 * b[i] / a[i];
              
              if(0 < t && t < 1) {
                bounds.add_point( p0.x + t * (3 * c.x + t * (1.5 * b.x + t * a.x)),
                                  p0.y + t * (3 * c.y + t * (1.5 * b.y + t * a.y)) );
              }
            }
            else if(dd > 0) {
              double d = sqrt(dd);
              double t1 = -0.5 * (b[i] + d) / a[i];
              double t2 =  0.5 * (d - b[i]) / a[i];
              
              if(0 < t1 && t1 < 1) {
                bounds.add_point( p0.x + t1 * (3 * c.x + t1 * (1.5 * b.x + t1 * a.x)),
                                  p0.y + t1 * (3 * c.y + t1 * (1.5 * b.y + t1 * a.y)) );
              }
              if(0 < t2 && t2 < 1) {
                bounds.add_point( p0.x + t2 * (3 * c.x + t2 * (1.5 * b.x + t2 * a.x)),
                                  p0.y + t2 * (3 * c.y + t2 * (1.5 * b.y + t2 * a.y)) );
              }
            }
          }
          else if(b[i] != 0) {
            double t = -c[i] / b[i];
            
            if(0 < t && t < 1) {
              bounds.add_point( p0.x + t * (3 * c.x + t * (1.5 * b.x + t * a.x)),
                                p0.y + t * (3 * c.y + t * (1.5 * b.y + t * a.y)) );
            }
          }
        }
        
      }
      
    } return;
    
    case 4: return; // TODO: higher order curves ...
  }
}

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
