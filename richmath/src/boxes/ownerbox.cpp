#include <boxes/ownerbox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class OwnerBox ...

OwnerBox::OwnerBox(MathSequence *content)
  : Box(),
  _content(content)
{
  if(!_content)
    _content = new MathSequence;
    
  adopt(_content, 0);
}

OwnerBox::~OwnerBox() {
  delete _content;
}

Box *OwnerBox::item(int i) {
  return _content;
}

void OwnerBox::resize(Context *context) {
  _content->resize(context);
  _extents = _content->extents();
  cx = 0;
  cy = 0;
}

void OwnerBox::paint(Context *context) {
  if(style)
    style->update_dynamic(this);
    
  paint_content(context);
}

void OwnerBox::paint_content(Context *context) {
  context->canvas->rel_move_to(cx, cy);
  
  Expr expr;
  if(style && context->stylesheet->get(style, TextShadow, &expr))
    context->draw_with_text_shadows(_content, expr);
  else
    _content->paint(context);
}

Box *OwnerBox::remove(int *index) {
  if(_parent) {
    MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
    *index = _index;
    if(seq)
      seq->insert(_index + 1, _content, 0, _content->length());
    return _parent->remove(index);
  }
  *index = 0;
  return _content;
}

Expr OwnerBox::to_pmath(int flags) {
  return _content->to_pmath(flags);
}

Box *OwnerBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index
) {
  if(*index < 0) {
    *index_rel_x -= cx;
    return _content->move_vertical(direction, index_rel_x, index);
  }
  
  return Box::move_vertical(direction, index_rel_x, index);
}

Box *OwnerBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  x -= cx;
  y -= cy;
  return _content->mouse_selection(x, y, start, end, was_inside_start);
}

void OwnerBox::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  cairo_matrix_translate(matrix,
                         cx,
                         cy/* + _extents.ascent*/);
}

bool OwnerBox::edit_selection(Context *context) {
  if(Box::edit_selection(context)) {
    int auto_delete;
    
    if(context->stylesheet) {
      if(!context->stylesheet->get(style, AutoDelete, &auto_delete))
        auto_delete = 0;
    }
    else if(!style || !style->get(AutoDelete, &auto_delete))
      auto_delete = 0;
      
    Box *selbox = context->selection.get();
    if(auto_delete && selbox != this) {
      MathSequence *seq = dynamic_cast<MathSequence*>(_parent);
      
      if(seq) {
        if(selbox == _content) {
          context->selection.set(seq,
                                 context->selection.start + _index,
                                 context->selection.end   + _index);
        }
        
        seq->insert(_index + 1, _content, 0, _content->length());
        seq->remove(_index, _index + 1);
      }
    }
    
    return true;
  }
  
  return false;
}

//} ... class OwnerBox
