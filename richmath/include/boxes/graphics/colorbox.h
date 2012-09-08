#ifndef __BOXES__GRAPHICS__COLORBOX_H__
#define __BOXES__GRAPHICS__COLORBOX_H__

#include <boxes/graphics/graphicselement.h>


namespace richmath {
  class ColorBox: public GraphicsElement {
    public:
      static ColorBox *create(Expr expr, int opts); // may return 0
      virtual ~ColorBox();
      
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void find_extends(GraphicsBounds &bounds){}
      virtual void paint(GraphicsBoxContext *context);
      
      virtual Expr to_pmath(int flags);
      
    protected:
      int _color;
      
      ColorBox(int color = 0);
  };
}

#endif // __BOXES__GRAPHICS__COLORBOX_H__
