#include <boxes/box.h>

#include <graphics/context.h>
#include <gui/native-widget.h>

using namespace pmath;
using namespace richmath;

static Hashtable<int, Box*, cast_hash> box_cache;

static int global_id = 0;

//{ class MouseEvent ...

void MouseEvent::set_source(Box *new_source){
  if(source == new_source)
    return;
  
  Box *common = 0;//Box::common_parent(source, new_source);
  
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  
  if(new_source){
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
: Base(),
  _extents(0,0,0),
  _parent(0),
  _index(0),
  _id(++global_id)
{
  box_cache.set(_id, this);
}

Box::~Box(){
  box_cache.remove(_id);
}

Box *Box::find(int id){
  return box_cache[id];
}

Box *Box::find(Expr frontendobject){
  if(frontendobject.expr_length() == 1
  && frontendobject[0] == PMATH_SYMBOL_FRONTENDOBJECT){
    Expr num = frontendobject[1];
    
    if(num.instance_of(PMATH_TYPE_INTEGER)
    && pmath_integer_fits_si(num.get()))
      return find(pmath_integer_get_si(num.get()));
  }
  
  return 0;
}

void Box::swap_id(Box *other){
  if(other){
    int id = other->_id;
    other->_id = this->_id;
    this->_id  = id;
    box_cache.set(other->_id, other);
    box_cache.set(this->_id,  this);
  }
}

bool Box::is_parent_of(Box *child){
  while(child && child != this)
    child = child->parent();
  
  return child == this;
}

Box *Box::common_parent(Box *a, Box *b){
  int d1 = 0;
  Box *tmp = a;
  while(tmp){
    tmp = tmp->_parent;
    ++d1;
  }
  
  int d2 = 0;
  tmp = b;
  while(tmp){
    tmp = tmp->_parent;
    ++d2;
  }
  
  while(d1 > d2){
    a = a->_parent;
    --d1;
  }
  
  while(d1 < d2){
    b = b->_parent;
    --d2;
  }
  
  while(a != b){
    a = a->_parent;
    b = b->_parent;
  }
  
  return a;
}

void Box::colorize_scope(SyntaxState *state){
  for(int i = 0;i < count();++i)
    item(i)->colorize_scope(state);
}

void Box::clear_coloring(){
  for(int i = 0;i < count();++i)
    item(i)->clear_coloring();
}

Box *Box::move_logical(
  LogicalDirection  direction, 
  bool              jumping, 
  int              *index
){
  if(direction == Forward){
    int b = *index;
    if(b < 0 || jumping)
      ++b;
    
    if(b < count()){
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
  
  if(b >= 0){
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
){
  if(_parent){
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index);
  }
  
  return this;
}

Box *Box::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  *eol = false;
  if(_parent){
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
){
}

void Box::transformation(
  Box            *parent, 
  cairo_matrix_t *matrix
){
  if(_parent && parent != this){
    _parent->transformation(parent, matrix);
    
    _parent->child_transformation(_index, matrix);
  }
}

bool Box::selectable(int i){
  if(_parent)
    return _parent->selectable(_index);
  
  int result;
  
  if(style && style->get(Selectable, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    if(all->get(style, Selectable, &result))
      return result;
    
    if(all->base && all->base->get(Selectable, &result))
      return result;
  }
  
  return true;
}

Box *Box::normalize_selection(int *start, int *end){
  if(_parent){
    *start = _index;
    *end = _index + 1;
    return _parent->normalize_selection(start, end);
  }
  
  return 0;
}

bool Box::request_repaint_all(){
  return request_repaint(0, -_extents.ascent, _extents.width, _extents.height());
}

bool Box::request_repaint(float x, float y, float w, float h){
  if(x >= _extents.width 
  || x + w <= 0 
  || y >= _extents.descent 
  || y + h <= -_extents.ascent)
    return false;
  
  if(_parent){
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    transformation(_parent, &matrix);
    
    Canvas::transform_rect(matrix, &x, &y, &w, &h);
    return _parent->request_repaint(x, y, w, h);
  }
  
  return false;
}

void Box::invalidate(){
  if(_parent)
    _parent->invalidate();
}

bool Box::edit_selection(Context *context){
  int editable;
  
  if(context->stylesheet){
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

SharedPtr<Stylesheet> Box::stylesheet(){
  if(!_parent)
    return Stylesheet::Default;
    
  return _parent->stylesheet();
}

int Box::get_style(IntStyleOptionName n, int result){
  Box *box;
  
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    box = this;
    do{
      if(box->changes_children_style()
      && all->get(box->style, n, &result))
        return result;
      
      box = box->_parent;
    }while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else{
    box = _parent;
    while(box){
      if(box->changes_children_style()
      && box->style 
      && box->style->get(n, &result))
        return result;
      
      box = box->_parent;
    }
  }
  
  return result;
}

float Box::get_style(FloatStyleOptionName n, float result){
  Box *box;
  
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    box = this;
    do{
      if(box->changes_children_style()
      && all->get(box->style, n, &result))
        return result;
      
      box = box->_parent;
    }while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else{
    box = _parent;
    while(box){
      if(box->changes_children_style()
      && box->style 
      && box->style->get(n, &result))
        return result;
      
      box = box->_parent;
    }
  }
  
  return result;
}

String Box::get_style(StringStyleOptionName n, String result){
  Box  *box;
  
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    box = this;
    do{
      if(box->changes_children_style()
      && all->get(box->style, n, &result))
        return result;
      
      box = box->_parent;
    }while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else{
    box = _parent;
    while(box){
      if(box->changes_children_style()
      && box->style 
      && box->style->get(n, &result))
        return result;
      
      box = box->_parent;
    }
  }
  
  return result;
}

Expr Box::get_style(ObjectStyleOptionName n, Expr result){
  Box  *box;
  
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    box = this;
    do{
      if(box->changes_children_style()
      && all->get(box->style, n, &result))
        return result;
      
      box = box->_parent;
    }while(box);
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  else{
    box = _parent;
    while(box){
      if(box->changes_children_style()
      && box->style 
      && box->style->get(n, &result))
        return result;
      
      box = box->_parent;
    }
  }
  
  return result;
}

String Box::get_style(StringStyleOptionName n){
  return get_style(n, String());
}

Expr Box::get_style(ObjectStyleOptionName n){
  return get_style(n, Expr());
}

int Box::get_own_style(IntStyleOptionName n, int result){
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    if(all->get(style, n, &result))
      return result;
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

float Box::get_own_style(FloatStyleOptionName n, float result){
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    if(all->get(style, n, &result))
      return result;
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

String Box::get_own_style(StringStyleOptionName n, String result){
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    if(all->get(style, n, &result))
      return result;
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

Expr Box::get_own_style(ObjectStyleOptionName n, Expr result){
  if(style && style->get(n, &result))
    return result;
  
  SharedPtr<Stylesheet> all = stylesheet();
  if(all){
    if(all->get(style, n, &result))
      return result;
    
    if(all->base && all->base->get(n, &result))
      return result;
  }
  
  return result;
}

String Box::get_own_style(StringStyleOptionName n){
  return get_own_style(n, String());
}

Expr Box::get_own_style(ObjectStyleOptionName n){
  return get_own_style(n, Expr());
}

//} ... styles

//{ event handlers ...

Box *Box::mouse_sensitive(){
  if(_parent)
    return _parent->mouse_sensitive();
  return 0;
}

void Box::on_mouse_enter(){
}

void Box::on_mouse_exit(){
}

void Box::on_mouse_down(MouseEvent &event){
}

void Box::on_mouse_move(MouseEvent &event){
}

void Box::on_mouse_up(MouseEvent &event){
}

void Box::on_mouse_cancel(){
}

void Box::on_enter(){
//  if(_parent)
//    _parent->on_enter();
}

void Box::on_exit(){
//  if(_parent)
//    _parent->on_exit();
}

void Box::on_key_down(SpecialKeyEvent &event){
  if(_parent)
    _parent->on_key_down(event);
}

void Box::on_key_up(SpecialKeyEvent &event){
  if(_parent)
    _parent->on_key_up(event);
}

void Box::on_key_press(uint32_t unichar){
  if(_parent)
    _parent->on_key_press(unichar);
}

//} ... event handlers
      
void Box::adopt(Box *child, int i){
  assert(child != 0);
  assert(child->_parent == 0 || child->_parent == this);
  child->_parent = this;
  child->_index = i;
}

void Box::abandon(Box *child){
  assert(child != 0);
  assert(child->_parent == this);
  child->_parent = 0;
  child->_index = 0;
}

//} ... class Box
