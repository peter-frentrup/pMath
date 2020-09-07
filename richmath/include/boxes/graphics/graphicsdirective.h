#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED


#include <boxes/graphics/graphicselement.h>

namespace richmath {
  class GraphicsDirective: public GraphicsElement {
      using base = GraphicsElement;
    public:
      static bool is_graphics_directive(Expr expr);
      static void apply(Expr directive, Context &context);
      
      static GraphicsDirective *try_create(Expr expr, BoxInputFlags opts);
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsBox *owner, Context &context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override { return _expr; }
      
    protected:
      Expr _expr;
      
      GraphicsDirective();
      GraphicsDirective(Expr expr);
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
