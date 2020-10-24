#include <graphics/animation.h>

#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

//{ class TimedEvent ...

TimedEvent::TimedEvent(double min_wait_seconds)
  : Shareable(),
  start_time(pmath_tickcount()),
  min_wait_seconds(min_wait_seconds)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

bool TimedEvent::register_for(FrontEndReference box_id) {
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

BoxRepaintEvent::BoxRepaintEvent(FrontEndReference box_id, double min_wait_seconds)
  : TimedEvent(min_wait_seconds),
  box_id(box_id)
{
  if(!register_for(box_id))
    box_id = FrontEndReference::None;
}

void BoxRepaintEvent::execute_event() {
  if(auto box = FrontEndObject::find_cast<Box>(box_id))
    box->request_repaint_all();
}

//} ... class BoxRepaintEvent

//{ class BoxAnimation ...

bool BoxAnimation::is_compatible(Canvas &canvas) {
  if(!current_buffer)
    return false;
  
  return current_buffer->is_compatible(canvas);
}

bool BoxAnimation::is_compatible(Canvas &canvas, float w, float h) {
  if(!current_buffer)
    return false;
  
  return current_buffer->is_compatible(canvas, w, h);
}

bool BoxAnimation::is_compatible(Canvas &canvas, const BoxSize &size) {
  if(!current_buffer)
    return false;
  
  return current_buffer->is_compatible(canvas, size);
}

//} ... class BoxAnimation

//{ class LinearTransition ...

LinearTransition::LinearTransition(
  FrontEndReference  box_id,
  Canvas            &dst,
  const BoxSize     &size,
  double             seconds)
  : BoxAnimation(box_id),
  seconds(seconds),
  repeat(false)
{
  if(seconds > 0) {
    buf1           = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    buf2           = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    current_buffer = new Buffer(dst, CAIRO_FORMAT_ARGB32, size);
    
    if(!buf1->canvas() || !buf2->canvas() || !current_buffer->canvas())
      buf1 = buf2 = current_buffer = nullptr;
  }
}

LinearTransition::LinearTransition(
  FrontEndReference  box_id,
  Canvas            &dst,
  const RectangleF  &rect,
  double             seconds)
  : BoxAnimation(box_id),
  seconds(seconds),
  repeat(false)
{
  if(seconds > 0) {
    buf1           = new Buffer(dst, CAIRO_FORMAT_ARGB32, rect);
    buf2           = new Buffer(dst, CAIRO_FORMAT_ARGB32, rect);
    current_buffer = new Buffer(dst, CAIRO_FORMAT_ARGB32, rect);
    
    if(!buf1->canvas() || !buf2->canvas() || !current_buffer->canvas())
      buf1 = buf2 = current_buffer = nullptr;
  }
}

void LinearTransition::update(ControlContext *cc) {
  if(repeat && !cc->is_foreground_window())
    repeat = false;
}

bool LinearTransition::paint(Canvas &canvas) {
  if(!buf1 || !buf2 || seconds <= 0)
    return false;
    
  double t = timer();
  if(t < 0)
    t = 0;
    
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
    box_id = FrontEndReference::None;
    return false;
  }
  
  t /= seconds;
  
  if(!current_buffer->blend(buf1, buf2, t))
    return false;
    
  return current_buffer->paint(canvas);
}

//} ... class LinearTransition
