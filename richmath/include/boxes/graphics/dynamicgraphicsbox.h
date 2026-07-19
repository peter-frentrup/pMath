#ifndef RICHMATH__BOXES__GRAPHICS__DYNAMICGRAPHICSBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__DYNAMICGRAPHICSBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <eval/dynamic.h>


namespace richmath {
  class DynamicGraphicsBox: public GraphicsElement {
      using base = GraphicsElement;
      class Impl;
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static GraphicsElement *create_or_error(Expr expr, BoxInputFlags opts);
      static DynamicGraphicsBox *try_create(Expr expr, BoxInputFlags opts);
    
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      int              count() override {     return _content ? 1 : 0; }
      GraphicsElement *item(int i) override { return _content; }
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
      virtual Style own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
    protected:
      virtual ~DynamicGraphicsBox();
      DynamicGraphicsBox();
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
    protected:
      enum {
        MustUpdateBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
    
      bool must_update() {       return get_flag(MustUpdateBit); }
      void must_update(bool value) { change_flag(MustUpdateBit, value); }
      
    private:
      Dynamic          dynamic;
      GraphicsElement *_content;
      Style            _style;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__DYNAMICGRAPHICSBOX_H__INCLUDED
