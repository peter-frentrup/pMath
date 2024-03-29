#ifndef RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <util/double-matrix.h>


namespace richmath {
  class LineBox final : public GraphicsElement {
    protected:
      virtual ~LineBox();
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static LineBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
      
    protected:
      Expr                _uncompressed_expr;
      Array<DoubleMatrix> _lines;
      
    protected:
      LineBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
  };
}


#endif // RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED
