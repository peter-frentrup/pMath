#ifndef RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED
#define RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class InterpretationBox: public OwnerBox {
    public:
      InterpretationBox();
      InterpretationBox(MathSequence *content);
      InterpretationBox(MathSequence *content, Expr _interpretation);
      
      virtual void reset_style() override;
      
      // Box::try_create<InterpretationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual bool edit_selection(Context *context) override;
      
    public:
      Expr interpretation;
  };
}

#endif // RICHMATH__BOXES__INTERPRETATIONBOX_H__INCLUDED
