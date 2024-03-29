#ifndef RICHMATH__GRAPHICS__ANIMATION_H__INCLUDED
#define RICHMATH__GRAPHICS__ANIMATION_H__INCLUDED

#include <eval/cubic-bezier-easing-function.h>
#include <util/frontendobject.h>
#include <graphics/buffer.h>


namespace richmath {
  class ControlContext;
  
  class TimedEvent: public Shareable {
    public:
      TimedEvent(double min_wait_seconds);
      
      bool register_for(FrontEndReference box_id);
      virtual void execute_event() = 0;
      
      void reset_timer() {
        start_time = pmath_tickcount();
      }
      
      double timer() const {
        return pmath_tickcount() - start_time;
      }
      
    public:
      double start_time;
      double min_wait_seconds;
  };
  
  class BoxRepaintEvent: public TimedEvent {
    public:
      BoxRepaintEvent(FrontEndReference box_id, double min_wait_seconds);
      
      bool register_event() { return register_for(box_id); }
      virtual void execute_event() override;
      
    public:
      FrontEndReference box_id;
  };
  
  class BoxAnimation: public BoxRepaintEvent {
    public:
      BoxAnimation(FrontEndReference box_id): BoxRepaintEvent(box_id, 0.0) {}
      
      virtual void update(ControlContext *control) = 0;
      virtual bool paint(Canvas &canvas) = 0;
      
      bool is_compatible(Canvas &canvas);
      bool is_compatible(Canvas &canvas, float w, float h);
      bool is_compatible(Canvas &canvas, const BoxSize &size);
      
    public:
      SharedPtr<Buffer> current_buffer;
  };
  
  class FadeAnimation final : public BoxAnimation {
    public:
      FadeAnimation(
        FrontEndReference  box_id,
        Canvas            &dst,
        const BoxSize     &size,
        double             seconds);
        
      FadeAnimation(
        FrontEndReference  box_id,
        Canvas            &dst,
        const RectangleF  &rect,
        double             seconds);
      
      virtual void update(ControlContext *control) override;
      virtual bool paint(Canvas &canvas) override;
      
    public:
      CubicBezierEasingFunction transition_function;
      double seconds;
      SharedPtr<Buffer> buf1;
      SharedPtr<Buffer> buf2;
      bool repeat;
  };
}

#endif // RICHMATH__GRAPHICS__ANIMATION_H__INCLUDED
