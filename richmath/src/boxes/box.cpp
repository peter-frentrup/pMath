#include <boxes/box.h>

#include <eval/application.h>

#include <graphics/context.h>
#include <gui/native-widget.h>

#include <stdio.h>


using namespace pmath;
using namespace richmath;


namespace {
  class BoxNavigation {
    public:
      static Box *next_box(Box *box, LogicalDirection direction, Box *restrict_to_parent);
    
    private:
      static int get_box_item_number(Box *parent, Box *box); // -1 on error
      static Box *next_box_outside(Box *box, LogicalDirection direction, Box *stop_parent);
  };
}

extern pmath_symbol_t richmath_System_Options;

//{ class MouseEvent ...

MouseEvent::MouseEvent()
  : position(0, 0),
    id(0),
    device(DeviceKind::Mouse),
    left(false),
    middle(false),
    right(false),
    origin(nullptr)
{
}

void MouseEvent::set_origin(Box *new_origin) {
  if(origin == new_origin)
    return;
    
  Box *common = nullptr;//Box::common_parent(origin, new_origin);
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  
  if(new_origin) {
    new_origin->transformation(common, &mat);
    cairo_matrix_invert(&mat);
  }
  
  if(origin)
    origin->transformation(common, &mat);
    
  position = Canvas::transform_point(mat, position);
  
  origin = new_origin;
}

//} ... class MouseEvent

//{ class AutoMemorySuspension ...

static int deletion_suspensions = 0;
static Box *box_limbo = nullptr;

bool AutoMemorySuspension::are_deletions_suspended() {
  return deletion_suspensions > 0;
}

void AutoMemorySuspension::suspend_deletions() {
  ++deletion_suspensions;
}

void AutoMemorySuspension::resume_deletions() {
  if(--deletion_suspensions > 0)
    return;
    
  int count = 0;
  while(box_limbo) {
    Box *tmp = box_limbo;
    box_limbo = tmp->_parent_or_limbo_next.as_tinted();
    
//    Expr expr = tmp->to_pmath(0);
//    pmath_debug_print_object("[limbo deletion: \n  ", expr.get(), "\n]\n");

    delete tmp;
    ++count;
  }
  
  if(count > 0)
    fprintf(stderr, "[deleted %d objects from limbo]\n", count);
}

//} ... class AutoMemorySuspension

//{ class Box ...

Box::Box()
  : ActiveStyledObject(),
    _parent_or_limbo_next(nullptr),
    _index(0),
    _extents(0, 0, 0)
{
}

Box::~Box() {
  if(AutoMemorySuspension::are_deletions_suspended()) {
    fprintf(stderr, "[warning: delete Box during memory suspension]\n");
  }
  Application::deactivated_control(this);
}

void Box::after_insertion() {
  for(int i = 0; i < count(); ++i)
    item(i)->after_insertion();
}

void Box::after_insertion(int start, int end) {
  int i = 0;
  while(i < count() && item(i)->index() < start)
    ++i;
  
  for(; i < count(); ++i) {
    Box *box = item(i);
    if(box->index() >= end)
      break;
    
    box->after_insertion();
  }
}

void Box::safe_destroy() {
  if(AutoMemorySuspension::are_deletions_suspended()) {
    _index = 0;
    TintedPtr_ASSERT( _parent_or_limbo_next.is_normal() );
    _parent_or_limbo_next = TintedPtr<Box, Box>::FromTinted(box_limbo);
    box_limbo = this;
    return;
  }
  
  delete this;
}

Expr Box::allowed_options() {
  Expr head = to_pmath_symbol();
  if(!head.is_symbol())
    return {};
  
  Expr opts = Call(Symbol(richmath_System_Options), std::move(head));
  return Application::interrupt_wait_cached(std::move(opts));
}

bool Box::is_parent_of(Box *child) {
  while(child && child != this)
    child = child->parent();
    
  return child == this;
}

Box *Box::common_parent(Box *a, Box *b) {
  int d1 = 0;
  Box *tmp = a;
  while(tmp) {
    tmp = tmp->parent();
    ++d1;
  }
  
  int d2 = 0;
  tmp = b;
  while(tmp) {
    tmp = tmp->parent();
    ++d2;
  }
  
  while(d1 > d2) {
    a = a->parent();
    --d1;
  }
  
  while(d1 < d2) {
    b = b->parent();
    --d2;
  }
  
  while(a != b) {
    a = a->parent();
    b = b->parent();
  }
  
  return a;
}

Box *Box::next_box(LogicalDirection direction, Box *restrict_to_parent) {
  return BoxNavigation::next_box(this, direction, restrict_to_parent);
}

Box *Box::next_child_or_null(int index, LogicalDirection direction) {
  int _count = count();
  if(_count == 0)
    return nullptr;
  
  if(direction == LogicalDirection::Forward) {
    for(int i = 0; i < _count; ++i) {
      Box *box = item(i);
      if(box->index() >= index)
        return box;
    }
  }
  else {
    for(int i = _count - 1; i >= 0; --i) {
      Box *box = item(i);
      if(box->index() < index)
        return box;
    }
  }
    
  return nullptr;
}

bool Box::update_dynamic_styles(Context &context) {
  if(context.stylesheet)
    return context.stylesheet->update_dynamic(style, this);
  
  return false;
}

void Box::colorize_scope(SyntaxState &state) {
  for(int i = 0; i < count(); ++i)
    item(i)->colorize_scope(state);
}

VolatileSelection Box::get_highlight_child(const VolatileSelection &src) {
  if(auto par = parent())
    return par->get_highlight_child(src);
    
  return src;
}

void Box::selection_path(Canvas &canvas, int start, int end) {
  if(end > count())
    end = count();
    
  Point p0 = canvas.current_pos();
  
  for(int i = start; i < end; ++i) 
    canvas.pixrect(item(i)->extents().to_rectangle(p0), false);
}

void Box::scroll_to(const RectangleF &rect) {
}

void Box::scroll_to(Canvas &canvas, const VolatileSelection &child) {
  if(auto par = parent())
    par->scroll_to(canvas, child);
}

void Box::default_scroll_to(Canvas &canvas, Box *scroll_view, const VolatileSelection &child_sel) {
  double x1, y1, x2, y2;
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child_sel.box->transformation(scroll_view, &mat);
  
  canvas.save();
  {
    canvas.transform(mat);
    canvas.move_to(0, 0);
    
    child_sel.box->selection_path(
      canvas,
      child_sel.start,
      child_sel.end);
      
    // cairo 1.10.0 bug:
    // cairo_path_extents gives (0,0,0,0) for pixel aligned lines
    canvas.stroke_extents(&x1, &y1, &x2, &y2);
    
    canvas.new_path();
  }
  canvas.restore();
  
  float x = x1;
  float y = y1;
  float w = x2 - x1;
  float h = y2 - y1;
  Canvas::transform_rect(mat, &x, &y, &w, &h);
  scroll_to(RectangleF{x, y, w, h});
  
  if(auto par = parent())
    par->scroll_to(canvas, child_sel);
}

Box *Box::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(style) {
    if(auto par = parent()) {
      if(get_own_style(Selectable, AutoBoolAutomatic) == AutoBoolFalse) {
        if(direction == LogicalDirection::Forward)
          *index = _index + 1;
        else
          *index = _index;
          
        return par;
      }
    }
  }
  
  if(direction == LogicalDirection::Forward) {
    int b = *index;
    if(b < 0 || jumping)
      ++b;
      
    if(b < count()) {
      *index = -1;
      return item(b)->move_logical(LogicalDirection::Forward, false, index);
    }
    
    if(auto par = parent()) {
      *index = _index;
      return par->move_logical(LogicalDirection::Forward, true, index);
    }
    
    return this;  
  }
  
  int b = *index - 1;
  if(b >= count())
    b = count() - 1;
  else if(jumping)
    --b;
    
  if(b >= 0) {
    *index = item(b)->length() + 1;
    return item(b)->move_logical(LogicalDirection::Backward, false, index);
  }
  
  if(auto par = parent()) {
    *index = _index + 1;
    return par->move_logical(LogicalDirection::Backward, true, index);
  }
  
  return this;
}

Box *Box::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(auto par = parent()) {
    *index = _index;
    return par->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

VolatileSelection Box::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  if(auto par = parent()) {
    return { par, _index, _index + 1 };
  }
  return { this, 0, length() };
}

void Box::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
}

void Box::transformation(
  Box            *ancestor,
  cairo_matrix_t *matrix
) {
  if(ancestor == this)
    return;
  
  if(auto par = parent()) {
    par->transformation(ancestor, matrix);
    par->child_transformation(_index, matrix);
  }
}

bool Box::selectable(int i) {
  switch(get_own_style(Selectable, AutoBoolAutomatic)) {
    case AutoBoolTrue:  return true;
    case AutoBoolFalse: return false;
    default: break;
  }
  
  if(auto par = parent())
    return par->selectable(_index);
  
  return true;
}

VolatileSelection Box::normalize_selection(int start, int end) {
  if(auto par = parent()) 
    return par->normalize_selection(_index, _index + 1);
  
  return {};
}

Expr Box::prepare_dynamic(Expr expr) {
  if(auto par = parent())
    return par->prepare_dynamic(std::move(expr));
  return expr;
}

void Box::dynamic_updated() {
  if(style) {
    style->flag_pending_dynamic();
    request_repaint_all();
  }
}

VolatileSelection Box::dynamic_to_literal(int start, int end) {
  for(int b = start; b < end; ++b) 
    item(b)->all_dynamic_to_literal();
  
  return {this, start, end};
}

bool Box::request_repaint_all() {
  return request_repaint(_extents.to_rectangle());
}

bool Box::request_repaint_range(int start, int end) {
  return request_repaint_all();
}

bool Box::request_repaint(const RectangleF &rect) {
  if(!_extents.to_rectangle().overlaps(rect, 0.0001))
    return false;
  
  if(auto par = parent()) {
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    transformation(par, &matrix);
    return par->request_repaint(Canvas::transform_rect(matrix, rect));
  }
  
  return false;
}

bool Box::visible_rect(RectangleF &rect, Box *top_most) {
  if(this == top_most)
    return true;
  
  if(!_extents.to_rectangle().overlaps(rect, 0.0001))
    return false;
  
  if(auto par = parent()) {
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    transformation(par, &matrix);
    Canvas::transform_rect_inline(matrix, rect);
    return par->visible_rect(rect, top_most);
  }
  
  return false;
}

void Box::invalidate() {
  if(auto par = parent())
    par->invalidate();
}

void Box::invalidate_options() {
  invalidate();
}

bool Box::edit_selection(SelectionReference &selection) {
  if(!get_own_style(Editable, true))
    return false;
    
  if(auto par = parent())
    return par->edit_selection(selection);
  
  return true;
}

//{ event handlers ...

Box *Box::mouse_sensitive() {
  if(auto par = parent())
    return par->mouse_sensitive();
  return nullptr;
}

void Box::on_mouse_enter() {
  auto i = get_own_style(InternalUsesCurrentValueOfMouseOver, ObserverKindNone);
  if(i & ObserverKindSelf)
    dynamic_updated();
  
  if(i & ObserverKindOther) {
    if(style)
      style->notify_all();
  }
}

void Box::on_mouse_exit() {
  auto i = get_own_style(InternalUsesCurrentValueOfMouseOver, ObserverKindNone);
  if(i & ObserverKindSelf)
    dynamic_updated();
  
  if(i & ObserverKindOther) {
    if(style)
      style->notify_all();
  }
}

void Box::on_mouse_down(MouseEvent &event) {
}

void Box::on_mouse_move(MouseEvent &event) {
}

void Box::on_mouse_up(MouseEvent &event) {
}

void Box::on_mouse_cancel() {
}

void Box::on_enter() {
}

void Box::on_exit() {
}

void Box::on_finish_editing() {
}

void Box::on_key_down(SpecialKeyEvent &event) {
  if(auto par = parent())
    par->on_key_down(event);
}

void Box::on_key_up(SpecialKeyEvent &event) {
  if(auto par = parent())
    par->on_key_up(event);
}

void Box::on_key_press(uint32_t unichar) {
  if(auto par = parent())
    par->on_key_press(unichar);
}

//} ... event handlers

void Box::adopt(Box *child, int i) {
  assert(child != nullptr);
  assert(child->_parent_or_limbo_next.is_normal());
  assert(child->parent() == nullptr || child->parent() == this);
  child->_parent_or_limbo_next = TintedPtr<Box,Box>(this);
  child->_index = i;
}

void Box::abandon(Box *child) {
  assert(child != nullptr);
  assert(child->parent() == this);
  child->_parent_or_limbo_next = TintedPtr<Box,Box>(nullptr);
  child->_index = 0;
}

FunctionChain<Box*, Expr> *Box::on_finish_load_from_object = nullptr;

void Box::finish_load_from_object(Expr expr) {
  FunctionChain<Box*, Expr> *event = on_finish_load_from_object;
  while(event) {
    if(event->func)
      event->func(this, expr);
    event = event->next;
  }
}

//} ... class Box

//{ class AbstractSequence ...

AbstractSequence::AbstractSequence()
  : Box()
{
}

AbstractSequence::~AbstractSequence() {
}

int AbstractSequence::insert(int pos, AbstractSequence *seq, int start, int end) {
  if(pos > length())
    pos = length();
    
  seq->ensure_boxes_valid();
  
  int box = 0;
  while(box < seq->count() && seq->item(box)->index() < start)
    ++box;
    
  String s = seq->raw_substring(start, end - start);
  const uint16_t *buf = s.buffer();
  start = 0;
  end   = s.length();
  
  while(start < end) {
    int next = start;
    while(next < end && buf[next] != PMATH_CHAR_BOX)
      ++next;
      
    pos = insert(pos, s.part(start, next - start));
    
    if(next < end)
      pos = insert(pos, seq->extract_box(box++));
      
    start = next + 1;
  }
  
  return pos;
}

bool AbstractSequence::try_load_from_object(Expr object, BoxInputFlags options) {
  load_from_object(object, options);
  return true;
}

VolatileSelection AbstractSequence::dynamic_to_literal(int start, int end) {
  int b = 0;
  while(b < count()) {
    Box *box = item(b);
    if(box->index() >= start)
      break;
      
    ++b;
  }
  
  while(b < count()) {
    Box *box = item(b);
    
    if(box->index() >= end)
      break;
    
    VolatileSelection next = box->all_dynamic_to_literal();
    if(next.box == this) {
      end += next.length() - 1; // TODO: -1 for the old PMATH_CHAR_BOX is only correct for MathSequence 
      
      while(b < count()) {
        box = item(b);
        if(box->index() >= next.end)
          break;
        ++b;
      }
    }
    else
      ++b;
  }
  
  return {this, start, end};
}

bool AbstractSequence::request_repaint_range(int start, int end) {
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
    
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child_transformation(start, &mat);
  
  Point p1 = Canvas::transform_point(mat, Point(0, 0));
  
  Point p2;
  if(start == end) {
    p2 = p1;
  }
  else {
    cairo_matrix_init_identity(&mat);
    child_transformation(end, &mat);
    p2 = Canvas::transform_point(mat, Point(0, 0));
  }
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  if(l1 == l2)
    return request_repaint({p1.x, p1.y - a1, p2.x - p1.x, a1 + d1});
             
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  bool result = true;
  result = request_repaint({p1.x, p1.y - a1, extents().width - p1.x, a1 + d1}) || result;
  result = request_repaint({0.0f, p1.y + d1, extents().width, p2.y - a2 - p1.y - d1}) || result;
  result = request_repaint({0.0f, p2.y - a2, p2.x, a2 + d2}) || result;
  return result;
}

//} ... class AbstractSequence

//{ class BoxNavigation ...

Box *BoxNavigation::next_box(Box *box, LogicalDirection direction, Box *restrict_to_parent) {
  if(!box)
    return nullptr;
    
  int count = box->count();
  if(count > 0) {
    if(direction == LogicalDirection::Forward)
      return box->item(0);
    else
      return box->item(count - 1);
  }
  
  return next_box_outside(box, direction, restrict_to_parent);
}

int BoxNavigation::get_box_item_number(Box *parent, Box *box) { // -1 on error
  assert(parent);
  assert(box);
  
  int count = parent->count();
  if(count == parent->length()) {
    int i = box->index();
    if(/*i >= 0 && i < count && */parent->item(i) == box)
      return i;
    return -1;
  }
  
  for(int i = 0; i < count; ++i) {
    if(parent->item(i) == box)
      return i;
  }
  return -1;
}

Box *BoxNavigation::next_box_outside(Box *box, LogicalDirection direction, Box *stop_parent) {
  int delta = (direction == LogicalDirection::Forward) ? +1 : -1;
  
  while(box && box != stop_parent) {
    Box *parent = box->parent();
    if(!parent)
      return nullptr;
      
    int num = get_box_item_number(parent, box);
    if(num < 0)
      return nullptr;
      
    num += delta;
    if(0 <= num && num < parent->count())
      return parent->item(num);
      
    box = parent;
  }
  return nullptr;
}

//} ... class BoxNavigation
