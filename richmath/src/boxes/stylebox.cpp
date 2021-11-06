#include <boxes/stylebox.h>

#include <cmath>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_StyleBox;
extern pmath_symbol_t richmath_System_TagBox;

//{ class AbstractStyleBox ...

AbstractStyleBox::AbstractStyleBox(AbstractSequence *content)
  : ExpandableOwnerBox(content)
{
}

void AbstractStyleBox::paint_or_resize_no_baseline(Context &context, bool paint) {
  if(style) {
    Point p0 = context.canvas().current_pos();
    
    ContextState cc(context);
    cc.begin(style);
    
    if(paint) {
      if(Color c = context.stylesheet->get_or_default(style, Background)) {
        if(context.canvas().show_only_text) {
          cc.end();
          return;
        }
        
        RectangleF rect = _extents.to_rectangle(p0);
        BoxRadius radii;
        
        if(Expr radii_expr = context.stylesheet->get_or_default(style, BorderRadius)) 
          radii = BoxRadius(std::move(radii_expr));
        
        rect.normalize();
        rect.pixel_align(context.canvas(), false, +1);
        
        radii.normalize(rect.width, rect.height);
        rect.add_round_rect_path(context.canvas(), radii, false);
        
        Color old_color = context.canvas().get_color();
        context.canvas().set_color(c);
        context.canvas().fill();
        context.canvas().set_color(old_color);
      }
      
      if(Color c = context.stylesheet->get_or_default(style, FontColor))
        context.canvas().set_color(c);
      
      context.canvas().move_to(p0);
      base::paint(context);
    }
    else 
      base::resize_default_baseline(context);
    
    cc.end();
  }
  else if(paint) 
    base::paint(context);
  else 
    base::resize_default_baseline(context);
}

void AbstractStyleBox::resize_default_baseline(Context &context) {
  paint_or_resize_no_baseline(context, false);
}

void AbstractStyleBox::paint(Context &context) {
  paint_or_resize_no_baseline(context, true);
}

void AbstractStyleBox::colorize_scope(SyntaxState &state) {
  if(get_own_style(FontColor).is_valid()) 
    return;
    
  base::colorize_scope(state);
}

VolatileSelection AbstractStyleBox::mouse_selection(Point pos, bool *was_inside_start) {
  if(auto par = parent()) {
    if(get_own_style(Placeholder)) {
      *was_inside_start = 0 <= pos.x && pos.x <= _extents.width;
      return { par, _index, _index + 1 };
    }
    
    if(pos.y < -_extents.ascent || _extents.descent < pos.y) {
      if(pos.x <= _extents.width / 2) {
        *was_inside_start = true;
        return { par, _index, _index };
      }
      else {
        *was_inside_start = false;
        return { par, _index + 1, _index + 1 };
      }
    }
  }
  
  return base::mouse_selection(pos, was_inside_start);
}

//} ... class AbstractStyleBox

//{ class StyleBox ...

StyleBox::StyleBox(MathSequence *content)
  : AbstractStyleBox(content)
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

void TagBox::resize_default_baseline(Context &context) {
  style->set(BaseStyleName, String(tag));
  base::resize_default_baseline(context);
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
