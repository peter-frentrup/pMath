#include <boxes/fillbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class FillBox ...

FillBox::FillBox(MathSequence *content, float _weight)
  : OwnerBox(content),
  weight(_weight)
{
}

FillBox::~FillBox() {
}

FillBox *FillBox::create(Expr expr, int opts) {
  if(!expr.is_expr())
    return 0;
    
  if(expr.expr_length() < 1 || expr.expr_length() > 2)
    return 0;
    
  FillBox *result = new FillBox();
  result->_content->load_from_object(expr[1], opts);
  result->weight = expr[2].to_double(1.0);
  
  return result;
}

bool FillBox::expand(const BoxSize &size) {
  _content->expand(size);
  _extents = _content->extents();
  _extents.merge(size);
  cx = 0;
  return true;
}

void FillBox::paint_content(Context *context) {
  if(_content->extents().width > 0) {
    float x, y;
    context->canvas->current_pos(&x, &y);
    
    int i = (int)(_extents.width / _content->extents().width);
    
    while(i-- > 0) {
      context->canvas->move_to(x, y);
      
      _content->paint(context);
      
      x += _content->extents().width;
    }
  }
}

Expr FillBox::to_pmath(int flags) {
  if(flags & BoxFlagParseable)
    return _content->to_pmath(flags);
    
  if(weight == 1) {
    return Call(
             Symbol(PMATH_SYMBOL_FILLBOX),
             _content->to_pmath(flags));
  }
  
  return Call(
           Symbol(PMATH_SYMBOL_FILLBOX),
           _content->to_pmath(flags),
           Number(weight));
}

Box *FillBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index
) {
  if(_content->extents().width > 0 && *index < 0) {
    *index_rel_x -= cx;
    *index_rel_x = fmodf(*index_rel_x, _content->extents().width);
    return _content->move_vertical(direction, index_rel_x, index);
  }
  
  return OwnerBox::move_vertical(direction, index_rel_x, index);
}

Box *FillBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  x -= cx;
  y -= cy;
  
  if(_content->extents().width > 0) {
    x = fmodf(x, _content->extents().width);
  }
  return _content->mouse_selection(x, y, start, end, was_inside_start);
}

bool FillBox::request_repaint(float x, float y, float w, float h) {
  int num_repititions = (int)(_extents.width / _content->extents().width);
  
  if(num_repititions > 1){
    x = 0.0;
    y = -_extents.ascent;
    w = _extents.width + 0.1;
    h = _extents.height() + 0.1;
  }
  
  
  return OwnerBox::request_repaint(x, y, w, h);
}

//} ... class FillBox
