#include <util/double-point.h>

#include <cmath>
#include <limits>


#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

using namespace richmath;

extern pmath_symbol_t richmath_System_List;

//{ class DoublePoint ...

bool DoublePoint::load_point(DoublePoint &point, Expr coords) {
  if(coords[0] != richmath_System_List)
    return false;
    
  if(coords.expr_length() != 2)
    return false;
    
  point.x = coords[1].to_double(NAN);
  point.y = coords[2].to_double(NAN);
  
  return !std::isnan(point.x) && !std::isnan(point.y);
}

bool DoublePoint::load_point_or_points(DoubleMatrix &points, Expr coords) {
  DoublePoint pt;
  if(load_point(pt, coords)) {
    WriteableDoubleMatrix the_point {1, 2};
    if(the_point) {
      the_point.set(0, 0, pt.x);
      the_point.set(0, 1, pt.y);
      points = PMATH_CPP_MOVE(the_point);
      return true;
    }
    return false;
  }
  
  return load_line(points, PMATH_CPP_MOVE(coords));
}

bool DoublePoint::load_line(DoubleMatrix &line, Expr coords) {
  if(coords.is_packed_array()) {
    line = DoubleMatrix::const_from_expr(coords);
    if(line) {
      if(line.cols() != 2) {
        line = DoubleMatrix{};
        return false;
      }
      
      return true;
    }
  }
  
  if(coords[0] != richmath_System_List)
    return false;
  
  size_t rows = coords.expr_length();
  WriteableDoubleMatrix new_points{rows, 2};
  if(new_points.is_empty() && rows > 0)
    return false;
  
  DoublePoint pt;
  for(size_t i = 0; i < rows; ++i) {
    if(!load_point(pt, coords[i + 1])) {
      line = DoubleMatrix{};
      return false;
    }
    
    new_points.set(i, 0, pt.x);
    new_points.set(i, 1, pt.y);
  }
  
  line = PMATH_CPP_MOVE(new_points);
  return true;
}

bool DoublePoint::load_line_or_lines(Array< DoubleMatrix > &lines, Expr coords) {
  if(coords[0] != richmath_System_List)
    return false;
    
  lines.length(1);
  if(load_line(lines[0], coords))
    return true;
    
  lines.length((int)coords.expr_length());
  for(int i = 0; i < lines.length(); ++i) {
    if(!load_line(lines[i], coords[i + 1])) {
      lines.length(0);
      return false;
    }
  }
  
  return true;
}

//} ... class DoublePoint
