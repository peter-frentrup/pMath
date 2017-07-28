#ifndef __BOXES__FRAMEBOX_H__
#define __BOXES__FRAMEBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class FrameBox: public OwnerBox {
    public:
      explicit FrameBox(MathSequence *content = 0);
      
      // Box::try_create<FrameBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxOptions options) override;
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_FRAMEBOX); }
      virtual Expr to_pmath(BoxFlags flags) override;
      
    protected:
      float em;
  };
}

#endif // __BOXES__FRAMEBOX_H__
