#ifndef RICHMATH__BOXES__FRAMEBOX_H__INCLUDED
#define RICHMATH__BOXES__FRAMEBOX_H__INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class FrameBox: public OwnerBox {
    public:
      explicit FrameBox(MathSequence *content = 0);
      
      // Box::try_create<FrameBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual void paint(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
    protected:
      virtual void resize_default_baseline(Context &context) override;
      
    protected:
      float em;
  };
}

#endif // RICHMATH__BOXES__FRAMEBOX_H__INCLUDED
