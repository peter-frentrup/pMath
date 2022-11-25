#ifndef RICHMATH__GRAPHICS__DOUBLE_POINT_H__INCLUDED
#define RICHMATH__GRAPHICS__DOUBLE_POINT_H__INCLUDED

#include <util/pmath-extra.h>
#include <util/double-matrix.h>

namespace richmath {
  class DoublePoint {
    public:
      DoublePoint()
        : x(0), y(0)
      {
      }
      
      DoublePoint(double _x, double _y)
        : x(_x), y(_y)
      {
      }
      
    public:
      static bool load_point(          DoublePoint  &point,  Expr coords);
      static bool load_point_or_points(DoubleMatrix &points, Expr coords);
      
      static bool load_line(               DoubleMatrix  &line,   Expr coords);
      static bool load_line_or_lines(Array<DoubleMatrix> &lines, Expr coords);
      
    public:
      double x;
      double y;
  };
  
}

#endif // RICHMATH__GRAPHICS__DOUBLE_POINT_H__INCLUDED
