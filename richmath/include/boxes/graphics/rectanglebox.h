#ifndef RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <util/double-point.h>

namespace richmath {
  class RectangleBox final : public GraphicsElement {
      class Impl;
    protected:
      virtual ~RectangleBox();
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static RectangleBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
    protected:
      Expr _expr;
      DoublePoint p0;
      DoublePoint p1;
      
    protected:
      RectangleBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED
