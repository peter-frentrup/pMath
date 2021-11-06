#ifndef RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED
#define RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED

#include <boxes/ownerbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class AbstractDynamicBox: public OwnerBox {
      using base = OwnerBox;
    public:
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      virtual void reset_style() override;
      
      virtual void before_paint_inline(Context &context) override;
      virtual void paint(Context &context) override;
      
    protected:
      explicit AbstractDynamicBox();
      virtual ~AbstractDynamicBox();
      
      void ensure_init();
  };
  
  class DynamicBox final : public AbstractDynamicBox {
      using base = AbstractDynamicBox;
    protected:
      virtual ~DynamicBox();
    public:
      explicit DynamicBox();
      
      // Box::try_create<DynamicBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual MathSequence *as_inline_span() override;
      virtual void before_paint_inline(Context &context) override;
      virtual void paint_content(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      virtual bool edit_selection(SelectionReference &selection) override;
      
    protected:
      virtual void resize_default_baseline(Context &context) override;
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::DynamicBox; }
    
    protected:
      enum {
        MustUpdateBit = base::NumFlagsBits,
        MustResizeBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool must_update() {       return get_flag(MustUpdateBit); }
      void must_update(bool value) { change_flag(MustUpdateBit, value); }
      bool must_resize() {       return get_flag(MustResizeBit); }
      void must_resize(bool value) { change_flag(MustResizeBit, value); }
      
    public:
      Dynamic dynamic;
  };
};

#endif // RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED
