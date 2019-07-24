#ifndef RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED

#include <boxes/graphics/pointbox.h>
#include <util/double-matrix.h>


namespace richmath {
  class LineBox: public GraphicsElement {
    public:
      static LineBox *create(Expr expr, BoxInputFlags opts); // may return nullptr
      virtual ~LineBox();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsBoxContext *context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      Expr                _uncompressed_expr;
      Array<DoubleMatrix> _lines;
      
    protected:
      LineBox();
  };
}


#endif // RICHMATH__BOXES__GRAPHICS__LINEBOX_H__INCLUDED
