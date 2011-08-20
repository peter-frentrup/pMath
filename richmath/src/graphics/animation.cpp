#include <graphics/animation.h>

#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class TimedEvent ...

TimedEvent::TimedEvent(double _min_wait_seconds)
  : Shareable(),
  start_time(pmath_tickcount()),
  min_wait_seconds(_min_wait_seconds)
{
}

bool TimedEvent::register_for(int box_id) {
  Box *box = FrontEndObject::find_cast<Box>(box_id);
  
  if(!box)
    return false;
    
  Document *doc = box->find_parent<Document>(true);
  
  if(doc && doc->native()) {
    ref();
    if(doc->native()->register_timed_event(this))
      return true;
  }
  
  return false;
}

//} ... class TimedEvent

//{ class BoxRepaintEvent ...

BoxRepaintEvent::BoxRepaintEvent(int _box_id, double _min_wait_seconds)
  : TimedEvent(_min_wait_seconds),
  box_id(_box_id)
{
  if(!register_for(box_id))
    box_id = 0;
}

void BoxRepaintEvent::execute_event() {
  Box *box = FrontEndObject::find_cast<Box>(box_id);
  
  if(box)
    box->request_repaint_all();
}

//} ... class BoxRepaintEvent

//{ class LinearTransition ...

LinearTransition::LinearTransition(
  int _box_id,
  Canvas *dst,
  const BoxSize &size,
  double _seconds)
  : BoxAnimation(_box_id),
  seconds(_seconds),
  repeat(false)
{
  if(_seconds > 0) {
    buf1           = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    buf2           = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    current_buffer = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    
    if(!buf1->canvas() || !buf2->canvas() || !current_buffer->canvas())
      buf1 = buf2 = current_buffer = 0;
  }
}

LinearTransition::LinearTransition(
  int _box_id,
  Canvas *dst,
  float x, float y, float w, float h,
  double _seconds)
  : BoxAnimation(_box_id),
  seconds(_seconds),
  repeat(false)
{
  if(_seconds > 0) {
    buf1           = new Buffer(dst, CAIRO_FORMAT_ARGB32, x, y, w, h);
    buf2           = new Buffer(dst, CAIRO_FORMAT_ARGB32, x, y, w, h);
    current_buffer = new Buffer(dst, CAIRO_FORMAT_ARGB32, x, y, w, h);
    
    if(!buf1->canvas() || !buf2->canvas() || !current_buffer->canvas())
      buf1 = buf2 = current_buffer = 0;
  }
}

bool LinearTransition::paint(Canvas *canvas) {
  if(!buf1 || !buf2 || seconds <= 0)
    return false;
    
  double t = timer();
  if(t < 0)
    return false;
    
  if(t > seconds) {
    if(repeat) {
      SharedPtr<Buffer> tmp = buf1;
      buf1 = buf2;
      buf2 = tmp;
      
      reset_timer();
      t = 0;
    }
    else
      return false;
  }
  
  if(!register_for(box_id)) {
    box_id = 0;
    return false;
  }
  
  t /= seconds;
  
  if(!current_buffer->blend(buf1, buf2, t))
    return false;
    
  return current_buffer->paint(canvas);
}

//} ... class LinearTransition
