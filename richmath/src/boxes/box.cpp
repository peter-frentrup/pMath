#include <boxes/box.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>

#include <eval/application.h>

#include <graphics/context.h>

#include <gui/native-widget.h>

#include <util/autovaluereset.h>

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
    ctrl_key(false),
    alt_key(false),
    shift_key(false),
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

//{ class Box ...

int Box::max_box_output_depth = INT_MAX;

Box::Box()
  : ActiveStyledObject(),
    _parent_or_limbo_next(nullptr),
    _index(0),
    _extents(0, 0, 0)
{
}

Box::~Box() {
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

int Box::depth(Box *box) {
  int result = 0;
  while(box) {
    ++result;
    box = box->parent();
  }
  return result;
}

Expr Box::allowed_options() {
  Expr head = to_pmath_symbol();
  if(!head.is_symbol())
    return {};
  
  Expr opts = Call(Symbol(richmath_System_Options), PMATH_CPP_MOVE(head));
  return Application::interrupt_wait_cached(PMATH_CPP_MOVE(opts));
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

Box *Box::find_nearest_box(FrontEndObject *obj) {
  Box *box = dynamic_cast<Box*>(obj);
  if(box)
    return box;
  
  auto tmp = dynamic_cast<StyledObject*>(obj);
  while(tmp) {
    box = dynamic_cast<Box*>(tmp);
    if(box)
      return box;
      
    tmp = tmp->style_parent();
  }
  
  return nullptr;
}

int Box::index_in_ancestor(Box *ancestor, int fallback) {
  Box *box = this; 
  
  int index = fallback;
  while(box && box != ancestor) {
    index = box->index();
    box = box->parent();
  }
  
  if(box)
    return index;
  else
    return fallback;
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

int Box::child_script_level(int index, const int *opt_ambient_script_level) {
  int ambient_script_level;
  if(style) {
    auto all = stylesheet();
    if(all) {
      if(all->get(style, ScriptLevel, &ambient_script_level))
        return ambient_script_level;
      
      auto defn = get_default_key(ScriptLevel);
      if(defn != ScriptLevel) {
        if(all->get(style, defn, &ambient_script_level))
          return true;
        
        StyledObject *obj = style_parent();
        while(obj) {
          if(obj->changes_children_style()) {
            if(all->get(style, defn, &ambient_script_level))
              return ambient_script_level;
          }
          
          obj = obj->style_parent();
        }
      }
    }
    else if(style.get(ScriptLevel, &ambient_script_level))
      return ambient_script_level;
  }
  
  if(opt_ambient_script_level)
    ambient_script_level = *opt_ambient_script_level;
  else if(auto p = parent())
    ambient_script_level = p->child_script_level(_index, nullptr);
  else
    ambient_script_level = 0;
  
  return ambient_script_level;
}

DynamicUpdateKind Box::update_dynamic_styles(Evaluator evaluator, Context &context) {
  if(context.stylesheet)
    return context.stylesheet->update_dynamic(style, this, evaluator);
  
  return DynamicUpdateKindNone;
}

DynamicUpdateKind Box::update_simple_dynamic_styles_on_resize(Context &context) {
  DynamicUpdateKind result = update_dynamic_styles(Evaluator::Simple, context);
  
//  if(result == DynamicUpdateKindLayout)
//    on_style_changed(true);
//  else if(result == DynamicUpdateKindPaint)
//    on_style_changed(false);
  return result;
}

DynamicUpdateKind Box::update_dynamic_styles_on_paint(Context &context) {
  DynamicUpdateKind result = update_dynamic_styles(Evaluator::Full, context);
  
  if(result == DynamicUpdateKindLayout)
    on_style_changed(true);
//  else if(result == DynamicUpdateKindPaint)
//    on_style_changed(false);
  return result;
}

void Box::colorize_scope(SyntaxState &state) {
  for(int i = 0; i < count(); ++i)
    item(i)->colorize_scope(state);
}

void Box::before_paint_inline(Context &context) {
  for(int i = 0; i < count(); ++i)
    item(i)->before_paint_inline(context);
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

void Box::selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, int start, int end) {
  if(end > count())
    end = count();
  
  if(auto mseq = as_inline_span()) {
    int ii = mseq->index();
    if(start <= ii && ii < end) {
//      if(auto par = dynamic_cast<MathSequence*>(parent())) {
//        int i = index();
//        par->selection_rectangles(rects, flags, p0 /* TODO: + ... */, i, i + 1);
//        return;
//      }
      mseq->selection_rectangles(rects, flags, p0, 0, mseq->length());
      return;
    }
  }
  
  if(auto tseq = as_inline_text_span()) {
    int ii = tseq->index();
    if(start <= ii && ii < end) {
//      if(auto par = dynamic_cast<TextSequence*>(parent())) {
//        int i = index();
//        par->selection_rectangles(rects, flags, p0 /* TODO: + ... */, i, i + 1);
//        return;
//      }
      tseq->selection_rectangles(rects, flags, p0, 0, tseq->length());
      return;
    }
  }
  
  for(int i = start; i < end; ++i) 
    rects.add(item(i)->extents().to_rectangle(p0));
}

bool Box::scroll_to(const RectangleF &rect) {
  return false;
}

bool Box::scroll_to(Canvas &canvas, const VolatileSelection &child) {
  if(auto par = parent())
    return par->scroll_to(canvas, child);
  return false;
}

bool Box::default_scroll_to(Canvas &canvas, Box *scroll_view, const VolatileSelection &child_sel) {
  RectangleF rect;
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
    rect = canvas.stroke_extents();
    
    canvas.new_path();
  }
  canvas.restore();
  
  Canvas::transform_rect_inline(mat, rect);
  bool any_scroll = scroll_to(rect);
  
  if(auto par = parent())
    return any_scroll | par->scroll_to(canvas, child_sel);

  return  any_scroll;
}

Expr Box::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::LimitedDepth)) {
    if(max_box_output_depth <= 0)
      return id().to_pmath();
    
    AutoValueReset<int> auto_mbod(max_box_output_depth);
    --max_box_output_depth;
    
    return to_pmath_impl(flags);
  }
  return to_pmath_impl(flags);
}

Expr Box::to_pmath(BoxOutputFlags flags, int start, int end) {
  if(has(flags, BoxOutputFlags::LimitedDepth)) {
    if(max_box_output_depth <= 0)
      return id().to_pmath();
    
    AutoValueReset<int> auto_mbod(max_box_output_depth);
    --max_box_output_depth;
    
    return to_pmath_impl(flags, start, end);
  }
  return to_pmath_impl(flags, start, end);
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

void Box::after_inline_span_mouse_selection(Box *top, VolatileSelection &sel, bool &was_inside_start) {
  if(top == this)
    return;
  
  if(auto par = parent())
    return par->after_inline_span_mouse_selection(top, sel, was_inside_start);
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
    return par->prepare_dynamic(PMATH_CPP_MOVE(expr));
  return expr;
}

void Box::dynamic_updated() {
  if(style) {
    style.flag_pending_dynamic();
    request_repaint_all();
  }
}

VolatileSelection Box::dynamic_to_literal(int start, int end) {
  for(int b = start; b < end; ++b) 
    item(b)->all_dynamic_to_literal();
  
  return {this, start, end};
}

bool Box::request_repaint_all() {
  if(auto seq = as_inline_span()) {
    if(seq->inline_span()) 
      return seq->request_repaint_all();
  }
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
  
  if(auto seq = as_inline_span()) {
    if(seq->inline_span()) { // i.e. layout is up to date.
      // seq is a child of this box, but we avoid infinite recursion because 
      // MathSequence::visible_rect() checks for inline_span().
      return seq->visible_rect(rect, top_most);
    }
  }
  
  if(!_extents.to_rectangle().overlaps(rect, 0.0001)) {
    // Note that inline-span boxes have empty/invalid _extents, but these are checked above.
    return false;
  }
  
  if(auto par = parent()) { // TODO: simplify for inline_span MathSequence ...
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

void Box::on_style_changed(bool layout_affected) {
  if(layout_affected)
    invalidate();
  else
    request_repaint_all();
}

bool Box::edit_selection(SelectionReference &selection, EditAction action) {
  if(!get_own_style(Editable, true))
    return false;
    
  if(auto par = parent())
    return par->edit_selection(selection, action);
  
  return true;
}

bool Box::editable() {
  SelectionReference sel {this, 0, length()};
  return edit_selection(sel, EditAction::DryRun);
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
    style.notify_all();
  }
}

void Box::on_mouse_exit() {
  auto i = get_own_style(InternalUsesCurrentValueOfMouseOver, ObserverKindNone);
  if(i & ObserverKindSelf)
    dynamic_updated();
  
  if(i & ObserverKindOther) {
    style.notify_all();
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
  RICHMATH_ASSERT(child != nullptr);
  RICHMATH_ASSERT(child->_parent_or_limbo_next.is_normal());
  RICHMATH_ASSERT(child->parent() == nullptr || child->parent() == this);
  child->_parent_or_limbo_next.set_to_normal(this);
  child->_index = i;
}

void Box::abandon(Box *child) {
  RICHMATH_ASSERT(child != nullptr);
  RICHMATH_ASSERT(child->parent() == this);
  
  if(child->_parent_or_limbo_next.is_normal())
    child->_parent_or_limbo_next.set_to_normal(nullptr);
  
  child->_index = 0;
}

FunctionChain<Box*, Expr> *Box::on_finish_load_from_object = nullptr;

void Box::finish_load_from_object(Expr expr) {
  Stylesheet::update_box_registry(this);
  
  FunctionChain<Box*, Expr> *event = on_finish_load_from_object;
  while(event) {
    if(event->func)
      event->func(this, expr);
    event = event->next;
  }
}

void Box::next_in_limbo(ObjectWithLimbo *next) {
  RICHMATH_ASSERT( _parent_or_limbo_next.is_normal() );
  _parent_or_limbo_next.set_to_tinted(next);
}
      
//} ... class Box

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
  RICHMATH_ASSERT(parent);
  RICHMATH_ASSERT(box);
  
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
