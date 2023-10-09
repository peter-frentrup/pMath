#ifndef RICHMATH__BOXES__GRAPHICS__CIRCLEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__CIRCLEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <util/interval.h>

namespace richmath {
  class CircleOrDiskBox final : public GraphicsElement {
      class Impl;
    protected:
      virtual ~CircleOrDiskBox();
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static CircleOrDiskBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
    protected:
      Expr _expr;
      double cx;
      double cy;
      double rx;
      double ry;
      Interval<double> angles;
      
    protected:
      CircleOrDiskBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__CIRCLEBOX_H__INCLUDED
