#include <boxes/errorbox.h>

#include <graphics/context.h>

using namespace richmath;

//{ class ErrorBox ...
ErrorBox::ErrorBox(const Expr object)
  : Box(),
  _object(object)
{
}

ErrorBox::~ErrorBox() {
}

bool ErrorBox::try_load_from_object(Expr expr, BoxInputFlags options) {
  if(_object != expr)
    return false;
  
  _object = expr;
  finish_load_from_object(std::move(expr));
  return true;
}

void ErrorBox::resize(Context &context) {
  float em = context.canvas().get_font_size();
  _extents.ascent = 0.75f * em;
  _extents.descent = 0.25f * em;
  _extents.width = 2 * em;
}

void ErrorBox::paint(Context &context) {
  float x, y;
  context.canvas().current_pos(&x, &y);
  
  context.draw_error_rect(
    x,
    y - _extents.ascent,
    x + _extents.width,
    y + _extents.descent);
}

VolatileSelection ErrorBox::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  return {this, 0, 0};
}

//} ... class ErrorBox
