#ifndef RICHMATH__BOXES__GRAPHICS__GRAPHICSSTYLEBOX_H__INCLUDED
#define RICHMATH__BOXES__GRAPHICS__GRAPHICSSTYLEBOX_H__INCLUDED

#include <boxes/graphics/graphicselement.h>
#include <graphics/canvas.h>
#include <eval/partial-dynamic.h>


namespace richmath {
  class GraphicsStyleBox: public GraphicsElement {
      using base = GraphicsElement;
      class Impl;
    public:
      static GraphicsElement *create(Expr expr, BoxInputFlags opts) = delete;
      static GraphicsStyleBox *try_create(Expr expr, BoxInputFlags opts);
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      int              count() override {     return _content ? 1 : 0; }
      GraphicsElement *item(int i) override { return _content; }
      
      virtual void find_extends(GraphicsBounds &bounds) override;
      virtual void paint(GraphicsDrawingContext &gc) override;
    
      virtual Style own_style() final override { return _style; };
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
    protected:
      virtual ~GraphicsStyleBox();
      GraphicsStyleBox();
      
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
      GraphicsElement *_content;
      Style            _style;
      PartialDynamic   _dynamic_directives;
      Expr             _latest_directives;
  };
}

#endif // RICHMATH__BOXES__GRAPHICS__GRAPHICSSTYLEBOX_H__INCLUDED
