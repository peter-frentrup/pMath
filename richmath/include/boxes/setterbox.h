#ifndef RICHMATH__BOXES__SETTERBOX_H__INCLUDED
#define RICHMATH__BOXES__SETTERBOX_H__INCLUDED

#include <boxes/buttonbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class SetterBox final : public AbstractButtonBox {
      class Impl;
      using base = AbstractButtonBox;
    public:
      explicit SetterBox(MathSequence *content = nullptr);
      
      // Box::try_create<SetterBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual ControlState calc_state(Context &context) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void reset_style() override;
      
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void click() override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
    
    protected:
      virtual ContainerType default_container_type() override { return ContainerType::PaletteButton; }
    
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::SetterBox; }
    
    protected:
      enum {
        IsInitializedBit = base::NumFlagsBits,
        MustUpdateBit,
        IsDownBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool is_initialized() {            return get_flag(IsInitializedBit); }
      void is_initialized(bool value) {      change_flag(IsInitializedBit, value); }
      bool must_update() {               return get_flag(MustUpdateBit); }
      void must_update(bool value) {         change_flag(MustUpdateBit, value); }
      bool is_down() {                   return get_flag(IsDownBit); }
      void is_down(bool value) {             change_flag(IsDownBit, value); }
      
    private:
      Dynamic dynamic;
      Expr    value;
  };
};

#endif // RICHMATH__BOXES__SETTERBOX_H__INCLUDED
