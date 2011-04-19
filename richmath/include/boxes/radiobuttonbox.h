#ifndef __BOXES__RADIOBUTTONBOX_H__
#define __BOXES__RADIOBUTTONBOX_H__

#include <boxes/emptywidgetbox.h>
#include <eval/dynamic.h>

namespace richmath{
  class RadioButtonBox: public EmptyWidgetBox{
    public:
      static RadioButtonBox *create(Expr expr);
      
      virtual void paint(Context *context);
      
      virtual Expr to_pmath(bool parseable);
      
      virtual void dynamic_finished(Expr info, Expr result);
      
      virtual void on_mouse_up(MouseEvent &event);
      
    protected:
      RadioButtonBox();
      
      ContainerType calc_type(Expr result);
    
      Dynamic dynamic;
      Expr    value;
  };
}

#endif // __BOXES__RADIOBUTTONBOX_H__
