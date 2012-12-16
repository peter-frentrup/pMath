#ifndef __BOXES__GRAPHICS__LINEBOX_H__
#define __BOXES__GRAPHICS__LINEBOX_H__

#include <boxes/graphics/graphicselement.h>


namespace richmath {
  // handles PointBox and LineBox
  class PointSetBox: public GraphicsElement {
    public:
      struct Point {
        Point()
          : x(0), y(0)
        {
        }
        
        Point(double _x, double _y)
          : x(_x), y(_y)
        {
        }
        
        double x;
        double y;
      };
      
    public:
      static PointSetBox *create(Expr expr, int opts); // may return NULL
      virtual ~PointSetBox();
      
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void find_extends(GraphicsBounds &bounds);
      virtual void paint(GraphicsBoxContext *context);
      
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
      bool is_drawing_points() { return (_uncompressed_expr[0] == PMATH_SYMBOL_POINTBOX); }
      bool is_drawing_lines()  { return (_uncompressed_expr[0] == PMATH_SYMBOL_LINEBOX); }
      
    protected:
      Expr                  _uncompressed_expr;
      Array< Array<Point> > _lines;
      
    protected:
      PointSetBox();
      
      
  };
}


#endif // __BOXES__GRAPHICS__LINEBOX_H__
