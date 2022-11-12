#include <boxes/progressindicatorbox.h>
#include <eval/application.h>
#include <gui/document.h>
#include <gui/native-widget.h>

#include <cmath>
#include <limits>

using namespace richmath;
using namespace std;

extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_System_ProgressIndicatorBox;

#ifdef _MSC_VER
namespace std {
  static bool isnan(double d) {return _isnan(d);}
}
#endif

#ifndef NAN
#  define NAN  (std::numeric_limits<double>::quiet_NaN())
#endif

//{ class ProgressIndicatorBox ...

ProgressIndicatorBox::ProgressIndicatorBox()
  : base(),
    range_min(0.0),
    range_max(1.0),
    range_value(0.5)
{
  must_update(true);
  dynamic.init(this, Expr());
}

ProgressIndicatorBox::~ProgressIndicatorBox() {
}

bool ProgressIndicatorBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_ProgressIndicatorBox)
    return false;
    
  if(expr.expr_length() < 2)
    return false;
    
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  Expr new_range = expr[2];
  if(new_range[0] != richmath_System_Range)
    return false;
    
  if(new_range.expr_length() != 2)
    return false;
    
  double new_range_min = new_range[1].to_double(NAN);
  double new_range_max = new_range[2].to_double(NAN);
  
  if(std::isnan(new_range_min))
    return false;
    
  if(std::isnan(new_range_max))
    return false;
    
  /* now success is guaranteed */
  
  range     = new_range;
  range_min = new_range_min;
  range_max = new_range_max;
  
  if(dynamic.expr() != expr[1] || has(opts, BoxInputFlags::ForceResetDynamic)) {
    dynamic = expr[1];
    must_update(true);
  }
  
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
    
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

bool ProgressIndicatorBox::expand(const BoxSize &size) {
  _extents.width = size.width;
  
  return true;
}

void ProgressIndicatorBox::resize(Context &context) {
  float em = context.canvas().get_font_size();
  _extents.ascent  = 0.5 * em * 1.5;
  _extents.descent = 0;
  _extents.width   = 6 * em * 1.5;
  
  ControlPainter::std->calc_container_size(*this, context.canvas(), ContainerType::ProgressIndicatorBackground, &_extents);
  //ControlPainter::std->calc_container_size(*this, context.canvas(), ContainerType::ProgressIndicatorBar, &size);
  
  float h = _extents.height();
  _extents.ascent = 0.25 * em + 0.5 * h;
  _extents.descent = h - _extents.ascent;
}

// TODO: support progress bar animations
void ProgressIndicatorBox::paint(Context &context) {
  if(context.canvas().show_only_text)
    return;
  
  have_drawn(true);
  
  if(must_update()) {
    must_update(false);
    
    Expr val;
    if(dynamic.get_value(&val)) {
      range_value = val.to_double(NAN);
    }
  }
  
  Point pos = context.canvas().current_pos();
  ControlPainter::std->draw_container(
    *this,
    context.canvas(),
    ContainerType::ProgressIndicatorBackground,
    ControlState::Normal,
    _extents.to_rectangle(pos));
    
  double p = 0;
  ControlState state = ControlState::Normal;
  
  if(range_min <= range_value && range_value <= range_max && range_max - range_min > 0) {
    p = (range_value - range_min) / (range_max - range_min);
  }
  else if(range_value > range_max) {
    p = 1;
  }
  else {
    p = 0;
  }
  
  BoxSize content_size = _extents;
  ControlPainter::std->calc_container_size(
    *this,
    context.canvas(),
    ContainerType::ProgressIndicatorBar,
    &content_size);
    
  ControlPainter::std->draw_container(
    *this,
    context.canvas(),
    ContainerType::ProgressIndicatorBar,
    state,
    RectangleF(
      pos.x + (_extents.width - content_size.width) / 2,
      pos.y - content_size.ascent,
      content_size.width * p,
      content_size.height()));
}

Expr ProgressIndicatorBox::to_pmath_symbol() {
  return Symbol(richmath_System_ProgressIndicatorBox);
}

Expr ProgressIndicatorBox::to_pmath_impl(BoxOutputFlags flags) {
  Expr val;
  if(has(flags, BoxOutputFlags::Literal))
    val = to_literal();
  else
    val = dynamic.expr();
    
  return Call(
           Symbol(richmath_System_ProgressIndicatorBox),
           val,
           range);
}

VolatileSelection ProgressIndicatorBox::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  return { this, 0, 0 };
}

void ProgressIndicatorBox::dynamic_updated() {
  if(must_update())
    return;
    
  must_update(true);
  request_repaint_all();
}

void ProgressIndicatorBox::dynamic_finished(Expr info, Expr result) {
  double new_value = result.to_double(NAN);
  
  if(range_value != new_value)
    request_repaint_all();
}

Expr ProgressIndicatorBox::to_literal() {
  if(!dynamic.is_dynamic())
    return dynamic.expr();
  
  return dynamic.get_value_now();
}

VolatileSelection ProgressIndicatorBox::dynamic_to_literal(int start, int end) {
  dynamic = to_literal();
  return {this, start, end};
}

void ProgressIndicatorBox::on_mouse_move(MouseEvent &event) {
  Document *doc = find_parent<Document>(false);
  
  if(doc && doc->native()) {
    doc->native()->set_cursor(CursorType::Default);
  }
}

bool ProgressIndicatorBox::is_foreground_window() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_foreground_window();
}

bool ProgressIndicatorBox::is_focused_widget() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_focused_widget();
}

bool ProgressIndicatorBox::is_using_dark_mode() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return false;
  
  AutoResetCurrentObserver guard;
  return doc->native()->is_using_dark_mode();
}

int ProgressIndicatorBox::dpi() {
  Document *doc = find_parent<Document>(false);
  if(!doc)
    return 96;
  
  AutoResetCurrentObserver guard;
  return doc->native()->dpi();
}

//} ... class ProgressIndicatorBox
