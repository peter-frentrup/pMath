#include <boxes/box.h>

#include <graphics/context.h>
#include <gui/native-widget.h>


using namespace pmath;
using namespace richmath;

//{ class MouseEvent ...

MouseEvent::MouseEvent()
  : x(0),
  y(0),
  left(false),
  middle(false),
  right(false),
  source(0)
{
}

void MouseEvent::set_source(Box *new_source) {
  if(source == new_source)
    return;
    
  Box *common = 0;//Box::common_parent(source, new_source);
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  
  if(new_source) {
    new_source->transformation(common, &mat);
    cairo_matrix_invert(&mat);
  }
  
  if(source)
    source->transformation(common, &mat);
    
  Canvas::transform_point(mat, &x, &y);
  
  source = new_source;
}

//} ... class MouseEvent

//{ class Box ...

Box::Box()
  : FrontEndObject(),
  _extents(0, 0, 0),
  _parent(0),
  _index(0)
{
}

Box::~Box() {
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
    tmp = tmp->_parent;
    ++d1;
  }
  
  int d2 = 0;
  tmp = b;
  while(tmp) {
    tmp = tmp->_parent;
    ++d2;
  }
  
  while(d1 > d2) {
    a = a->_parent;
    --d1;
  }
  
  while(d1 < d2) {
    b = b->_parent;
    --d2;
  }
  
  while(a != b) {
    a = a->_parent;
    b = b->_parent;
  }
  
  return a;
}

void Box::colorize_scope(SyntaxState *state) {
  for(int i = 0; i < count(); ++i)
    item(i)->colorize_scope(state);
}

Box *Box::get_highlight_child(Box *src, int *start, int *end) {
  if(_parent)
    return _parent->get_highlight_child(src, start, end);
    
  return src;
}

void Box::selection_path(Canvas *canvas, int start, int end) {
  if(end > count())
    end = count();
    
  float x, y;
  canvas->current_pos(&x, &y);
  
  for(int i = start; i < end; ++i) {
    Box *b = item(i);
    
    float x1 = x;
    float y1 = y - b->extents().ascent;
    float x2 = x + b->extents().width;
    float y2 = y1;
    float x3 = x2;
    float y3 = y + b->extents().descent;
    float x4 = x1;
    float y4 = y3;
    
    canvas->align_point(&x1, &y1, false);
    canvas->align_point(&x2, &y2, false);
    canvas->align_point(&x3, &y3, false);
    canvas->align_point(&x4, &y4, false);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x2, y2);
    canvas->line_to(x3, y3);
    canvas->line_to(x4, y4);
    canvas->close_path();
  }
}

void Box::scroll_to(float x, float y, float w, float h) {
}

void Box::scroll_to(Canvas *canvas, Box *child, int start, int end) {
  if(_parent)
    _parent->scroll_to(canvas, child, start, end);
}

void Box::default_scroll_to(Canvas *canvas, Box *parent, Box *child, int start, int end) {
  double x1, y1, x2, y2;
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child->transformation(parent, &mat);
  
  canvas->save();
  {
    canvas->transform(mat);
    canvas->move_to(0, 0);
    
    child->selection_path(
      canvas,
      start,
      end);
      
    // cairo 1.10.0 bug:
    // cairo_path_extents gives (0,0,0,0) for pixel aligned lines
    cairo_stroke_extents(canvas->cairo(), &x1, &y1, &x2, &y2);
    
    canvas->new_path();
  }
  canvas->restore();
  
  float x = x1;
  float y = y1;
  float w = x2 - x1;
  float h = y2 - y1;
  Canvas::transform_rect(mat, &x, &y, &w, &h);
  scroll_to(x, y, w, h);
  
  if(_parent)
    _parent->scroll_to(canvas, child, start, end);
}

Box *Box::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(direction == Forward) {
    int b = *index;
    if(b < 0 || jumping)
      ++b;
      
    if(b < count()) {
      *index = -1;
      return item(b)->move_logical(Forward, false, index);
    }
    
    if(!_parent)
      return this;
      
    *index = _index;
    return _parent->move_logical(Forward, true, index);
  }
  
  int b = *index - 1;
  if(b >= count())
    b = count() - 1;
  else if(jumping)
    --b;
    
  if(b >= 0) {
    *index = item(b)->length() + 1;
    return item(b)->move_logical(Backward, false, index);
  }
  
  if(!_parent)
    return this;
    
  *index = _index + 1;
  return _parent->move_logical(Backward, true, index);
}

Box *Box::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index
) {
  if(_parent) {
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index);
  }
  
  return this;
}

Box *Box::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  if(_parent) {
    *start = _index;
    *end = *start + 1;
    return _parent;
  }
  *start = 0;
  *end = length();
  return this;
}

void Box::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
}

void Box::transformation(
  Box            *parent,
  cairo_matrix_t *matrix
) {
  if(_parent && parent != this) {
    _parent->transformation(parent, matrix);
    
    _parent->child_transformation(_index, matrix);
  }
}

bool Box::selectable(int i) {
  if(_parent)
    return _parent->selectable(_index);
    
  int result;
  
  if(style && style->get(Selectable, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    if(all->get(style, Selectable, &result))
      return result;
      
    if(all->base && all->base->get(Selectable, &result))
      return result;
  }
  
  return true;
}

Box *Box::normalize_selection(int *start, int *end) {
  if(_parent) {
    *start = _index;
    *end = _index + 1;
    return _parent->normalize_selection(start, end);
  }
  
  return 0;
}

Expr Box::prepare_dynamic(Expr expr) {
  if(_parent)
    return _parent->prepare_dynamic(expr);
  return expr;
}

void Box::dynamic_updated() {
  if(style) {
    style->set(InternalHavePendingDynamic, true);
    request_repaint_all();
  }
}

Box *Box::dynamic_to_literal(int *start, int *end) {
  for(int b = *start; b < *end; ++b) {
    Box *box = item(b);
    int s = 0;
    int e = box->length();
    item(b)->dynamic_to_literal(&s, &e);
  }
  
  return this;
}

bool Box::request_repaint_all() {
  return request_repaint(
           0.0,
           -_extents.ascent,
           _extents.width + 0.1,
           _extents.height() + 0.1);
}

bool Box::request_repaint_range(int start, int end) {
  return request_repaint_all();
}

bool Box::request_repaint(float x, float y, float w, float h) {
  if( x     > _extents.width   ||
      x + w < 0                ||
      y     > _extents.descent ||
      y + h < -_extents.ascent)
  {
    return false;
  }
  
  if(_parent) {
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    transformation(_parent, &matrix);
    
    Canvas::transform_rect(matrix, &x, &y, &w, &h);
    return _parent->request_repaint(x, y, w, h);
  }
  
  return false;
}

void Box::invalidate() {
  if(_parent)
    _parent->invalidate();
}

void Box::invalidate_options() {
  invalidate();
}

bool Box::edit_selection(Context *context) {
  int editable;
  
  if(context->stylesheet) {
    if(context->stylesheet->get(style, Editable, &editable) && !editable)
      return false;
  }
  else if(style && style->get(Editable, &editable) && !editable)
    return false;
    
  if(_parent)
    return _parent->edit_selection(context);
    
  if(context->stylesheet
      && context->stylesheet->base
      && context->stylesheet->base->get(Editable, &editable))
    return editable;
    
  return true;
}

//{ styles ...

SharedPtr<Stylesheet> Box::stylesheet() {
  if(!_parent)
    return Stylesheet::Default;
    
  return _parent->stylesheet();
}

int Box::get_style(IntStyleOptionName n, int result) {
  Box *box;
  
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    box = this;
    do {
      if(box->changes_children_style()
          && all->get(box->style, n, &result))
        return result;
        
      box = box->_parent;
    } while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else {
    box = _parent;
    while(box) {
      if(box->changes_children_style()
          && box->style
          && box->style->get(n, &result))
        return result;
        
      box = box->_parent;
    }
  }
  
  return result;
}

float Box::get_style(FloatStyleOptionName n, float result) {
  Box *box;
  
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    box = this;
    do {
      if(box->changes_children_style()
          && all->get(box->style, n, &result))
        return result;
        
      box = box->_parent;
    } while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else {
    box = _parent;
    while(box) {
      if(box->changes_children_style()
          && box->style
          && box->style->get(n, &result))
        return result;
        
      box = box->_parent;
    }
  }
  
  return result;
}

String Box::get_style(StringStyleOptionName n, String result) {
  Box  *box;
  
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    box = this;
    do {
      if(box->changes_children_style()
          && all->get(box->style, n, &result))
        return result;
        
      box = box->_parent;
    } while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else {
    box = _parent;
    while(box) {
      if(box->changes_children_style()
          && box->style
          && box->style->get(n, &result))
        return result;
        
      box = box->_parent;
    }
  }
  
  return result;
}

Expr Box::get_style(ObjectStyleOptionName n, Expr result) {
  Box  *box;
  
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    box = this;
    do {
      if(box->changes_children_style()
          && all->get(box->style, n, &result))
        return result;
        
      box = box->_parent;
    } while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else {
    box = _parent;
    while(box) {
      if(box->changes_children_style()
          && box->style
          && box->style->get(n, &result))
        return result;
        
      box = box->_parent;
    }
  }
  
  return result;
}

String Box::get_style(StringStyleOptionName n) {
  return get_style(n, String());
}

Expr Box::get_style(ObjectStyleOptionName n) {
  return get_style(n, Expr());
}

int Box::get_own_style(IntStyleOptionName n, int result) {
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    if(all->get(style, n, &result))
      return result;
      
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

float Box::get_own_style(FloatStyleOptionName n, float result) {
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    if(all->get(style, n, &result))
      return result;
      
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

String Box::get_own_style(StringStyleOptionName n, String result) {
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    if(all->get(style, n, &result))
      return result;
      
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

Expr Box::get_own_style(ObjectStyleOptionName n, Expr result) {
  if(style && style->get(n, &result))
    return result;
    
  SharedPtr<Stylesheet> all = stylesheet();
  if(all) {
    if(all->get(style, n, &result))
      return result;
      
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

String Box::get_own_style(StringStyleOptionName n) {
  return get_own_style(n, String());
}

Expr Box::get_own_style(ObjectStyleOptionName n) {
  return get_own_style(n, Expr());
}

//} ... styles

//{ event handlers ...

Box *Box::mouse_sensitive() {
  if(_parent)
    return _parent->mouse_sensitive();
  return 0;
}

void Box::on_mouse_enter() {
}

void Box::on_mouse_exit() {
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
//  if(_parent)
//    _parent->on_enter();
}

void Box::on_exit() {
//  if(_parent)
//    _parent->on_exit();
}

void Box::on_key_down(SpecialKeyEvent &event) {
  if(_parent)
    _parent->on_key_down(event);
}

void Box::on_key_up(SpecialKeyEvent &event) {
  if(_parent)
    _parent->on_key_up(event);
}

void Box::on_key_press(uint32_t unichar) {
  if(_parent)
    _parent->on_key_press(unichar);
}

//} ... event handlers

void Box::adopt(Box *child, int i) {
  assert(child != 0);
  assert(child->_parent == 0 || child->_parent == this);
  child->_parent = this;
  child->_index = i;
}

void Box::abandon(Box *child) {
  assert(child != 0);
  assert(child->_parent == this);
  child->_parent = 0;
  child->_index = 0;
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
  
  String s = seq->raw_substring(start, end - start);
  
  const uint16_t *buf = s.buffer();
  start = 0;
  end   = s.length();
  
  int box = -1;
  while(start < end) {
    int next = start;
    while(next < end && buf[next] != PMATH_CHAR_BOX)
      ++next;
      
    pos = insert(pos, s.part(start, next - start));
    
    if(next < end/* && buf[next] == PMATH_CHAR_BOX*/) {
      if(box < 0) {
        box = 0;
        while(box < seq->count() && seq->item(box)->index() < next)
          ++box;
      }
      
      pos = insert(pos, seq->extract_box(box++));
    }
    
    start = next + 1;
  }
  
  return pos;
}

Box *AbstractSequence::dynamic_to_literal(int *start, int *end) {
  int b = 0;
  while(b < count()) {
    Box *box = item(b);
    if(box->index() >= *start)
      break;
      
    ++b;
  }
  
  while(b < count()) {
    Box *box = item(b);
    
    if(box->index() >= *end)
      break;
      
    int s = 0;
    int e = box->length();
    Box *next = box->dynamic_to_literal(&s, &e);
    
    if(next == this) {
      *end += e - s - 1;
      
      while(b < count()) {
        box = item(b);
        if(box->index() >= e)
          break;
        ++b;
      }
    }
    else
      ++b;
  }
  
  return this;
}

bool AbstractSequence::request_repaint_range(int start, int end) {
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
    
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child_transformation(start, &mat);
  
  double x1 = 0;
  double y1 = 0;
  cairo_matrix_transform_point(&mat, &x1, &y1);
  
  double x2;
  double y2;
  if(start == end) {
    x2 = x1;
    y2 = y1;
  }
  else {
    x2 = y2 = 0;
    cairo_matrix_init_identity(&mat);
    child_transformation(end, &mat);
    cairo_matrix_transform_point(&mat, &x2, &y2);
  }
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  if(l1 == l2)
    return request_repaint(
             x1,
             y1 - a1,
             x2 - x1,
             a1 + d1);
             
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  bool result = true;
  result = request_repaint(
             x1,
             y1 - a1,
             extents().width - x1,
             a1 + d1) || result;
  result = request_repaint(
             0.0,
             y1 + d1,
             extents().width,
             y2 - a2 - y1 - d1) || result;
  result = request_repaint(
             0.0,
             y2 - a2,
             x2,
             a2 + d2) || result;
  return result;
}

//} ... class AbstractSequence
