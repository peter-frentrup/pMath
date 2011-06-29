#ifndef __BOXES__FRAMEBOX_H__
#define __BOXES__FRAMEBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class FrameBox: public OwnerBox{
    public:
      static FrameBox *create(Expr expr, int options);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_FRAMEBOX); }
      virtual Expr to_pmath(int flags);
    
    protected:
      explicit FrameBox(MathSequence *content = 0);
      
    protected:
      float em;
  };
}

#endif // __BOXES__FRAMEBOX_H__
