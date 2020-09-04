#ifndef RICHMATH__BOXES__GRAPHICS__POINTBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__POINTBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
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
  
  class PointBox: public GraphicsElement {
    protected:
      virtual ~PointBox();
    public:
      static PointBox *create(Expr expr, BoxInputFlags opts); // may return nullptr
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsBox *owner, Context &context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      Expr         _uncompressed_expr;
      DoubleMatrix _points;
      
    protected:
      PointBox();
  };
}


#endif // RICHMATH__BOXES__GRAPHICS__POINTBOX_H__INCLUDED
