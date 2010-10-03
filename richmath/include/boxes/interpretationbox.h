#ifndef __BOXES__INTERPRETATIONBOX_H__
#define __BOXES__INTERPRETATIONBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class InterpretationBox: public OwnerBox{
    public:
      InterpretationBox();
      InterpretationBox(MathSequence *content);
      InterpretationBox(MathSequence *content, Expr _interpretation);
      
      virtual Expr to_pmath(bool parseable);
      
      virtual bool edit_selection(Context *context);
      
    public:
      Expr interpretation;
  };
}

#endif // __BOXES__INTERPRETATIONBOX_H__
