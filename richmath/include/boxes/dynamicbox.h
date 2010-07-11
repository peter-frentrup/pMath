#ifndef __BOXES__DYNAMICBOX_H__
#define __BOXES__DYNAMICBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class DynamicBox: public OwnerBox{
    public:
      explicit DynamicBox(Expr _dynamic_content);
      virtual ~DynamicBox();
      
      static DynamicBox *create(Expr expr, int opts);
      
      virtual void paint_content(Context *context);
      
      virtual pmath_t to_pmath(bool parseable);
      
      virtual void dynamic_updated();
    
    public:
      Expr dynamic_content;
      bool must_update;
  };
};

#endif // __BOXES__DYNAMICBOX_H__
