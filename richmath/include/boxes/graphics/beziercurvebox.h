#ifndef RICHMATH__BOXES__GRAPHICS__BEZIERCURVEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__BEZIERCURVEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <util/double-matrix.h>


namespace richmath {
  class BezierCurveBox: public GraphicsElement {
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static BezierCurveBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
      
      virtual Style own_style() override { return style; }
      
      virtual Expr update_cause() final override;
      virtual void update_cause(Expr cause) final override;
      
    protected:
      Expr         _points_expr;
      DoubleMatrix _points;
      Style        style;
      
    protected:
      BezierCurveBox();
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::BezierSplineBox; }
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__BEZIERCURVEBOX_H__INCLUDED
