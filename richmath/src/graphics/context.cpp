#include <graphics/context.h>

#include <boxes/box.h>
#include <graphics/buffer.h>
#include <gui/control-painter.h>
#include <syntax/syntax-state.h>

#include <cmath>

#ifdef _WIN32
#  include <cairo-win32.h>
#endif

using namespace richmath;

namespace richmath {
  class ContextState::Impl {
    public:
      Impl(ContextState &self) : self{self} {}
      
      void apply_syntax(SharedPtr<Style> style);
    
    private:
      void ensure_new_syntax(SharedPtr<GeneralSyntaxInfo> &new_syntax) const;
      void set_syntax_color(SharedPtr<Style> style, SharedPtr<GeneralSyntaxInfo> &new_syntax, int glyph_style_index, ColorStyleOptionName color_style_name) const;
      
    private:
      ContextState &self;
  };
}

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_Inherited;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_MathFontFamily;
extern pmath_symbol_t richmath_System_None;

//{ class Context ...

Context::Context()
  : Base(),
    canvas_ptr(nullptr),
    width(HUGE_VAL),
    section_content_window_width(HUGE_VAL),
    sequence_unfilled_width(0),
    cursor_color(Color::Black),
    syntax(GeneralSyntaxInfo::std),
    multiplication_sign(0x00D7),
    script_level(0),
    script_size_min(5),
    mouseover_box_id(FrontEndReference::None),
    clicked_box_id(FrontEndReference::None),
    show_auto_styles(true),
    show_string_characters(true),
    math_spacing(true),
    single_letter_italics(true),
    boxchar_fallback_enabled(true),
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
  canvas().save();
  {
    Color c = canvas().get_color();
    canvas().pixrect(x1, y1, x2, y2, true);
    canvas().set_color(Color::from_rgb24(0xFFE6E6));
    canvas().fill_preserve();
    canvas().set_color(Color::from_rgb24(0xFF5454));
    canvas().hair_stroke();
    canvas().set_color(c);
  }
  canvas().restore();
}

void Context::draw_selection_path() {
  cairo_path_t *path = cairo_copy_path(canvas().cairo());
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
          last_cursor_pos[c] = Point(path->data[end + 1].point.x, path->data[end + 1].point.y);
          ++c;
          break;
          
        case CAIRO_PATH_LINE_TO:
          last_cursor_pos[c] = Point(path->data[end + 1].point.x, path->data[end + 1].point.y);
          ++c;
          break;
          
        case CAIRO_PATH_CURVE_TO: // flat path => no curves
          last_cursor_pos[c] = Point(path->data[end + 3].point.x, path->data[end + 3].point.y);
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
      canvas().set_color(cursor_color);
      canvas().hair_stroke();
    }
    else
      canvas().new_path();
  }
  else {
    canvas().save();
    {
      canvas().set_color(active ? SelectionColor : InactiveSelectionColor, SelectionFillAlpha); //ControlPainter::std->selection_color()
      canvas().fill_preserve();
      
      canvas().reset_matrix();
      RectangleF clip_rect = canvas().clip_extents();
      clip_rect.grow(1.0f);
      
      clip_rect.add_rect_path(canvas(), true);
      canvas().clip_preserve();
      cairo_set_line_width(canvas().cairo(), 2.0);
      canvas().set_color(active ? SelectionColor : InactiveSelectionColor);
      canvas().stroke();
    }
    canvas().restore();
  }
}

float Context::get_script_size(float oldem) {
  float em;
  
  if(script_level < 2 || script_size_multis.length() == 0)
    em = oldem;
  else if(script_level - 2 < script_size_multis.length())
    em = script_size_multis[script_level - 2] * oldem;
  else
    em = script_size_multis[script_size_multis.length() - 1] * oldem;
    
  if(em < script_size_min)
    return oldem; // script_size_min;
    
  return em;
}

void Context::set_script_size_multis(Expr expr) {
  if(expr == richmath_System_Automatic || expr.is_null()) {
    math_shaper->get_script_size_multis(&script_size_multis);
//    script_size_multis.length(1);
//    script_size_multis[0] = 0.71;
    return;
  }
  
  if(expr[0] == richmath_System_List) {
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
  Color  color,
  float  radius,
  float  dx,
  float  dy
) {
  if(!color.is_valid())
    return;
  
  if(canvas().show_only_text)
    return;
    
  Point p0 = canvas().current_pos();
  
  if(radius > 0) {
    RectangleF rect {
      p0.x + dx - radius,
      p0.y + dy - radius - box->extents().ascent,
      box->extents().width    + 2 * radius,
      box->extents().height() + 2 * radius};
    
    SharedPtr<Buffer> buf = new Buffer(
      canvas(),
      CAIRO_FORMAT_A8,
      rect);
      
    if(buf->canvas()) {
      with_canvas(*buf->canvas(), [&]() {
        buf->clear();
        
        canvas().rel_move_to(dx, dy);
        canvas().set_color(Color::Black);
        
        canvas().show_only_text = true;
        box->paint(*this);
        canvas().show_only_text = false;
      });
      buf->blur(radius);
      
      canvas().save();
      canvas().set_color(color); // important: set color before mask!
      buf->mask(canvas());
      canvas().fill();
      canvas().restore();
    }
    else
      radius = 0;
  }
  
  if(radius <= 0) {
    canvas().rel_move_to(dx, dy);
    canvas().set_color(color);
    
    canvas().show_only_text = true;
    box->paint(*this);
    canvas().show_only_text = false;
  }
  
  canvas().move_to(p0);
}

void Context::draw_with_text_shadows(Box *box, Expr shadows) {
  // shadows = {{dx,dy,color,r},...}   r is optional
  
  if(canvas().show_only_text && shadows == richmath_System_None)
    return;
    
  Color c = canvas().get_color();
  if(shadows[0] == richmath_System_List) {
    for(size_t i = 1; i <= shadows.expr_length(); ++i) {
      Expr shadow = shadows[i];
      
      if( shadow[0] == richmath_System_List &&
          shadow.expr_length() >= 3 &&
          shadow.expr_length() <= 4)
      {
        if(Color col = Color::from_pmath(shadow[3])) {
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
  
  canvas().set_color(c);
  
  box->paint(*this);
}

//} ... class Context

//{ class ContextState ...

void ContextState::begin(SharedPtr<Style> style) {
  old_cursor_color           = ctx.cursor_color;
  old_color                  = ctx.canvas().get_color();
  old_fontsize               = ctx.canvas().get_font_size();
  old_width                  = ctx.width;
  old_math_shaper            = ctx.math_shaper;
  old_text_shaper            = ctx.text_shaper;
  old_script_level           = ctx.script_level;
  old_syntax                 = ctx.syntax;
  old_math_spacing           = ctx.math_spacing;
  old_show_auto_styles       = ctx.show_auto_styles;
  old_show_string_characters = ctx.show_string_characters;
  have_font_feature_set      = false;
  
  old_antialiasing           = (cairo_antialias_t) - 1;
  old_script_size_multis.length(0);
  
  apply_layout_styles(style);
  apply_non_layout_styles(style);
}

void ContextState::apply_layout_styles(SharedPtr<Style> style) {
  if(!style)
    return;
  
  int i;
  float f;
  String s;
  Expr expr;
  
  if(ctx.stylesheet->get(style, AutoSpacing, &i)) {
    ctx.math_spacing = i;
  }
  
  FontStyle fs = ctx.text_shaper->get_style();
  if(ctx.stylesheet->get(style, FontSlant, &i)) {
    if(i == FontSlantItalic)
      fs += Italic;
    else
      fs -= Italic;
  }
  
  if(ctx.stylesheet->get(style, FontWeight, &i)) {
    if(i == FontWeightBold)
      fs += Bold;
    else
      fs -= Bold;
  }
  
  if(ctx.stylesheet->get(style, MathFontFamily, &expr)) {
    if(expr.is_string()) {
      s = String(expr);
      
      if(auto math_shaper = MathShaper::available_shapers.search(s))
        ctx.math_shaper = *math_shaper;
    }
    else if(expr[0] == richmath_System_List) {
      for(const auto &item : expr.items()) {
        s = String(item);
        
        if(auto math_shaper = MathShaper::available_shapers.search(s)) {
          ctx.math_shaper = *math_shaper;
          break;
        }
      }
    }
  }
  
  ctx.math_shaper = ctx.math_shaper->math_set_style(fs);
  
  if(ctx.stylesheet->get(style, FontFamilies, &expr)) {
    if(expr.is_string()) {
      s = String(expr);
      
      if(FontInfo::font_exists_similar(s)) {
        FallbackTextShaper *fts = new FallbackTextShaper(TextShaper::find(s, fs));
        fts->add(ctx.math_shaper);
        ctx.text_shaper = fts;
      }
    }
    else if(expr == richmath_System_MathFontFamily) {
      ctx.text_shaper = ctx.math_shaper;
    }
    else if(expr[0] == richmath_System_List) {
      if(expr.expr_length() == 0) {
        ctx.text_shaper = ctx.math_shaper;
      }
      else {
        SharedPtr<FallbackTextShaper> fts = nullptr;
        
        int max_fallbacks = FontsPerGlyphCount / 2;
        
        for(auto item : expr.items()) {
          if(String s = item) {
            if(FontInfo::font_exists_similar(s)) {
              if(--max_fallbacks == 0)
                break;
                
              FallbackTextShaper::add_or_create(fts, TextShaper::find(s, fs));
            }
          }
          else if(item == richmath_System_MathFontFamily) {
            FallbackTextShaper::add_or_create(fts, ctx.math_shaper);
          }
          else if(item == richmath_System_Inherited) {
            FallbackTextShaper::add_or_create(fts, ctx.text_shaper);
          }
        }
        
        if(fts) {
          fts->add(ctx.math_shaper);
          ctx.text_shaper = fts;
        }
      }
    }
  }
  else
    ctx.text_shaper = ctx.text_shaper->set_style(fs);
    
  if(ctx.stylesheet->get(style, FontFeatures, &expr)) {
    have_font_feature_set = true;
    old_font_feature_set.clear();
    old_font_feature_set.add(ctx.fontfeatures);
    
    ctx.fontfeatures.add(expr);
  }
  
  if(ctx.stylesheet->get(style, FontSize, &f)) {
    ctx.canvas().set_font_size(f);
  }
  
  if(ctx.stylesheet->get(style, LineBreakWithin, &i) && !i) {
    ctx.width = Infinity;
  }
  
  if(ctx.stylesheet->get(style, ScriptLevel, &i)) {
    ctx.script_level = i;
  }
  
  if(ctx.stylesheet->get(style, ScriptSizeMultipliers, &expr)) {
    using std::swap;
    swap(old_script_size_multis, ctx.script_size_multis);
    ctx.set_script_size_multis(expr);
  }
  
  if(ctx.stylesheet->get(style, ShowAutoStyles, &i)) {
    ctx.show_auto_styles = i;
  }
  
  if(ctx.stylesheet->get(style, ShowStringCharacters, &i)) {
    ctx.show_string_characters = i;
  }
}

void ContextState::apply_non_layout_styles(SharedPtr<Style> style) {
  if(!style)
    return;
  
  int i;
  if(ctx.stylesheet->get(style, Antialiasing, &i)) {
    old_antialiasing = cairo_get_antialias(ctx.canvas().cairo());
    switch(i) {
      case AutoBoolFalse:
        cairo_set_antialias(
          ctx.canvas().cairo(),
          CAIRO_ANTIALIAS_NONE);
        break;
        
      case AutoBoolTrue:
        cairo_set_antialias(
          ctx.canvas().cairo(),
          CAIRO_ANTIALIAS_GRAY);
        break;
        
      case AutoBoolAutomatic:
        cairo_set_antialias(
          ctx.canvas().cairo(),
          CAIRO_ANTIALIAS_DEFAULT);
        break;
    }
  }
  
  Impl(*this).apply_syntax(style);
}

void ContextState::end() {
  ctx.cursor_color = old_cursor_color;
  ctx.canvas().set_color(      old_color);
  ctx.canvas().set_font_size(  old_fontsize);
  ctx.width                  = old_width;
  ctx.script_level           = old_script_level;
  ctx.show_string_characters = old_show_string_characters;
  ctx.show_auto_styles       = old_show_auto_styles;
  ctx.math_spacing           = old_math_spacing;
  ctx.math_shaper            = old_math_shaper;
  ctx.text_shaper            = old_text_shaper;
  ctx.syntax                 = old_syntax;
  
  if(old_antialiasing >= 0)
    cairo_set_antialias(ctx.canvas().cairo(), old_antialiasing);
    
  if(old_script_size_multis.length() > 0) 
    swap(old_script_size_multis, ctx.script_size_multis);
    
  if(have_font_feature_set) {
    ctx.fontfeatures.clear();
    ctx.fontfeatures.add(old_font_feature_set);
  }
}

//} ... class ContextState

//{ class ContextState::Impl ...

void ContextState::Impl::apply_syntax(SharedPtr<Style> style) {
  SharedPtr<GeneralSyntaxInfo> new_syntax = nullptr;
  
  set_syntax_color(style, new_syntax, GlyphStyleSpecialStringPart,  CharacterNameSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleComment,            CommentSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleExcessOrMissingArg, ExcessOrMissingArgumentSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleSpecialUse,         FunctionLocalVariableSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleFunctionCall,       FunctionNameSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleImplicit,           ImplicitOperatorSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleKeyword,            KeywordSymbolSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleScopeError,         LocalScopeConflictSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleLocal,              LocalVariableSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleParameter,          PatternVariableSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleString,             StringSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleShadowError,        SymbolShadowingSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleSyntaxError,        SyntaxErrorColor);
  set_syntax_color(style, new_syntax, GlyphStyleNewSymbol,          UndefinedSymbolSyntaxColor);
  set_syntax_color(style, new_syntax, GlyphStyleInvalidOption,      UnknownOptionSyntaxColor);
  
  if(new_syntax)
    self.ctx.syntax = new_syntax;
}

void ContextState::Impl::ensure_new_syntax(SharedPtr<GeneralSyntaxInfo> &new_syntax) const {
  if(new_syntax)
    return;
  
  new_syntax = new GeneralSyntaxInfo();
  if(self.old_syntax) {
    memcpy(new_syntax->glyph_style_colors, self.old_syntax->glyph_style_colors, sizeof(new_syntax->glyph_style_colors));
  }
}

void ContextState::Impl::set_syntax_color(SharedPtr<Style> style, SharedPtr<GeneralSyntaxInfo> &new_syntax, int glyph_style_index, ColorStyleOptionName color_style_name) const {
  Color c;
  if(self.ctx.stylesheet->get(style, color_style_name, &c)) {
    ensure_new_syntax(new_syntax);
    new_syntax->glyph_style_colors[glyph_style_index] = c;
  }
}

//} ... class ContextState::Impl
