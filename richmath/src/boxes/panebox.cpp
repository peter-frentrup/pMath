#include <boxes/panebox.h>
#include <boxes/mathsequence.h>

#include <graphics/context.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_PaneBox;

//{ class PaneBox ...

PaneBox::PaneBox(MathSequence *content) : base(content) {
}

PaneBox::~PaneBox() {
}

bool PaneBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(expr[0] != richmath_System_PaneBox)
    return false;
  
  if(expr.expr_length() < 1)
    return false;
    
  Expr options(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  
  if(options.is_null())
    return false;
  
  /* now success is guaranteed */
  
  content()->load_from_object(expr[1], opts);
  
  reset_style();
  if(options != PMATH_UNDEFINED) 
    style->add_pmath(options);
  
  finish_load_from_object(std::move(expr));
  return true;
}

void PaneBox::reset_style() {
  Style::reset(style, "Pane");
}

Expr PaneBox::to_pmath_symbol() { 
  return Symbol(richmath_System_PaneBox); 
}

Expr PaneBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  g.emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("Pane"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_PaneBox));
  return e;
}

void PaneBox::resize_default_baseline(Context &context) {
  float w = get_own_style(ImageSizeHorizontal, ImageSizeAutomatic);
  float h = get_own_style(ImageSizeVertical,   ImageSizeAutomatic);
  
  auto old_width = context.width;
  if(get_own_style(LineBreakWithin, true)) {
    if(w > 0) {
      context.width = w;
    }
  }
  else {
    context.width = Infinity;
  }
  
  _content->resize(context);
  
  if(w < 0)
    w = _content->extents().width;
    
  if(h < 0)
    h = _content->extents().height();
  
  _extents.width = w;
  _extents.ascent = h;
  _extents.descent = 0;
  
  cy = _content->extents().ascent - _extents.ascent;
  
  context.width = old_width;
  
}

void PaneBox::paint_content(Context &context) {
  Point pos = context.canvas().current_pos();
  
  context.canvas().save();
  {
    context.canvas().pixrect(_extents.to_rectangle(pos), false);
    context.canvas().clip();
    
    context.canvas().move_to(pos);
    base::paint_content(context);
  }
  context.canvas().restore();
}

//} ... class PaneBox
