#ifndef __BOYES__DYNAMICLOCALBOX_H__
#define __BOYES__DYNAMICLOCALBOX_H__

#include <boxes/dynamicbox.h>


namespace richmath {
  class DynamicLocalBox final : public AbstractDynamicBox {
      using base = AbstractDynamicBox;
      class Impl;
    protected:
      virtual ~DynamicLocalBox();
    public:
      DynamicLocalBox();
      
      // Box::try_create<DynamicLocalBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Expr prepare_dynamic(Expr expr) override;
      
    protected:
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::DynamicLocalBox; }

    private:
      Expr _public_symbols;
      Expr _private_symbols;
  };
}

#endif // __BOYES__DYNAMICLOCALBOX_H__
