#include <boxes/fillbox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

extern pmath_symbol_t richmath_System_FillBox;

//{ class FillBox ...

FillBox::FillBox(MathSequence *content)
  : OwnerBox(content),
  _weight(1.0f)
{
}

FillBox::~FillBox() {
}

bool FillBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_FillBox)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
  
  Expr options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options.is_null())
    return false;
    
  /* now success is guaranteed */
  if(style) {
    reset_style();
    style->add_pmath(options);
  }
  else if(options != PMATH_UNDEFINED)
    style = new Style(options);
  
  _content->load_from_object(expr[1], opts);
  _weight = 1.0f; // loaded from style on resize
  
  finish_load_from_object(std::move(expr));
  return true;
}

bool FillBox::expand(const BoxSize &size) {
  _content->expand(size);
  _extents = _content->extents();
  _extents.merge(size);
  cx = 0;
  return true;
}

void FillBox::paint_content(Context &context) {
  if(_content->extents().width > 0) {
    float x, y;
    context.canvas().current_pos(&x, &y);
    
    int i = (int)(_extents.width / _content->extents().width);
    
    while(i-- > 0) {
      context.canvas().move_to(x, y);
      
      _content->paint(context);
      
      x += _content->extents().width;
    }
  }
}

Expr FillBox::to_pmath_symbol() {
  return Symbol(richmath_System_FillBox); 
}

Expr FillBox::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Parseable) && get_own_style(StripOnInput, true))
    return _content->to_pmath(flags);
  
  if(!style)
    return Call(Symbol(richmath_System_FillBox), _content->to_pmath(flags));
  
  Gather g;
  g.emit(_content->to_pmath(flags));
  style->emit_to_pmath(false);

  Expr expr = g.end();
  expr.set(0, Symbol(richmath_System_FillBox));
  return expr;
}

Box *FillBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(_content->extents().width > 0 && *index < 0) {
    *index_rel_x -= cx;
    *index_rel_x = fmodf(*index_rel_x, _content->extents().width);
    return _content->move_vertical(direction, index_rel_x, index, false);
  }
  
  return base::move_vertical(direction, index_rel_x, index, called_from_child);
}

VolatileSelection FillBox::mouse_selection(Point pos, bool *was_inside_start) {
  pos -= Vector2F{ cx, cy };
  if(_content->extents().width > 0) {
    pos.x = fmodf(pos.x, _content->extents().width);
  }
  return _content->mouse_selection(pos, was_inside_start);
}

bool FillBox::request_repaint(const RectangleF &rect) {
  int num_repititions = (int)(_extents.width / _content->extents().width);
  
  if(num_repititions > 1) 
    base::request_repaint(_extents.to_rectangle());
  
  return base::request_repaint(rect);
}

void FillBox::resize_default_baseline(Context &context) {
  _weight = get_own_style(FillBoxWeight, 1.0f);
  if(!(_weight > 0.0))
    _weight = 0.0;
  
  base::resize_default_baseline(context);
}
      

//} ... class FillBox
