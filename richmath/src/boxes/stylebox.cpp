#include <boxes/stylebox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_StyleBox;
extern pmath_symbol_t richmath_System_TagBox;

//{ class AbstractStyleBox ...

AbstractStyleBox::AbstractStyleBox(MathSequence *content)
  : OwnerBox(content)
{
}

void AbstractStyleBox::paint_or_resize_no_baseline(Context *context, bool paint) {
  if(style) {
    float x, y;
    context->canvas->current_pos(&x, &y);
    
    ContextState cc(context);
    cc.begin(style);
    
    Color c;
    if(paint) {
      if(context->stylesheet->get(style, Background, &c)) {
        if(c.is_valid()) {
          if(context->canvas->show_only_text)
            return;
          
          Rectangle rect(x, y - _extents.ascent, _extents.width, _extents.height());
          BoxRadius radii;
          
          Expr expr;
          if(context->stylesheet->get(style, BorderRadius, &expr))
            radii = BoxRadius(expr);
          
          rect.normalize();
          rect.pixel_align(*context->canvas, false, +1);
          
          radii.normalize(rect.width, rect.height);
          rect.add_round_rect_path(*context->canvas, radii, false);
          
          context->canvas->set_color(c);
          context->canvas->fill();
        }
      }
      
      c = cc.old_color;
      context->stylesheet->get(style, FontColor, &c);
      context->canvas->set_color(c);
      
      context->canvas->move_to(x, y);
      OwnerBox::paint(context);
    }
    else 
      OwnerBox::resize_default_baseline(context);
    
    cc.end();
  }
  else if(paint) 
    OwnerBox::paint(context);
  else 
    OwnerBox::resize_default_baseline(context);
}

void AbstractStyleBox::resize_default_baseline(Context *context) {
  paint_or_resize_no_baseline(context, false);
}

void AbstractStyleBox::paint(Context *context) {
  paint_or_resize_no_baseline(context, true);
}

void AbstractStyleBox::colorize_scope(SyntaxState *state) {
  if(get_own_style(FontColor).is_valid()) 
    return;
    
  OwnerBox::colorize_scope(state);
}

Box *AbstractStyleBox::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(style && _parent) {
    if(!get_own_style(Selectable, true)) {
      if(direction == LogicalDirection::Forward)
        *index = _index + 1;
      else
        *index = _index;
        
      return _parent;
    }
  }
  
  return OwnerBox::move_logical(direction, jumping, index);
}

Box *AbstractStyleBox::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  if(style && *index < 0) { // called from parent
    if(!get_own_style(Selectable, true)) {
      if(*index_rel_x <= _extents.width)
        *index = _index;
      else
        *index = _index + 1;
        
      return _parent;
    }
  }
  
  return OwnerBox::move_vertical(direction, index_rel_x, index, called_from_child);
}

Box *AbstractStyleBox::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  if(_parent) {
    if(get_own_style(Placeholder)) {
      *start = _index;
      *end = _index + 1;
      *was_inside_start = x >= 0 && x <= _extents.width;
      return _parent;
    }
    
    if(y < -_extents.ascent || y > _extents.descent || !get_own_style(Selectable, true)) {
      if(x <= _extents.width / 2) {
        *start = *end = _index;
        *was_inside_start = true;
      }
      else {
        *start = *end = _index + 1;
        *was_inside_start = false;
      }
      
      return _parent;
    }
  }
  
  return OwnerBox::mouse_selection(x, y, start, end, was_inside_start);
}

//} ... class AbstractStyleBox

//{ class ExpandableAbstractStyleBox ...

bool ExpandableAbstractStyleBox::expand(const BoxSize &size) {
  BoxSize size2 = size;
  float r = _extents.width - _content->extents().width - cx;
  float t = _extents.ascent  - _content->extents().ascent;
  float b = _extents.descent - _content->extents().descent;
  size2.width -= cx + r;
  size2.ascent -= t;
  size2.descent -= b;
  
  if(_content->expand(size)) {
    _extents = _content->extents();
    _extents.width += r;
    _extents.ascent += t;
    _extents.descent += b;
    
    return true;
  }
  
  return false;
}

//} ... class ExpandableAbstractStyleBox

//{ class StyleBox ...

StyleBox::StyleBox(MathSequence *content)
  : ExpandableAbstractStyleBox(content)
{
  style = new Style;
}

bool StyleBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_StyleBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
  
  Expr options;
  
  if(expr[2].is_string())
    options = Expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  else
    options = Expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
    
  if(options.is_null()) 
    return false;
    
  /* now success is guaranteed */
  
  if(style)
    style->clear();
  
  if(expr[2].is_string()) {
    if(!style)
      style = new Style();
    
    style->set_pmath(BaseStyleName, expr[2]);
  }
  
  if(options != PMATH_UNDEFINED) {
    if(style)
      style->add_pmath(options);
    else
      style = new Style(options);
      
    int i;
    if(style->get(AutoNumberFormating, &i)) {
      if(i)
        opts |= BoxInputFlags::FormatNumbers;
      else
        opts -= BoxInputFlags::FormatNumbers;
    }
  }
  
  _content->load_from_object(expr[1], opts);
  
  finish_load_from_object(std::move(expr));
  return true;
}

Expr StyleBox::to_pmath_symbol() { 
  return Symbol(richmath_System_StyleBox); 
}

Expr StyleBox::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Parseable) && get_own_style(StripOnInput, true)) {
    return _content->to_pmath(flags);
  }
  
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  Expr e;
  bool with_inherited = true;
  if(style && !style->get_dynamic(BaseStyleName, &e)) {
    String s;
    if(style->get(BaseStyleName, &s)) {
      with_inherited = false;
      Gather::emit(s);
    }
  }
  style->emit_to_pmath(with_inherited);
  
  e = g.end();
  e.set(0, Symbol(richmath_System_StyleBox));
  return e;
}

//} ... class StyleBox

//{ class TagBox ...

TagBox::TagBox(MathSequence *content)
  : ExpandableAbstractStyleBox(content)
{
  style = new Style();
}

TagBox::TagBox(MathSequence *content, Expr _tag)
  : ExpandableAbstractStyleBox(content),
  tag(_tag)
{
  style = new Style();
}

bool TagBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_TagBox)
    return false;
  
  if(expr.expr_length() < 2)
    return false;
  
  Expr options_expr(pmath_options_extract_ex(expr.get(), 2, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options_expr.is_null())
    return false;
    
  /* now success is guaranteed */
  
  tag = expr[2];
  
  if(style){
    style->clear();
    style->add_pmath(options_expr);
  }
  else if(options_expr != PMATH_UNDEFINED)
    style = new Style(options_expr);
  
  int i;
  if(style->get(AutoNumberFormating, &i)) {
    if(i)
      opts |= BoxInputFlags::FormatNumbers;
    else
      opts -= BoxInputFlags::FormatNumbers;
  }
  
  _content->load_from_object(expr[1], opts);
  
  finish_load_from_object(std::move(expr));
  return true;
}

void TagBox::resize_default_baseline(Context *context) {
  style->set(BaseStyleName, String(tag));
  ExpandableAbstractStyleBox::resize_default_baseline(context);
}

Expr TagBox::to_pmath_symbol() { 
  return Symbol(richmath_System_TagBox); 
}

Expr TagBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  g.emit(tag);
  if(style)
    style->emit_to_pmath(false);
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_TagBox));
  return e;
}

//} ... class TagBox
