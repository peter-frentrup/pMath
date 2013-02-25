#ifndef __BOXES__GRAPHICS__LINEBOX_H__
#define __BOXES__GRAPHICS__LINEBOX_H__

#include <boxes/graphics/pointbox.h>


namespace richmath {
  class LineBox: public GraphicsElement {
    public:
      static LineBox *create(Expr expr, int opts); // may return NULL
      virtual ~LineBox();
      
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void find_extends(GraphicsBounds &bounds);
      virtual void paint(GraphicsBoxContext *context);
      
      virtual Expr to_pmath(int flags); // BoxFlagXXX
      
    protected:
      Expr                        _uncompressed_expr;
      Array< Array<DoublePoint> > _lines;
      
    protected:
      LineBox();
  };
}


#endif // __BOXES__GRAPHICS__LINEBOX_H__
