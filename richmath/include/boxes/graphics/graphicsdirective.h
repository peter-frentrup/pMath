#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED


#include <boxes/graphics/graphicselement.h>
#include <eval/partial-dynamic.h>

namespace richmath {
  class Context;
  
  class GraphicsDirective final : public GraphicsDirectiveBase {
      using base = GraphicsDirectiveBase;
      class Impl;
    public:
      static bool is_graphics_directive(Expr expr);
      static void apply(Expr directive, Context &context);
      static void apply(Expr directive, GraphicsDrawingContext &gc);
      static void apply_thickness(Length thickness, Context &context);
      static void apply_thickness(Length thickness, GraphicsDrawingContext &gc);
      
      static GraphicsDirective *try_create(Expr expr, BoxInputFlags opts);
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsDrawingContext &gc) override;
      
      virtual SharedPtr<Style> own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override { return _dynamic.expr(); }
      
    private:
      GraphicsDirective();
      GraphicsDirective(Expr expr);
    
    private:
      enum {
        MustUpdateBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
    
      bool must_update() {       return get_flag(MustUpdateBit); }
      void must_update(bool value) { change_flag(MustUpdateBit, value); }
      
    private:
      SharedPtr<Style> _style;
      PartialDynamic   _dynamic;
      Expr             _latest_directives;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
