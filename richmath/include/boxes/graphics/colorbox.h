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
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static ColorBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsBox *owner, Context &context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      Color _color;
      
      ColorBox();
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__COLORBOX_H__INCLUDED
