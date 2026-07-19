#ifndef RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <eval/partial-dynamic.h>
#include <util/double-point.h>

namespace richmath {
  class RectangleBox final : public GraphicsElement {
      using base = GraphicsElement;
      class Impl;
    protected:
      virtual ~RectangleBox();
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static GraphicsElement *create_or_error(Expr expr, BoxInputFlags opts);
      static RectangleBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
      virtual Style own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
    protected:
      enum {
        MustUpdateBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
    
      bool must_update() {       return get_flag(MustUpdateBit); }
      void must_update(bool value) { change_flag(MustUpdateBit, value); }
      
    protected:
      DoublePoint    p0;
      DoublePoint    p1;
      PartialDynamic _dynamic_args;
      Style          _style;
      
    protected:
      RectangleBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__RECTANGLEBOX_H__INCLUDED
