#ifndef __BOXES__BUTTONBOX_H__
#define __BOXES__BUTTONBOX_H__

#include <boxes/containerwidgetbox.h>

namespace richmath{
  class ButtonBox: public ContainerWidgetBox {
    public:
      explicit ButtonBox(MathSequence *content = 0);
      
      static ButtonBox *create(Expr expr, int opts);
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      
      virtual pmath_t to_pmath(bool parseable);
      
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      
      virtual void click();
  };
}

#endif // __BOXES__BUTTONBOX_H__
