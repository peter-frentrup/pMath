#ifndef __BOXES__DYNAMICBOX_H__
#define __BOXES__DYNAMICBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class DynamicBox: public OwnerBox{
    public:
      explicit DynamicBox(Expr _dynamic_content);
      virtual ~DynamicBox();
      
      static DynamicBox *create(Expr expr, int opts);
      
      virtual void resize(Context *context);
      virtual void paint_content(Context *context);
      
      virtual pmath_t to_pmath(bool parseable);
      
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
      virtual bool edit_selection(Context *context){ return false; }
    
    public:
      Expr dynamic_content;
    
    protected:
      bool must_update;
      bool must_resize;
  };
};

#endif // __BOXES__DYNAMICBOX_H__
