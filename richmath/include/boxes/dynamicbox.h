#ifndef __BOXES__DYNAMICBOX_H__
#define __BOXES__DYNAMICBOX_H__

#include <boxes/ownerbox.h>
#include <eval/dynamic.h>

namespace richmath{
  class DynamicBox: public OwnerBox{
    public:
      virtual ~DynamicBox();
      
      static DynamicBox *create(Expr expr, int opts);
      
      virtual void resize(Context *context);
      virtual void paint_content(Context *context);
      
      virtual Expr to_pmath(bool parseable);
      
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
      virtual bool edit_selection(Context *context){ return false; }
      
      virtual void on_mouse_enter();
      virtual void on_mouse_exit();
    
    protected:
      explicit DynamicBox();
    
    public:
      Dynamic dynamic;
    
    protected:
      bool must_update;
      bool must_resize;
  };
};

#endif // __BOXES__DYNAMICBOX_H__
