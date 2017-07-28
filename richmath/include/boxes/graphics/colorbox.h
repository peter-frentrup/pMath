#ifndef __BOXES__GRAPHICS__COLORBOX_H__
#define __BOXES__GRAPHICS__COLORBOX_H__

#include <boxes/graphics/graphicselement.h>


namespace richmath {
  class ColorBox: public GraphicsElement {
    friend class Box;
    public:
      static ColorBox *create(Expr expr, BoxOptions opts); // may return nullptr
      virtual ~ColorBox();
      
      virtual bool try_load_from_object(Expr expr, BoxOptions opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsBoxContext *context) override;
      
      virtual Expr to_pmath(BoxFlags flags) override;
      
    protected:
      int _color;
      
      ColorBox(int color = 0);
  };
}

#endif // __BOXES__GRAPHICS__COLORBOX_H__
