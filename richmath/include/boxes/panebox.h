#ifndef RICHMATH__BOXES__PANEBOX_H__INCLUDED
#define RICHMATH__BOXES__PANEBOX_H__INCLUDED

#include <boxes/transformationbox.h>

namespace richmath {
  class PaneBox : public AbstractTransformationBox {
      using base = AbstractTransformationBox;
      class Impl;
    protected:
      virtual ~PaneBox();
    public:
      explicit PaneBox();
      
      virtual void reset_style() override;
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void paint_content(Context &context) override;
    
    protected:
      virtual void resize_default_baseline(Context &context) override;
      virtual float allowed_content_width(const Context &context) override;
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::PaneBox; }
  };
}

#endif // RICHMATH__BOXES__PANEBOX_H__INCLUDED
