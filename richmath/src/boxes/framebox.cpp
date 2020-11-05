#include <boxes/framebox.h>

#include <boxes/graphics/graphicsdirective.h>
#include <boxes/mathsequence.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_FrameBox;

//{ class FrameBox ...

FrameBox::FrameBox(MathSequence *content)
  : base(content)
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
  
  finish_load_from_object(std::move(expr));
  return true;
}

void FrameBox::resize_default_baseline(Context &context) {
  em = context.canvas().get_font_size();
  
  float border_and_padding = 0.25f * em;
  
  float old_width = context.width;
  context.width -= 2 * border_and_padding;
  
  base::resize_default_baseline(context);
  
  cx = border_and_padding;
  
  _extents.width +=   2 * border_and_padding;
  _extents.ascent +=  cx;
  _extents.descent += cx;
  
  if(_extents.ascent < em)
    _extents.ascent = em;
    
  if(_extents.descent < em * 0.4f)
    _extents.descent = em * 0.4f;
    
  context.width = old_width;
}

void FrameBox::paint(Context &context) {
  update_dynamic_styles(context);
  
  Point p0 = context.canvas().current_pos();
  
  context.canvas().save();
  {
    ContextState cs(context);
    cs.begin(style);
    {
      bool sot = context.canvas().show_only_text;
      context.canvas().show_only_text = false;
          
      RectangleF rect = _extents.to_rectangle(p0);
      BoxRadius radii;
      
      if(Expr expr = get_own_style(BorderRadius)) 
        radii = BoxRadius(expr);
      
      if(Expr expr = get_own_style(FrameStyle)) {
        GraphicsDirective::apply(expr, context);
      }
      float half_border_thickness = 0.05f * context.canvas().get_font_size();
    
      rect.normalize();
      rect.grow(-half_border_thickness);
      radii += BoxRadius(-half_border_thickness);
      
      float thickness_pixels = context.canvas().user_to_device_dist(Vector2F(2 * half_border_thickness, 0)).length();
      if(thickness_pixels > 0.5) {
        thickness_pixels = round(thickness_pixels);
        bool is_odd = (fmodf(thickness_pixels, 2.0f) == 1.0f);
        rect.pixel_align(context.canvas(), is_odd, +1);
      }
      
      radii.normalize(rect.width, rect.height);
      rect.add_round_rect_path(context.canvas(), radii);
      
      cairo_set_line_join(context.canvas().cairo(), CAIRO_LINE_JOIN_MITER);
      context.canvas().reset_matrix();
      
      cairo_set_line_width(context.canvas().cairo(), thickness_pixels);
      
      if(Color bg = get_own_style(Background)) {
        context.canvas().save();
        context.canvas().set_color(bg);
        context.canvas().fill_preserve();
        context.canvas().restore();
      }
      
      context.canvas().stroke();
      
      context.canvas().show_only_text = sot;
    }
    cs.end();
  }
  context.canvas().restore();
  
  context.canvas().move_to(p0);
  
  base::paint(context);
}

void FrameBox::reset_style() {
  Style::reset(style, "Framed");
}

Expr FrameBox::to_pmath_symbol() {
  return Symbol(richmath_System_FrameBox);
}

Expr FrameBox::to_pmath(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::Parseable) && get_own_style(StripOnInput, false)) {
    return _content->to_pmath(flags);
  }
  
  Gather g;
  
  Gather::emit(_content->to_pmath(flags));
  
  if(style) {
    bool with_inherited = true;
    
    String s;
    if(style->get(BaseStyleName, &s) && s.equals("Framed"))
      with_inherited = false;
    
    style->emit_to_pmath(with_inherited);
  }
  
  Expr e = g.end();
  e.set(0, Symbol(richmath_System_FrameBox));
  return e;
}

//} ... class FrameBox
