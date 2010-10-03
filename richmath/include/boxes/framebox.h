#ifndef __BOXES__FRAMEBOX_H__
#define __BOXES__FRAMEBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class FrameBox: public OwnerBox{
    public:
      FrameBox(MathSequence *content = 0);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Expr to_pmath(bool parseable);
      
    protected:
      float em;
  };
}

#endif // __BOXES__FRAMEBOX_H__
