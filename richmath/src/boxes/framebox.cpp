#include <boxes/framebox.h>

#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_FrameBox;

//{ class FrameBox ...

FrameBox::FrameBox(MathSequence *content)
  : OwnerBox(content)
{
}

bool FrameBox::try_load_from_object(Expr expr, BoxInputFlags options) {
  if(expr[0] != richmath_System_FrameBox)
    return false;
    
  if(expr.expr_length() < 1)
    return false;
    
  Expr options_expr(pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY));
  if(options_expr.is_null())
    return false;
    
  /* now success is guaranteed */
  
  reset_style();
  if(!style)
    style = new Style(options_expr);
  else
    style->add_pmath(options_expr);
    
  _content->load_from_object(expr[1], options);
  
  return true;
}

void FrameBox::resize(Context *context) {
  em = context->canvas->get_font_size();
  
  float old_width = context->width;
  context->width -= em * 0.5f;
  
  OwnerBox::resize(context);
  
  cx = em * 0.25f;
  
  _extents.width +=   2 * cx;
  _extents.ascent +=  cx;
  _extents.descent += cx;
  
  if(_extents.ascent < em)
    _extents.ascent = em;
    
  if(_extents.descent < em * 0.4f)
    _extents.descent = em * 0.4f;
    
  context->width = old_width;
}

void FrameBox::paint(Context *context) {
  update_dynamic_styles(context);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  Rectangle rect(x, y - _extents.ascent, _extents.width, _extents.height());
  BoxRadius radii;
  
  Expr expr;
  if(context->stylesheet->get(style, BorderRadius, &expr)) {
    radii = BoxRadius(expr);
  }
  
  rect.normalize();
  rect.pixel_align(*context->canvas, false, +1);
  
  radii.normalize(rect.width, rect.height);
  rect.add_round_rect_path(*context->canvas, radii, false);
  
  Point delta(-0.1f * em, -0.1f * em);
  delta.pixel_align_distance(*context->canvas);
  
  rect.grow(delta);
  rect.normalize_to_zero();
  
  radii += BoxRadius(delta.x, delta.y);
  radii.normalize(rect.width, rect.height);
  
  rect.add_round_rect_path(*context->canvas, radii, true);
  
  bool sot = context->canvas->show_only_text;
  context->canvas->show_only_text = false;
  context->canvas->fill();
  context->canvas->show_only_text = sot;
  
  context->canvas->move_to(x, y);
  OwnerBox::paint(context);
  //paint_content(context);
}

Expr FrameBox::to_pmath_symbol() {
  return Symbol(richmath_System_FrameBox);
}

Expr FrameBox::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  Gather::emit(_content->to_pmath(flags));
  
  if(style)
    style->emit_to_pmath();
    
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_FrameBox));
  return e;
}

//} ... class FrameBox
