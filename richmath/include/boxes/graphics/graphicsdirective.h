#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED


#include <boxes/graphics/graphicselement.h>
#include <eval/partial-dynamic.h>

namespace richmath {
  class GraphicsDirective final : public GraphicsDirectiveBase {
      using base = GraphicsDirectiveBase;
      class Impl;
    public:
      static bool is_graphics_directive(Expr expr);
      static void apply(Expr directive, Context &context);
      
      static GraphicsDirective *try_create(Expr expr, BoxInputFlags opts);
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsBox *owner, Context &context) override;
      
      virtual Expr to_pmath(BoxOutputFlags flags) override { return _dynamic.expr(); }
      
      virtual SharedPtr<Style> own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
    
    private:
      GraphicsDirective();
      GraphicsDirective(Expr expr);
      
    private:
      SharedPtr<Style> _style;
      PartialDynamic   _dynamic;
      Expr             _latest_directives;
      bool             _must_update : 1;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
