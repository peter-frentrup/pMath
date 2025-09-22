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
      
    protected:
      Expr         _points_expr;
      DoubleMatrix _points;
      
    protected:
      BezierCurveBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__BEZIERCURVEBOX_H__INCLUDED
