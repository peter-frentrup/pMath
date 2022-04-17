#include <graphics/rectangle.h>
#include <graphics/canvas.h>

#include <cmath>


#ifdef min
  #undef min
#endif


extern pmath_symbol_t richmath_System_List;

using namespace richmath;

static float min(float a, float b) {
  return (a < b) ? a : b;
}

float richmath::round_directed(float x, int direction, bool to_half) {
  if(to_half)
    x += 0.5f;
    
  if(direction < 0)
    x = floorf(x);
  else if(direction > 0)
    x = ceilf(x);
  else
    x = ceilf(x - 0.5);
    
  if(to_half)
    x -= 0.5f;
    
  return x;
}

//{ class Vector2F ...

float Vector2F::length() {
  return hypot(x, y);
}

void Vector2F::pixel_align_distance(Canvas &canvas) {
  if(canvas.pixel_device) {
    cairo_matrix_t ctm = canvas.get_matrix();
    
    if( (ctm.xx == 0 && ctm.yy == 0) ||
        (ctm.xy == 0 && ctm.yx == 0))
    {
      canvas.user_to_device_dist(&x, &y);
      
      bool x_was_zero = fabs(x) < 1e-5;
      bool y_was_zero = fabs(y) < 1e-5;
      
      x = round(x);
      y = round(y);
      
      if(!x_was_zero) {
        if(x >= 0 && x < 1)
          x = 1;
        else if(x <= 0 && x > -1)
          x = -1;
      }
      
      if(!y_was_zero) {
        if(y >= 0 && y < 1)
          y = 1;
        else if(y <= 0 && y > -1)
          y = -1;
      }
      
      canvas.device_to_user_dist(&x, &y);
    }
  }
}

//} ... class Vector2F

//{ class Point ...

void Point::pixel_align_point(Canvas &canvas, bool tostroke) {
  if(canvas.pixel_device) {
    cairo_matrix_t ctm = canvas.get_matrix();
    
    if( (ctm.xx == 0 && ctm.yy == 0) ||
        (ctm.xy == 0 && ctm.yx == 0))
    {
      canvas.user_to_device(&x, &y);
      if(tostroke) {
        x = ceil(x) - 0.5;
        y = ceil(y) - 0.5;
      }
      else {
        x = round(x);
        y = round(y);
      }
      canvas.device_to_user(&x, &y);
    }
  }
}

//} ... class Point

//{ class RectangleF ...

void RectangleF::normalize() {
  if(width < 0) {
    x = x + width;
    width = -width;
  }
  
  if(height < 0) {
    y = y + height;
    height = -height;
  }
}

void RectangleF::normalize_to_zero() {
  if(width < 0) {
    x += width / 2;
    width = 0;
  }
  
  if(height < 0) {
    y += height / 2;
    height = 0;
  }
}

void RectangleF::pixel_align(Canvas &canvas, bool tostroke, int direction) {
  if(canvas.pixel_device) {
    cairo_matrix_t ctm = canvas.get_matrix();
    
    /* only align to pixel boundaries,
       if the rectangle is rotated by 0°, 90°, 180° or 270° */
    if( (ctm.xx == 0 && ctm.yy == 0) ||
        (ctm.xy == 0 && ctm.yx == 0))
    {
      float x1 = x;
      float y1 = y;
      float x2 = x + width;
      float y2 = y + height;
      
      canvas.user_to_device(&x1, &y1);
      canvas.user_to_device(&x2, &y2);
      
      float new_x1 = round_directed(x1, x1 < x2 ? -direction :  direction, tostroke);
      float new_y1 = round_directed(y1, y1 < y2 ? -direction :  direction, tostroke);
      float new_x2 = round_directed(x2, x1 < x2 ?  direction : -direction, tostroke);
      float new_y2 = round_directed(y2, y1 < y2 ?  direction : -direction, tostroke);
      
//      if(x1 == x2)
//        x2+= 1;
//      if(y1 == y2)
//        y2+= 1;

      canvas.device_to_user(&new_x1, &new_y1);
      canvas.device_to_user(&new_x2, &new_y2);
      
      x      = new_x1;
      y      = new_y1;
      width  = new_x2 - new_x1;
      height = new_y2 - new_y1;
    }
  }
}

void RectangleF::grow(float dx, float dy) {
  x -= dx;
  y -= dy;
  
  width += 2 * dx;
  height += 2 * dy;
}

void RectangleF::grow(Side side, float delta) {
  switch(side) {
    case Side::Left:
      x-= delta;
      width+= delta;
      break;
    case Side::Right:
      width+= delta;
      break;
    case Side::Top:
      y-= delta;
      height+= delta;
      break;
    case Side::Bottom:
      height+= delta;
      break;
  }
}

void RectangleF::add_round_rect_path(Canvas &canvas, const BoxRadius &radii, bool negative) const {
  canvas.new_sub_path();
  
  if(negative) {
    canvas.ellipse_arc(
      x          + radii.bottom_left_x,
      y + height - radii.bottom_left_y,
      radii.bottom_left_x,
      radii.bottom_left_y,
      M_PI,
      M_PI / 2.0,
      true);
      
    canvas.ellipse_arc(
      x + width  - radii.bottom_right_x,
      y + height - radii.bottom_right_y,
      radii.bottom_right_x,
      radii.bottom_right_y,
      M_PI / 2.0,
      0.0,
      true);
      
    canvas.ellipse_arc(
      x + width - radii.top_right_x,
      y         + radii.top_right_y,
      radii.top_right_x,
      radii.top_right_y,
      0.0,
      -M_PI / 2.0,
      true);
      
    canvas.ellipse_arc(
      x + radii.top_left_x,
      y + radii.top_left_y,
      radii.top_left_x,
      radii.top_left_y,
      -M_PI / 2.0,
      -M_PI,
      true);
  }
  else {
    canvas.ellipse_arc(
      x + radii.top_left_x,
      y + radii.top_left_y,
      radii.top_left_x,
      radii.top_left_y,
      -M_PI,
      -M_PI / 2.0,
      false);
      
    canvas.ellipse_arc(
      x + width - radii.top_right_x,
      y         + radii.top_right_y,
      radii.top_right_x,
      radii.top_right_y,
      -M_PI / 2.0,
      0.0,
      false);
      
    canvas.ellipse_arc(
      x + width  - radii.bottom_right_x,
      y + height - radii.bottom_right_y,
      radii.bottom_right_x,
      radii.bottom_right_y,
      0.0,
      M_PI / 2.0,
      false);
      
    canvas.ellipse_arc(
      x          + radii.bottom_left_x,
      y + height - radii.bottom_left_y,
      radii.bottom_left_x,
      radii.bottom_left_y,
      M_PI / 2.0,
      M_PI,
      false);
  }
  
  canvas.close_path();
}

void RectangleF::add_rect_path(Canvas &canvas, bool negative) const {
  add_round_rect_path(canvas, BoxRadius(), negative);
}

//} ... class RectangleF

//{ class BorderRadius ...

BoxRadius::BoxRadius(float all)
  : top_left_x(    all),
    top_left_y(    all),
    top_right_x(   all),
    top_right_y(   all),
    bottom_right_x(all),
    bottom_right_y(all),
    bottom_left_x( all),
    bottom_left_y( all)
{
}

BoxRadius::BoxRadius(float all_x, float all_y)
  : top_left_x(    all_x),
    top_left_y(    all_y),
    top_right_x(   all_x),
    top_right_y(   all_y),
    bottom_right_x(all_x),
    bottom_right_y(all_y),
    bottom_left_x( all_x),
    bottom_left_y( all_y)
{
}

BoxRadius::BoxRadius(float tl, float tr, float br, float bl)
  : top_left_x(    tl),
    top_left_y(    tl),
    top_right_x(   tr),
    top_right_y(   tr),
    bottom_right_x(br),
    bottom_right_y(br),
    bottom_left_x( bl),
    bottom_left_y( bl)
{
}

BoxRadius::BoxRadius(float tl_x, float tl_y, float tr_x, float tr_y, float br_x, float br_y, float bl_x, float bl_y)
  : top_left_x(    tl_x),
    top_left_y(    tl_y),
    top_right_x(   tr_x),
    top_right_y(   tr_y),
    bottom_right_x(br_x),
    bottom_right_y(br_y),
    bottom_left_x( bl_x),
    bottom_left_y( bl_y)
{
}

BoxRadius::BoxRadius(const Expr &expr)
  : top_left_x(    0),
    top_left_y(    0),
    top_right_x(   0),
    top_right_y(   0),
    bottom_right_x(0),
    bottom_right_y(0),
    bottom_left_x( 0),
    bottom_left_y( 0)
{
  if(expr.is_number()) {
    *this = BoxRadius(expr.to_double(0));
    return;
  }
  
  if(expr[0] != richmath_System_List)
    return;
    
  if(expr.expr_length() == 1) { // {all}
    *this = BoxRadius(expr[1].to_double(0));
    return;
  }
  
  if(expr.expr_length() == 2) { // {allx, ally}
    *this = BoxRadius(expr[1].to_double(0), expr[2].to_double(0));
    return;
  }
  
  if(expr.expr_length() == 4) { // {tl, tr, br, bl} or {{tlx, tly}, ...}
  
    Expr tmp = expr[1];
    if(tmp.is_number()) {
      top_left_x = top_left_y = tmp.to_double(0);
    }
    else if(tmp.expr_length() == 2 && tmp[0] == richmath_System_List) {
      top_left_x = tmp[1].to_double(0);
      top_left_y = tmp[2].to_double(0);
    }
    
    tmp = expr[2];
    if(tmp.is_number()) {
      top_right_x = top_right_y = tmp.to_double(0);
    }
    else if(tmp.expr_length() == 2 && tmp[0] == richmath_System_List) {
      top_right_x = tmp[1].to_double(0);
      top_right_y = tmp[2].to_double(0);
    }
    
    tmp = expr[3];
    if(tmp.is_number()) {
      bottom_right_x = bottom_right_y = tmp.to_double(0);
    }
    else if(tmp.expr_length() == 2 && tmp[0] == richmath_System_List) {
      bottom_right_x = tmp[1].to_double(0);
      bottom_right_y = tmp[2].to_double(0);
    }
    
    tmp = expr[4];
    if(tmp.is_number()) {
      bottom_left_x = bottom_left_y = tmp.to_double(0);
    }
    else if(tmp.expr_length() == 2 && tmp[0] == richmath_System_List) {
      bottom_left_x = tmp[1].to_double(0);
      bottom_left_y = tmp[2].to_double(0);
    }
    
  }
}

BoxRadius &BoxRadius::operator+=(const BoxRadius &other) {
  top_left_x     += other.top_left_x;
  top_left_y     += other.top_left_y;
  top_right_x    += other.top_right_x;
  top_right_y    += other.top_right_y;
  bottom_right_x += other.bottom_right_x;
  bottom_right_y += other.bottom_right_y;
  bottom_left_x  += other.bottom_left_x;
  bottom_left_y  += other.bottom_left_y;
  
  return *this;
}

void BoxRadius::normalize(float max_width, float max_height) {
  if(top_left_x     < 0) top_left_x     = 0;
  if(top_left_y     < 0) top_left_y     = 0;
  if(top_right_x    < 0) top_right_x    = 0;
  if(top_right_y    < 0) top_right_y    = 0;
  if(bottom_right_x < 0) bottom_right_x = 0;
  if(bottom_right_y < 0) bottom_right_y = 0;
  if(bottom_left_x  < 0) bottom_left_x  = 0;
  if(bottom_left_y  < 0) bottom_left_y  = 0;
  
  float f = 1;
  if(top_left_x + top_right_x > max_width)
    f = min(f, max_width / (top_left_x + top_right_x));
    
  if(bottom_left_x + bottom_right_x > max_width)
    f = min(f, max_width / (bottom_left_x + bottom_right_x));
    
  if(top_left_y + bottom_left_y > max_height)
    f = min(f, max_height / (top_left_y + bottom_left_y));
    
  if(top_right_y + bottom_right_y > max_height)
    f = min(f, max_height / (top_right_y + bottom_right_y));
    
  if(f < 1) {
    top_left_x     *= f;
    top_left_y     *= f;
    top_right_x    *= f;
    top_right_y    *= f;
    bottom_right_x *= f;
    bottom_right_y *= f;
    bottom_left_x  *= f;
    bottom_left_y  *= f;
  }
}

//} ... class BorderRadius
