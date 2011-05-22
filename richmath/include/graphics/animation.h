#ifndef __GRAPHICS__ANIMATION_H__
#define __GRAPHICS__ANIMATION_H__

#include <graphics/buffer.h>

namespace richmath{
  class TimedEvent: public Shareable{
    public:
      TimedEvent(double _min_wait_seconds);

      bool register_for(int box_id);
      virtual void execute_event() = 0;

      void reset_timer(){
        start_time = pmath_tickcount();
      }

      double timer(){
        //return (((double)clock()) - (double)start_time) / (double)CLOCKS_PER_SEC;
        return pmath_tickcount() - start_time;
      }

    public:
      double start_time;
      double min_wait_seconds;
  };

  class BoxRepaintEvent: public TimedEvent{
    public:
      BoxRepaintEvent(int _box_id, double _min_wait_seconds);

      bool register_event(){ return register_for(box_id); }
      virtual void execute_event();

    public:
      int box_id;
  };

  class BoxAnimation: public BoxRepaintEvent{
    public:
      BoxAnimation(int _box_id): BoxRepaintEvent(_box_id, 0.0){}

      virtual bool paint(Canvas *canvas) = 0;

    public:
      SharedPtr<Buffer> current_buffer;
  };

  class LinearTransition: public BoxAnimation{
    public:
      LinearTransition(
        int _box_id,
        Canvas *dst,
        const BoxSize &size,
        double _seconds);

      LinearTransition(
        int _box_id,
        Canvas *dst,
        float x, float y, float w, float h,
        double _seconds);

      virtual bool paint(Canvas *canvas);

    public:
      double seconds;
      SharedPtr<Buffer> buf1;
      SharedPtr<Buffer> buf2;
      bool repeat;
  };
}

#endif // __GRAPHICS__ANIMATION_H__
