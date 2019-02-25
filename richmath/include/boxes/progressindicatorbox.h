#ifndef __RICHMATH__PROGRESSINDICATOR_H__
#define __RICHMATH__PROGRESSINDICATOR_H__

#include <boxes/box.h>
#include <eval/dynamic.h>
#include <gui/control-painter.h>


namespace richmath {
  class ProgressIndicatorBox: public Box, public ControlContext {
    public:
      explicit ProgressIndicatorBox();
      virtual ~ProgressIndicatorBox();
      
      // Box::try_create<ProgressIndicatorBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Box *item(int i) override { return nullptr; }
      virtual int count() override { return 0; }
      virtual int length() override { return 0; }
      
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      virtual Box *remove(int *index) override { return this; }
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
        
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      
      virtual Box *mouse_sensitive() override { return this; }
      virtual void on_mouse_move(MouseEvent &event) override;
      
      virtual bool is_foreground_window() override;
      virtual int dpi() override;
    
    private:
      Expr to_literal();
    
    private:
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
