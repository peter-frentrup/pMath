#ifndef __BOXES__GRAPHICS__LINEBOX_H__
#define __BOXES__GRAPHICS__LINEBOX_H__

#include <boxes/graphics/graphicselement.h>


namespace richmath {
  class LineBox: public GraphicsElement {
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
      static LineBox *create(Expr expr, int opts); // may return NULL
      virtual ~LineBox();
      
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void find_extends(GraphicsBounds &bounds);
      virtual void paint(GraphicsBoxContext *context);
      
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
    protected:
      Expr                  _expr;
      Array< Array<Point> > _lines;
      
    protected:
      LineBox();
  };
}


#endif // __BOXES__GRAPHICS__LINEBOX_H__
