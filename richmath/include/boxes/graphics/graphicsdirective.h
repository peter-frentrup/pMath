#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED


#include <boxes/graphics/graphicselement.h>
#include <graphics/canvas.h>
#include <eval/partial-dynamic.h>

namespace richmath {

  class GraphicsDirective final : public GraphicsDirectiveBase {
      using base = GraphicsDirectiveBase;
      class Impl;
    public:
      static bool is_graphics_directive(Expr expr);
      static void apply(Expr directive, GraphicsDrawingContext &gc);
      
      static GraphicsDirective *try_create(Expr expr, BoxInputFlags opts);
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void find_extends(GraphicsBounds &bounds) override {}
      virtual void paint(GraphicsDrawingContext &gc) override;
      
      virtual Style own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      static bool decode_dashing(Array<double> &dash_array, double &offset, enum CapForm &cap_form, Expr dashing_expr, float scale_factor);
      static bool decode_dash_array(Array<double> &dash_array, Expr dashes, float scale_factor);
      static void enlarge_zero_dashes(Array<double> &dash_array);
      
      static bool decode_joinform(enum JoinForm &join_form, float &miter_limit, Expr expr);
    
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
      Style            _style;
      PartialDynamic   _dynamic;
      Expr             _latest_directives;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSDIRECTIVE_H__INCLUDED
