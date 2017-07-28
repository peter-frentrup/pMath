#ifndef __BOXES__GRAPHICS__POINTBOX_H__
#define __BOXES__GRAPHICS__POINTBOX_H__

#include <boxes/graphics/graphicselement.h>


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
      static bool load_point(                DoublePoint  &point,  Expr coords);
      static bool load_point_or_points(Array<DoublePoint> &points, Expr coords);
      
      static bool load_line(                Array<DoublePoint>   &line,  Expr coords);
      static bool load_line_or_lines(Array< Array<DoublePoint> > &lines, Expr coords);
      
    public:
      double x;
      double y;
  };
  
  class PointBox: public GraphicsElement {
    public:
      static PointBox *create(Expr expr, BoxOptions opts); // may return nullptr
      virtual ~PointBox();
      
      virtual bool try_load_from_object(Expr expr, BoxOptions opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsBoxContext *context) override;
      
      virtual Expr to_pmath(BoxFlags flags) override; // BoxFlagXXX
      
    protected:
      Expr               _uncompressed_expr;
      Array<DoublePoint> _points;
      
    protected:
      PointBox();
  };
}


#endif // __BOXES__GRAPHICS__POINTBOX_H__
