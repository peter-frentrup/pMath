#ifndef RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED
#define RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED

#include <boxes/ownerbox.h>
#include <eval/dynamic.h>


namespace richmath {
  class AbstractDynamicBox: public OwnerBox {
    protected:
      virtual ~AbstractDynamicBox();
    public:
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
    protected:
      explicit AbstractDynamicBox();
  };
  
  class DynamicBox: public AbstractDynamicBox {
    protected:
      virtual ~DynamicBox();
    public:
      explicit DynamicBox();
      
      // Box::try_create<DynamicBox>(expr, options)
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual void paint_content(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      virtual bool edit_selection(SelectionReference &selection) override;
      
    protected:
      virtual void resize_default_baseline(Context &context) override;
      
    public:
      Dynamic dynamic;
      
    protected:
      bool must_update;
      bool must_resize;
  };
};

#endif // RICHMATH__BOXES__DYNAMICBOX_H__INCLUDED
