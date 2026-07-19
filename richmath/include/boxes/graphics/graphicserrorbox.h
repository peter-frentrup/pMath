#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSERRORBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSERRORBOX_H__INCLUDED


#include <boxes/graphics/graphicselement.h>

namespace richmath {
  class GraphicsErrorBox: public GraphicsElement {
      using base = GraphicsElement;
    public:
      GraphicsErrorBox(Expr expr, Expr error_message);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      static Expr message_badhead(Expr expr);
      static Expr message_badarg(Expr expr);
      static Expr message_argxxx(Expr expr, size_t min, size_t max);
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      
      virtual void paint(GraphicsDrawingContext &gc) override;
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override { return _expr; }
      
    private:
      Expr _expr;
      Expr _error_message;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSERRORBOX_H__INCLUDED
