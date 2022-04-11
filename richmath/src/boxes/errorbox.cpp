#include <boxes/errorbox.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <graphics/context.h>

using namespace richmath;

namespace richmath { namespace strings {
  extern String Message;
}}

extern pmath_symbol_t richmath_System_StyleBox;

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

void ErrorBox::on_mouse_enter() {
  if(auto doc = find_parent<Document>(false)) {
    Expr name = _object.is_expr() ? _object[0] : _object;
    Expr tooltip_boxes = List(String("Unknown box name: "), name.to_string());
    tooltip_boxes = Call(Symbol(richmath_System_StyleBox), tooltip_boxes, strings::Message);
    doc->native()->show_tooltip(this, tooltip_boxes);
  }
}

void ErrorBox::on_mouse_exit() {
  if(auto doc = find_parent<Document>(false))
    doc->native()->hide_tooltip();
}

//} ... class ErrorBox
