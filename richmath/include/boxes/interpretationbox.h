#ifndef __BOXES__INTERPRETATIONBOX_H__
#define __BOXES__INTERPRETATIONBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class InterpretationBox: public OwnerBox {
    public:
      InterpretationBox();
      InterpretationBox(MathSequence *content);
      InterpretationBox(MathSequence *content, Expr _interpretation);
      
      virtual void reset_style();
      
      // Box::try_create<InterpretationBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_INTERPRETATIONBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual bool edit_selection(Context *context);
      
    public:
      Expr interpretation;
  };
}

#endif // __BOXES__INTERPRETATIONBOX_H__
