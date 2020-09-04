#ifndef RICHMATH__BOXES__GRAPHICS__COLORBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__COLORBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>

#include <graphics/color.h>


namespace richmath {
  class ColorBox: public GraphicsElement {
    friend class Box;
    protected:
      virtual ~ColorBox();
    public:
      static ColorBox *create(Expr expr, BoxInputFlags opts); // may return nullptr
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsBoxContext *context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      Color _color;
      
      ColorBox();
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__COLORBOX_H__INCLUDED
