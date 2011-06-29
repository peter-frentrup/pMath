#ifndef __BOXES__INTERPRETATIONBOX_H__
#define __BOXES__INTERPRETATIONBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class InterpretationBox: public OwnerBox{
    public:
      InterpretationBox();
      InterpretationBox(MathSequence *content);
      InterpretationBox(MathSequence *content, Expr _interpretation);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_INTERPRETATIONBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual bool edit_selection(Context *context);
      
    public:
      Expr interpretation;
  };
}

#endif // __BOXES__INTERPRETATIONBOX_H__
