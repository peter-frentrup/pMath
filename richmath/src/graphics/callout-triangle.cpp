#include <graphics/callout-triangle.h>

#include <algorithm>


using namespace richmath;

//{ class CalloutTriangle ...

CalloutTriangle CalloutTriangle::ForBasePointingToTarget(const Interval<float> &base, const Interval<float> &target, float tip_size) {
  auto common = base.intersect(target);
  
  int triangle_base_direction = 1; // -1 = /|   0 = /\   1 = |\  .
  if(common.to < base.from + base.length() / 3)
    triangle_base_direction = 1;
  else if(common.from > base.to - base.length() / 3)
    triangle_base_direction = -1;
  else
    triangle_base_direction = 0;
  
  CalloutTriangle tri;
  tri.y_tip_height = (triangle_base_direction == 0) ? tip_size - tip_size/4 : tip_size;
  
  tri.x_tip_projection = common.from + common.length() / 2;
  Interval<float> triangle_base {
    tri.x_tip_projection - (triangle_base_direction <= 0 ? tri.y_tip_height : 0), 
    tri.x_tip_projection + (triangle_base_direction >= 0 ? tri.y_tip_height : 0)};
  
  triangle_base = base.snap(triangle_base);
  
  tri.x_base_from = triangle_base.from;
  tri.x_base_to   = triangle_base.to;
  
  return tri;
}

CalloutTriangle CalloutTriangle::ForSideOfBasePointingToTarget(const RectangleF &base, Side side, const RectangleF &target, float tip_size, bool relative) {
  CalloutTriangle tri;
  switch(side) {
    case Side::Left:
    case Side::Right:
      tri = CalloutTriangle::ForBasePointingToTarget(base.y_interval(), target.y_interval(), tip_size);
      if(relative)
        tri.offset_x(-base.y);
      break;
    
    case Side::Top:
    case Side::Bottom:
      tri = CalloutTriangle::ForBasePointingToTarget(base.x_interval(), target.x_interval(), tip_size);
      if(relative)
        tri.offset_x(-base.x);
      break;
  }
  return tri;
}

void CalloutTriangle::offset_x(float dx) {
  x_base_from      += dx;
  x_base_to        += dx;
  x_tip_projection += dx;
}

void CalloutTriangle::swap_x_base() {
  using std::swap;
  swap(x_base_from, x_base_to);
}

void CalloutTriangle::get_triangle_points(Point out_points[3], const RectangleF &base, Side side) const {
  switch(side) {
    case Side::Left:
    case Side::Right:
      out_points[0].y = x_base_from;
      out_points[1].y = x_tip_projection;
      out_points[2].y = x_base_to;
      break;
    
    case Side::Top:
    case Side::Bottom:
      out_points[0].x = x_base_from;
      out_points[1].x = x_tip_projection;
      out_points[2].x = x_base_to;
      break;
  }

  switch(side) {
    case Side::Left:   out_points[0].x = out_points[1].x = out_points[2].x = base.left();   break;
    case Side::Right:  out_points[0].x = out_points[1].x = out_points[2].x = base.right();  break;
    case Side::Top:    out_points[0].y = out_points[1].y = out_points[2].y = base.top();    break;
    case Side::Bottom: out_points[0].y = out_points[1].y = out_points[2].y = base.bottom(); break;
  }
  
  switch(side) {
    case Side::Left:   out_points[1].x -= y_tip_height; break;
    case Side::Right:  out_points[1].x += y_tip_height; break;
    case Side::Top:    out_points[1].y -= y_tip_height; break;
    case Side::Bottom: out_points[1].y += y_tip_height; break;
  }
}

RectangleF CalloutTriangle::get_basement(const RectangleF &base, Side side, float depth) const {
  switch(side) {
    case Side::Left:   return RectangleF(base.left(),          x_base_from, depth, x_base_to - x_base_from);
    case Side::Right:  return RectangleF(base.right() - depth, x_base_from, depth, x_base_to - x_base_from);
    
    case Side::Top:    return RectangleF(x_base_from, base.top(),            x_base_to - x_base_from, depth);
    case Side::Bottom: return RectangleF(x_base_from, base.bottom() - depth, x_base_to - x_base_from, depth);
  }
  
  return RectangleF();
}

//} ... class CalloutTriangle
