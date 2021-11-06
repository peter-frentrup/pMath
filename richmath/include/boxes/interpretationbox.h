#ifndef RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED
#define RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED

#include <boxes/stylebox.h>


namespace richmath {
  class InterpretationBox final : public AbstractStyleBox {
      using base = AbstractStyleBox;
    public:
      explicit InterpretationBox(AbstractSequence *content);
      explicit InterpretationBox(AbstractSequence *content, Expr _interpretation);
      
      virtual MathSequence *as_inline_span() override;
      virtual void reset_style() override;
      
      // Box::try_create<InterpretationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual bool edit_selection(SelectionReference &selection) override;
      
    public:
      Expr interpretation;
  };
}

#endif // RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED
