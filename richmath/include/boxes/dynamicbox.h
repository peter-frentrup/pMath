#ifndef __BOXES__DYNAMICBOX_H__
#define __BOXES__DYNAMICBOX_H__

#include <boxes/ownerbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class AbstractDynamicBox: public OwnerBox {
    public:
      virtual ~AbstractDynamicBox();
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
    protected:
      explicit AbstractDynamicBox();
  };
  
  class DynamicBox: public AbstractDynamicBox {
    public:
      explicit DynamicBox();
      virtual ~DynamicBox();
      
      // Box::try_create<DynamicBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void resize(Context *context) override;
      virtual void paint_content(Context *context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      virtual bool edit_selection(Context *context) override;
      
    public:
      Dynamic dynamic;
      
    protected:
      bool must_update;
      bool must_resize;
  };
};

#endif // __BOXES__DYNAMICBOX_H__
