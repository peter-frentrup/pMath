#ifndef __BOXES__GRAPHICS__COLORBOX_H__
#define __BOXES__GRAPHICS__COLORBOX_H__

#include <boxes/graphics/graphicselement.h>


namespace richmath {
  class ColorBox: public GraphicsElement {
    public:
      ColorBox(int color = 0);
      static ColorBox *create(Expr expr, int opts);
      virtual ~ColorBox();
      
      virtual void paint(Context *context);
      
      virtual Expr to_pmath(int flags);
      
    protected:
      int _color;
  };
}

#endif // __BOXES__GRAPHICS__COLORBOX_H__
