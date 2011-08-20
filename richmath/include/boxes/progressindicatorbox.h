#ifndef __RICHMATH__PROGRESSINDICATOR_H__
#define __RICHMATH__PROGRESSINDICATOR_H__

#include <boxes/box.h>
#include <eval/dynamic.h>
#include <gui/control-painter.h>


namespace richmath {
  class ProgressIndicatorBox: public Box {
    public:
      virtual ~ProgressIndicatorBox();
      
      static ProgressIndicatorBox *create(Expr expr);
      
      virtual Box *item(int i) { return 0; }
      virtual int count() { return 0; }
      virtual int length() { return 0; }
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      virtual Box *remove(int *index) { return this; }
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_PROGRESSINDICATORBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual void dynamic_updated();
      virtual void dynamic_finished(Expr info, Expr result);
      virtual Box *dynamic_to_literal(int *start, int *end);
      
      virtual Box *mouse_sensitive() { return this; }
      virtual void on_mouse_move(MouseEvent &event);
      
    protected:
      explicit ProgressIndicatorBox();
      
    protected:
      double range_min;
      double range_max;
      double range_value;
      Expr range;
      Dynamic dynamic;
      
      SharedPtr<BoxAnimation> animation;
      bool must_update;
      bool have_drawn;
  };
};

#endif // __RICHMATH__PROGRESSINDICATOR_H__
