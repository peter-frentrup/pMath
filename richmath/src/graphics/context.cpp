#include <graphics/context.h>

#include <boxes/box.h>
#include <graphics/buffer.h>
#include <gui/control-painter.h>
#include <util/syntax-state.h>

#include <cmath>

#ifdef _WIN32
#  include <cairo-win32.h>
#endif

using namespace richmath;

//{ class Context ...

Context::Context()
  : Base(),
    canvas(0),
    width(HUGE_VAL),
    section_content_window_width(HUGE_VAL),
    sequence_unfilled_width(0),
    cursor_color(0x000000),
    syntax(GeneralSyntaxInfo::std),
    multiplication_sign(0x00D7),
    show_auto_styles(true),
    show_string_characters(true),
    math_spacing(true),
    smaller_fraction_parts(false),
    single_letter_italics(true),
    boxchar_fallback_enabled(true),
    script_indent(0),
    script_size_min(5),
    mouseover_box_id(FrontEndReference::None),
    clicked_box_id(FrontEndReference::None),
    active(true)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  script_size_multis.length(1, 0.71f);
}

void Context::draw_error_rect(
  float x1,
  float y1,
  float x2,
  float y2
) {
  canvas->save();
  {
    int c = canvas->get_color();
    canvas->pixrect(x1, y1, x2, y2, true);
    canvas->set_color(0xFFE6E6);
    canvas->fill_preserve();
    canvas->set_color(0xFF5454);
    canvas->hair_stroke();
    canvas->set_color(c);
  }
  canvas->restore();
}

void Context::draw_selection_path() {
  cairo_path_t *path = cairo_copy_path(canvas->cairo());
  int num_points = 0;
  int end = 0;
  while(end < path->num_data) {
    end += path->data[end].header.length;
    if(path->data[end].header.type != CAIRO_PATH_CLOSE_PATH)
      ++num_points;
  }
  
  if(num_points == 2) {
    int c = 0;
    end = 0;
    while(c < 2 && end < path->num_data) {
      switch(path->data[end].header.type) {
        case CAIRO_PATH_MOVE_TO:
          last_cursor_x[c] = path->data[end + 1].point.x;
          last_cursor_y[c] = path->data[end + 1].point.y;
          ++c;
          break;
          
        case CAIRO_PATH_LINE_TO:
          last_cursor_x[c] = path->data[end + 1].point.x;
          last_cursor_y[c] = path->data[end + 1].point.y;
          ++c;
          break;
          
        case CAIRO_PATH_CURVE_TO: // flat path => no curves
          last_cursor_x[c] = path->data[end + 3].point.x;
          last_cursor_y[c] = path->data[end + 3].point.y;
          ++c;
          break;
          
        case CAIRO_PATH_CLOSE_PATH:
          break;
      }
      
      end += path->data[end].header.length;
    }
  }
  
  cairo_path_destroy(path);
  
  if(num_points <= 2) {
    if(old_selection != selection) {
      canvas->set_color(cursor_color);
      canvas->hair_stroke();
    }
    else
      canvas->new_path();
  }
  else {
    cairo_push_group(canvas->cairo());
    {
      canvas->save();
      {
        cairo_matrix_t idmat;
        cairo_matrix_init_identity(&idmat);
        cairo_set_matrix(canvas->cairo(), &idmat);
        cairo_set_line_width(canvas->cairo(), 2.0);
        canvas->set_color(active ? SelectionBorderColor : InactiveSelectionBorderColor);
        canvas->stroke_preserve();
      }
      canvas->restore();
      
      canvas->set_color(active ? SelectionFillColor : InactiveSelectionFillColor); //ControlPainter::std->selection_color()
      canvas->fill();
    }
    cairo_pop_group_to_source(canvas->cairo());
    canvas->paint_with_alpha(SelectionAlpha);
  }
}

float Context::get_script_size(float oldem) {
  float em;
  
  if(script_indent < 1 || script_size_multis.length() == 0)
    em = oldem;
  else if(script_indent > script_size_multis.length())
    em = script_size_multis[script_size_multis.length() - 1] * oldem;
  else
    em = script_size_multis[script_indent - 1] * oldem;
    
  if(em < script_size_min)
    return oldem; // script_size_min;
    
  return em;
}

void Context::set_script_size_multis(Expr expr) {
  if(expr == PMATH_SYMBOL_AUTOMATIC || expr.is_null()) {
    math_shaper->get_script_size_multis(&script_size_multis);
//    script_size_multis.length(1);
//    script_size_multis[0] = 0.71;
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_LIST) {
    script_size_multis.length(expr.expr_length());
    for(size_t i = 0; i < expr.expr_length(); ++i) {
      script_size_multis[i] = expr[i + 1].to_double();
    }
    return;
  }
  
  script_size_multis.length(1, expr.to_double());
}

void Context::draw_text_shadow(
  Box   *box,
  int    color,
  float  radius,
  float  dx,
  float  dy
) {
  if(canvas->show_only_text)
    return;
    
  float x0, y0;
  canvas->current_pos(&x0, &y0);
  
  if(radius > 0) {
    float x, y, w, h;
    
    x = x0 + dx - radius;
    y = y0 + dy - radius - box->extents().ascent;
    w = box->extents().width    + 2 * radius;
    h = box->extents().height() + 2 * radius;
    
    SharedPtr<Buffer> buf = new Buffer(
      canvas,
      CAIRO_FORMAT_A8,
      x, y, w, h);
      
    if(buf->canvas()) {
      Canvas *old = canvas;
      canvas = buf->canvas();
      
      buf->clear();
      
      canvas->current_pos(&x, &y);
      canvas->move_to(x + dx, y + dy);
      canvas->set_color(0);
      
      canvas->show_only_text = true;
      box->paint(this);
      canvas->show_only_text = false;
      
      buf->blur(radius);
      canvas = old;
      
      canvas->save();
      canvas->set_color(color); // important: set color before mask!
      buf->mask(canvas);
      canvas->fill();
      canvas->restore();
    }
    else
      radius = 0;
  }
  
  if(radius <= 0) {
    canvas->rel_move_to(dx, dy);
    canvas->set_color(color);
    
    canvas->show_only_text = true;
    box->paint(this);
    canvas->show_only_text = false;
  }
  
  canvas->move_to(x0, y0);
}

void Context::draw_with_text_shadows(Box *box, Expr shadows) {
  // shadows = {{dx,dy,color,r},...}   r is optional
  
  if(canvas->show_only_text && shadows == PMATH_SYMBOL_NONE)
    return;
    
  int c = canvas->get_color();
  if(shadows[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 1; i <= shadows.expr_length(); ++i) {
      Expr shadow = shadows[i];
      
      if( shadow[0] == PMATH_SYMBOL_LIST &&
          shadow.expr_length() >= 3 &&
          shadow.expr_length() <= 4)
      {
        int col = pmath_to_color(shadow[3]);
        
        if(col >= 0) {
          draw_text_shadow(
            box,
            col,
            shadow[4].to_double(),
            shadow[1].to_double(),
            shadow[2].to_double());
        }
      }
    }
  }
  
  canvas->set_color(c);
  
  box->paint(this);
}

//} ... class Context

//{ class ContextState ...

void ContextState::begin(SharedPtr<Style> style) {
  old_cursor_color           = ctx->cursor_color;
  old_color                  = ctx->canvas->get_color();
  old_fontsize               = ctx->canvas->get_font_size();
  old_width                  = ctx->width;
  old_math_shaper            = ctx->math_shaper;
  old_text_shaper            = ctx->text_shaper;
  old_math_spacing           = ctx->math_spacing;
  old_show_auto_styles       = ctx->show_auto_styles;
  old_show_string_characters = ctx->show_string_characters;
  have_font_feature_set      = false;
  
  old_antialiasing           = (cairo_antialias_t) - 1;
  old_script_size_multis.length(0);
  
  if(style) {
    int i;
    float f;
    String s;
    Expr expr;
    
    if(ctx->stylesheet->get(style, Antialiasing, &i)) {
      old_antialiasing = cairo_get_antialias(ctx->canvas->cairo());
      switch(i) {
        case 0:
          cairo_set_antialias(
            ctx->canvas->cairo(),
            CAIRO_ANTIALIAS_NONE);
          break;
          
        case 1:
          cairo_set_antialias(
            ctx->canvas->cairo(),
            CAIRO_ANTIALIAS_GRAY);
          break;
          
        case 2:
          cairo_set_antialias(
            ctx->canvas->cairo(),
            CAIRO_ANTIALIAS_DEFAULT);
          break;
      }
    }
    
    if(ctx->stylesheet->get(style, AutoSpacing, &i)) {
      ctx->math_spacing = i;
      //show_auto_styles = i;
    }
    
    FontStyle fs = ctx->text_shaper->get_style();
    if(ctx->stylesheet->get(style, FontSlant, &i)) {
      if(i == FontSlantItalic)
        fs += Italic;
      else
        fs -= Italic;
    }
    
    if(ctx->stylesheet->get(style, FontWeight, &i)) {
      if(i == FontWeightBold)
        fs += Bold;
      else
        fs -= Bold;
    }
    
    if(ctx->stylesheet->get(style, MathFontFamily, &expr)) {
      if(expr.is_string()) {
        s = String(expr);
        
        if(auto math_shaper = MathShaper::available_shapers.search(s))
          ctx->math_shaper = *math_shaper;
      }
      else if(expr[0] == PMATH_SYMBOL_LIST) {
        for(const auto &item : expr.items()) {
          s = String(item);
          
          if(auto math_shaper = MathShaper::available_shapers.search(s)) {
            ctx->math_shaper = *math_shaper;
            break;
          }
        }
      }
    }
    
    ctx->math_shaper = ctx->math_shaper->math_set_style(fs);
    
    if(ctx->stylesheet->get(style, FontFamilies, &expr)) {
      if(expr.is_string()) {
        s = String(expr);
        
        if(FontInfo::font_exists(s)) {
          FallbackTextShaper *fts = new FallbackTextShaper(TextShaper::find(s, fs));
          fts->add(ctx->math_shaper);
          ctx->text_shaper = fts;
        }
      }
      else if(expr[0] == PMATH_SYMBOL_LIST) {
        if(expr.expr_length() == 0) {
          ctx->text_shaper = ctx->math_shaper;
        }
        else {
          FallbackTextShaper *fts = 0;
          
          int max_fallbacks = FontsPerGlyphCount / 2;
          
          for(size_t i = 1; i <= expr.expr_length(); ++i) {
            s = String(expr[i]);
            
            if(FontInfo::font_exists(s)) {
              if(--max_fallbacks == 0)
                break;
                
              if(fts)
                fts->add(TextShaper::find(s, fs));
              else
                fts = new FallbackTextShaper(TextShaper::find(s, fs));
            }
          }
          
          if(fts) {
            fts->add(ctx->math_shaper);
            ctx->text_shaper = fts;
          }
        }
      }
    }
    else
      ctx->text_shaper = ctx->text_shaper->set_style(fs);
      
    if(ctx->stylesheet->get(style, FontFeatures, &expr)) {
      have_font_feature_set = true;
      old_font_feature_set.clear();
      old_font_feature_set.add(ctx->fontfeatures);
      
      ctx->fontfeatures.add(expr);
    }
    
    if(ctx->stylesheet->get(style, FontSize, &f)) {
      ctx->canvas->set_font_size(f);
    }
    
    if(ctx->stylesheet->get(style, LineBreakWithin, &i) && !i) {
      ctx->width = Infinity;
    }
    
    if(ctx->stylesheet->get(style, ScriptSizeMultipliers, &expr)) {
      old_script_size_multis.swap(ctx->script_size_multis);
      ctx->set_script_size_multis(expr);
    }
    
    if(ctx->stylesheet->get(style, ShowAutoStyles, &i)) {
      ctx->show_auto_styles = i;
      //show_auto_styles = i;
    }
    
    if(ctx->stylesheet->get(style, ShowStringCharacters, &i)) {
      ctx->show_string_characters = i;
    }
  }
}

void ContextState::end() {
  ctx->cursor_color = old_cursor_color;
  ctx->canvas->set_color(       old_color);
  ctx->canvas->set_font_size(   old_fontsize);
  ctx->width                  = old_width;
  ctx->show_string_characters = old_show_string_characters;
  ctx->show_auto_styles       = old_show_auto_styles;
  ctx->math_spacing           = old_math_spacing;
  ctx->math_shaper            = old_math_shaper;
  ctx->text_shaper            = old_text_shaper;
  
  if(old_antialiasing >= 0)
    cairo_set_antialias(ctx->canvas->cairo(), old_antialiasing);
    
  if(old_script_size_multis.length() > 0)
    old_script_size_multis.swap(ctx->script_size_multis);
    
  if(have_font_feature_set) {
    ctx->fontfeatures.clear();
    ctx->fontfeatures.add(old_font_feature_set);
  }
}

//} ... class ContextState
