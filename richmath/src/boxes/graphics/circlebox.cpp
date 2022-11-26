#include <boxes/graphics/circlebox.h>

#include <boxes/graphics/graphicsdrawingcontext.h>
#include <graphics/canvas.h>
#include <util/double-point.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER
#  define isnan(x)     (_isnan(x))
#  define isfinite(x)  (_finite(x))
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

namespace richmath {
  class CircleBox::Impl {
    public:
      explicit Impl(CircleBox &self);
      
      bool transform_to_std_circle(cairo_matrix_t &mat);
      
    private:
      CircleBox &self;
  };
}

using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_CircleBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_Range;

static const double TwoPi = 2 * M_PI;

//{ class CircleBox ...

CircleBox::CircleBox()
  : GraphicsElement(),
    cx{0.0}, cy{0.0},
    rx{1.0}, ry{1.0},
    angles{0.0, TwoPi}
{
}

CircleBox::~CircleBox() {
}

bool CircleBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_CircleBox)
    return false;
    
  if(expr.expr_length() > 3)
    return false;
    
  if(_expr == expr) {
    finish_load_from_object(PMATH_CPP_MOVE(expr));
    return true;
  }
  
  _expr = expr;
  cx = cy = 0.0;
  rx = ry = 1.0;
  angles.from = 0.0;
  angles.to = TwoPi;
  if(expr.expr_length() >= 1) {
    DoublePoint center;
    if(DoublePoint::load_point(center, expr[1])) {
      cx = center.x;
      cy = center.y;
    }
    // TODO: else message
  }
  
  if(expr.expr_length() >= 2) {
    Expr rad = expr[2];
    if(rad.expr_length() == 2 && rad[0] == richmath_System_List) {
      DoublePoint radii;
      if(DoublePoint::load_point(radii, rad)) {
        rx = radii.x;
        ry = radii.y;
      }
      // TODO: else message
    }
    else {
      double r = rad.to_double(NAN);
      if(!isnan(r)) {
        rx = ry = r;
      }
      // TODO: else message
    }
  }
  
  if(expr.expr_length() >= 3) {
    Expr angle_range = expr[3];
    if(angle_range.expr_length() == 2 && angle_range[0] == richmath_System_Range) {
      double a1 = angle_range[1].to_double(NAN);
      double a2 = angle_range[2].to_double(NAN);
      
      if(isfinite(a1)) angles.from = a1; // TODO: else message
      if(isfinite(a2)) angles.to   = a2; // TODO: else message
      
      if(angles.to < angles.from) {
        if(angles.length() < -TwoPi)
          angles.to = angles.from + TwoPi;
        else
          angles.to += TwoPi;
      }
      else if(angles.length() > TwoPi)
        angles.to = angles.from + TwoPi;
    }
    // TODO: else message
  }
  
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

CircleBox *CircleBox::try_create(Expr expr, BoxInputFlags opts) {
  CircleBox *box = new CircleBox;
  
  if(!box->try_load_from_object(expr, opts)) {
    delete box;
    return nullptr;
  }
  
  return box;
}

void CircleBox::find_extends(GraphicsBounds &bounds) {
  DoublePoint p1 {cx - rx, cy - ry};
  DoublePoint p2 {cx - rx, cy + ry};
  DoublePoint p3 {cx + rx, cy - ry};
  DoublePoint p4 {cx + rx, cy + ry};
  cairo_matrix_transform_point(&bounds.elem_to_container, &p1.x, &p1.y);
  cairo_matrix_transform_point(&bounds.elem_to_container, &p2.x, &p2.y);
  cairo_matrix_transform_point(&bounds.elem_to_container, &p3.x, &p3.y);
  cairo_matrix_transform_point(&bounds.elem_to_container, &p4.x, &p4.y);
  
  if(bounds.contains(p1) && bounds.contains(p2) && bounds.contains(p3) && bounds.contains(p4))
    return;
  
  auto oldmat = bounds.elem_to_container;
  if(Impl(*this).transform_to_std_circle(bounds.elem_to_container)) {
    double t1 = atan2(bounds.elem_to_container.xy, bounds.elem_to_container.xx); // x'(t1) = 0
    double t2 = atan2(bounds.elem_to_container.yy, bounds.elem_to_container.yx); // y'(t2) = 0
    
    double c1 = cos(t1);
    double s1 = sin(t1);
    double c2 = cos(t2);
    double s2 = sin(t2);
    
    if(angles.length() >= TwoPi) {
      bounds.add_point( c1,  s1); // at t1
      bounds.add_point(-c1, -s1); // at t1 + pi
      bounds.add_point( c2,  s2); // at t2
      bounds.add_point(-c2, -s2); // at t2 + pi
    }
    else {
      if(angles.contains(        t1 < 0     ? t1 + TwoPi : t1))  bounds.add_point( c1,  s1); // at t1
      if(angles.contains(M_PI + (t1 < -M_PI ? t1 + TwoPi : t1))) bounds.add_point(-c1, -s1); // at t1 + pi
      if(angles.contains(        t2 < 0     ? t2 + TwoPi : t2))  bounds.add_point( c2,  s2); // at t2
      if(angles.contains(M_PI + (t2 < -M_PI ? t2 + TwoPi : t2))) bounds.add_point(-c2, -s2); // at t2 + pi
      
      bounds.add_point(cos(angles.from), sin(angles.from));
      bounds.add_point(cos(angles.to),   sin(angles.to));
    }
  }
  else { // degenerate circle, center already adjusted. TODO: restrict according to angles
    //bounds.add_point(cx, cy);
    bounds.add_point(-rx, -ry);
    bounds.add_point(-rx,  ry);
    bounds.add_point( rx, -ry);
    bounds.add_point( rx,  ry);
  }
  
  bounds.elem_to_container = oldmat;
}

void CircleBox::paint(GraphicsDrawingContext &gc) {
  auto mat = gc.canvas().get_matrix();
  
  cairo_matrix_t std_circle_mat = mat;
  if(Impl(*this).transform_to_std_circle(std_circle_mat)) {
    gc.canvas().set_matrix(std_circle_mat);
    gc.canvas().arc(0, 0, 1, angles.from, angles.to, false);
  }
  else { // one or both of the radii are zero. TODO: restrict according to angles
    gc.canvas().move_to(cx - rx, cy - ry);
    gc.canvas().line_to(cx + rx, cy + ry);
    gc.canvas().close_path();
  }
  
  gc.canvas().set_matrix(gc.initial_matrix());
  gc.canvas().stroke();
  gc.canvas().set_matrix(mat);
}

Expr CircleBox::to_pmath_impl(BoxOutputFlags flags) {
  return _expr;
}

//} ... class CircleBox

//{ class CircleBox::Impl ...

CircleBox::Impl::Impl(CircleBox &self)
: self{self}
{
}

// Transform a matrix so that the circle/ellipse befor transformation with center (cx,cy) and radii (rx,ry)
// is a normal circle with radius 1 and center (0,0) w.r.t. the new matrix.
// If the radii are degenerate, only the center gets transformed and false is returned.
bool CircleBox::Impl::transform_to_std_circle(cairo_matrix_t &mat) {
  cairo_matrix_translate(&mat, self.cx, self.cy);
  if(self.rx > 0 && self.ry > 0) {
    cairo_matrix_scale(&mat, self.rx, self.ry);
    return true;
  }
  return false;
}

//} ... class CircleBox::Impl
