#include <boxes/stylebox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

//{ class AbstractStyleBox ...

AbstractStyleBox::AbstractStyleBox(MathSequence *content)
: OwnerBox(content)
{
}

void AbstractStyleBox::paint_or_resize(Context *context, bool paint){
  if(style){
    float x, y;
    context->canvas->current_pos(&x, &y);
    
    int i;
    ContextState cc(context);
    cc.begin(style);
    
    if(paint){
      if(context->stylesheet->get(style, Background, &i)){
        if(i >= 0){
          if(context->canvas->show_only_text)
            return;
          
          context->canvas->set_color(i);
          context->canvas->pixrect(
            x, 
            y - _extents.ascent, 
            x + _extents.width,
            y + _extents.descent,
            false);
          
          context->canvas->fill();
        }
      }
      
      i = cc.old_color;
      context->stylesheet->get(style, FontColor, &i);
      context->canvas->set_color(i);
      
      context->canvas->move_to(x, y);
      OwnerBox::paint(context);
    }
    else{
      OwnerBox::resize(context);
    }
    
    cc.end();
  }
  else if(paint)
    OwnerBox::paint(context);
  else
    OwnerBox::resize(context);
}

void AbstractStyleBox::resize(Context *context){
  paint_or_resize(context, false);
}

void AbstractStyleBox::paint(Context *context){
  paint_or_resize(context, true);
}

void AbstractStyleBox::colorize_scope(SyntaxState *state){
  if(show_auto_styles)
    OwnerBox::colorize_scope(state);
}

Box *AbstractStyleBox::move_logical(
  LogicalDirection  direction, 
  bool              jumping, 
  int              *index
){
  if(style && _parent){
    if(!get_own_style(Selectable, true)){
      if(direction == Forward)
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
  int              *index
){
  if(style && *index < 0){
    if(!get_own_style(Selectable, true)){
      if(*index_rel_x <= _extents.width)
        *index = _index;
      else
        *index = _index + 1;
      
      return _parent;
    }
  }
  
  return OwnerBox::move_vertical(direction, index_rel_x, index);
}

Box *AbstractStyleBox::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  if(style && _parent){
    if(get_own_style(Placeholder)){
      *start = _index;
      *end = _index + 1;
      return _parent;
    }
    
    if(!get_own_style(Selectable, true)){
      if(x <= _extents.width / 2)
        *start = *end = _index;
      else
        *start = *end = _index + 1;
        
      return _parent;
    }
  }
  
  return OwnerBox::mouse_selection(x, y, start, end, eol);
}

//} ... class AbstractStyleBox

//{ class StyleBox ...

StyleBox::StyleBox(MathSequence *content)
: AbstractStyleBox(content)
{
  style = new Style;
}

StyleBox *StyleBox::create(Expr expr, int opts){
  Expr options;
  
  if(expr[2].instance_of(PMATH_TYPE_STRING))
    options = Expr(pmath_options_extract(expr.get(), 2));
  else
    options = Expr(pmath_options_extract(expr.get(), 1));
    
  if(options.is_valid()){
    StyleBox *box = new StyleBox;
    
    if(expr[2].instance_of(PMATH_TYPE_STRING)){
      if(!box->style)
        box->style = new Style();
      box->style->set_pmath_string(BaseStyleName, expr[2]);
    }
    
    if(options != PMATH_UNDEFINED){
      if(box->style)
        box->style->add_pmath(options);
      else
        box->style = new Style(options);
      
      int i;
      if(box->style->get(AutoNumberFormating, &i)){
        if(i)
          opts|= BoxOptionFormatNumbers;
        else
          opts&= ~BoxOptionFormatNumbers;
      }
    }
    
    box->content()->load_from_object(expr[1], opts);
    
    return box;
  }
  
  return 0;
}

bool StyleBox::expand(const BoxSize &size){
  BoxSize size2 = size;
  float r = _extents.width - _content->extents().width - cx;
  float t = _extents.ascent  - _content->extents().ascent;
  float b = _extents.descent - _content->extents().descent;
  size2.width-= cx + r;
  size2.ascent-= t;
  size2.descent-= b;
  
  cy-= _content->extents().ascent;
  if(_content->expand(size)){
    _extents = _content->extents();
    _extents.width+= r;
    _extents.ascent+= t;
    _extents.descent+= b;
    
    cy+= _content->extents().ascent;
    return true;
  }
  
  cy+= _content->extents().ascent;
  return false;
}

pmath_t StyleBox::to_pmath(bool parseable){
  pmath_gather_begin(0);
  
  pmath_emit(_content->to_pmath(parseable), 0);
  
  style->emit_to_pmath(false, true);
  
  return pmath_expr_set_item(
    pmath_gather_end(), 0,
    pmath_ref(PMATH_SYMBOL_STYLEBOX));
}

//} ... class StyleBox

//{ class TagBox ...

TagBox::TagBox()
: AbstractStyleBox()
{
  style = new Style();
}

TagBox::TagBox(MathSequence *content)
: AbstractStyleBox(content)
{
  style = new Style();
}

TagBox::TagBox(MathSequence *content, Expr _tag)
: AbstractStyleBox(content),
  tag(_tag)
{
  style = new Style();
}

bool TagBox::expand(const BoxSize &size){
  BoxSize size2 = size;
  float r = _extents.width - _content->extents().width - cx;
  float t = _extents.ascent  - _content->extents().ascent;
  float b = _extents.descent - _content->extents().descent;
  size2.width-= cx + r;
  size2.ascent-= t;
  size2.descent-= b;
  
  cy-= _content->extents().ascent;
  if(_content->expand(size)){
    _extents = _content->extents();
    _extents.width+= r;
    _extents.ascent+= t;
    _extents.descent+= b;
    
    cy+= _content->extents().ascent;
    return true;
  }
  
  cy+= _content->extents().ascent;
  return false;
}

void TagBox::resize(Context *context){
  style->set(BaseStyleName, String(tag));
  AbstractStyleBox::resize(context);
}

pmath_t TagBox::to_pmath(bool parseable){
  pmath_gather_begin(0);
  
  pmath_emit(_content->to_pmath(parseable), 0);
  pmath_emit(pmath_ref(tag.get()), 0);
  
  int i;
  if(style && style->get(AutoDelete, &i)){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_EDITABLE),
        pmath_ref(i ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)),
      NULL);
  }
  
  if(style && style->get(Editable, &i) && i){
    pmath_emit(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_EDITABLE),
        pmath_ref(PMATH_SYMBOL_TRUE)),
      NULL);
  }
  
  return pmath_expr_set_item(
    pmath_gather_end(), 0,
    pmath_ref(PMATH_SYMBOL_TAGBOX));
}

//} ... class TagBox
