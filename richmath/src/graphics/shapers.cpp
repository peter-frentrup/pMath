#include <graphics/shapers.h>

#include <cmath>

#include <boxes/mathsequence.h>

#include <graphics/context.h>

#ifdef CAIRO_HAS_WIN32_FONT
  #include <graphics/win32-shaper.h>
#else
  #error no support for font backend
#endif

static float divide(float n, float d, float fail = 0){
  return d == 0 ? fail : n/d;
}

using namespace richmath;

class FontKey{
  public:
    FontKey(
      const String &name, 
      FontStyle     style)
    : _name(name),
      _style(style)
    {
    }
    
    bool operator==(const FontKey &other) const {
      return _name  == other._name 
          && _style == other._style;
    }
    
    bool operator!=(const FontKey &other) const {
      return !(*this == other);
    }
    
    unsigned int hash() const {
      return _name.hash() ^ (unsigned int)_style;
    }
    
  private:
    String    _name;
    FontStyle _style;
};

static Hashtable<
  FontKey,
  SharedPtr<TextShaper>
> shapers;

static Hashtable<uint32_t, uint32_t, cast_hash> accent_chars;

//{ class TextShaper ...

void TextShaper::vertical_glyph_size(
  Context         *context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
){
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = 0;
  cg.y = 0;
  
  context->canvas->set_font_face(font(info.fontinfo));
  cg.index = info.index;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  
  if(*ascent < -cte.y_bearing)
     *ascent = -cte.y_bearing;
  if(*descent < cte.height + cte.y_bearing)
     *descent = cte.height + cte.y_bearing;
}

void TextShaper::show_glyph(
  Context         *context, 
  float            x,
  float            y,
  const uint16_t   ch,
  const GlyphInfo &info
){
  bool workaround = false;
  
  cairo_surface_t *target = context->canvas->target();
  switch(cairo_surface_get_type(target)){
    case CAIRO_SURFACE_TYPE_IMAGE:
    case CAIRO_SURFACE_TYPE_WIN32:
      workaround = (cairo_image_surface_get_format(target) == CAIRO_FORMAT_ARGB32);
      break;
    
    default:
      break;
  }
  
  if(workaround){
  /* Workaround a Cairo (1.8.8) Bug:
      Platform: Windows, Cleartype on, ARGB32 image or HDC
      The last (cleartype-blured) pixel column of the last glyph and the zero-th 
      column (also cleartype-blured) of the first pixel in a glyph-string wont 
      be drawn. 
      That looks ugly, so we add invisible glyphs at the first and the last
      index with adjusted x-positions.
      
      To see the difference, draw something to the glass area of the window (an 
      ARGB32-image surface is used there) with and without this workaround.
   */
    static GlyphInfo space_glyph;
    static TextShaper *last_space_shaper = 0;
    
    if(last_space_shaper != this){
      static const uint16_t space_char = ' ';
      
      last_space_shaper = this;
      
      decode_token(context, 1, &space_char, &space_glyph);
    }
    
    cairo_glyph_t cg[3];
    cg[0].index = space_glyph.index; // invisible
    cg[0].x = x - 3.0;
    cg[0].y = y;
    cg[1].index = info.index;
    cg[1].x = x + info.x_offset;
    cg[1].y = y;
    cg[2].index = space_glyph.index; // invisible
    cg[2].x = x + info.right + 3.0;
    cg[2].y = y;
    
    context->canvas->set_font_face(font(info.fontinfo));
    context->canvas->show_glyphs(cg, 3);
  }
  else{
    cairo_glyph_t cg;
    cg.index = info.index;
    cg.x = x + info.x_offset;
    cg.y = y;
    
    context->canvas->set_font_face(font(info.fontinfo));
    context->canvas->show_glyphs(&cg, 1);
  }
}

SharedPtr<TextShaper> TextShaper::find(
  const String  &name,
  FontStyle      style
){
  FontKey key(name, style);
  SharedPtr<TextShaper> *result = shapers.search(key);
  SharedPtr<TextShaper> fd;
  
  if(result)
    return *result;
  
  fd =
    #ifdef CAIRO_HAS_WIN32_FONT
      new WindowsFontShaper(name, style);
    #else
      no support for font backend
    #endif
    
  shapers.set(key, fd);
  return fd;
}

uint32_t TextShaper::get_accent_char(uint32_t input_char){
  if(accent_chars.size() == 0){
    accent_chars.set('`',    0x0300);
    accent_chars.set('\'',   0x0301);
    accent_chars.set('^',    0x0302);
    accent_chars.set('~',    0x0303);
    accent_chars.set('.',    0x0307);
    accent_chars.set('"',    0x0308);
    accent_chars.set(0x00B0, 0x030A); // degree/ring
  }
  
  return accent_chars[input_char];
}

void TextShaper::clear_cache(){
  shapers.clear();
  accent_chars.clear();
}

//} ... class TextShaper

//{ class MathShaper ...

Hashtable<String, SharedPtr<MathShaper> > MathShaper::available_shapers;

SharedPtr<MathShaper> MathShaper::math_set_style(FontStyle style){
  SharedPtr<TextShaper> ts = set_style(style);
  SharedPtr<MathShaper> ms(static_cast<MathShaper*>(ts.release()));
  
  return ms;
}

//} ... class MathShaper

//{ class SimpleMathShaper ...

SimpleMathShaper::SimpleMathShaper(int radical_font)
: MathShaper(),
  _radical_font(radical_font)
{
}

SimpleMathShaper::~SimpleMathShaper(){
}

void SimpleMathShaper::vertical_glyph_size(
  Context         *context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
){
  if(info.composed){
    if(info.horizontal_stretch){
      uint16_t left, middle, right, special_center;
      h_stretch_big_glyphs(
        ch,
        &left,
        &middle,
        &right,
        &special_center);
      
      if(left || middle || right || special_center){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        
        if(left){
          cg.index = left;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
             *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
             *descent = cte.height + cte.y_bearing;
        }
        
        if(middle){
          cg.index = middle;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
             *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
             *descent = cte.height + cte.y_bearing;
        }
        
        if(right){
          cg.index = right;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
             *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
             *descent = cte.height + cte.y_bearing;
        }
        
        if(special_center){
          cg.index = special_center;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
             *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
             *descent = cte.height + cte.y_bearing;
        }
      }
    }
    else if(info.index == 0xFFFF){
      uint16_t upper, lower;
      v_stretch_pair_glyphs(
        ch,
        &upper,
        &lower);
      
      if(upper && lower){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
      
        float d = context->canvas->get_font_size() * 0.25;
        float h = 0;
        
        context->canvas->set_font_face(font(info.fontinfo));
        
        cg.index = upper;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        h+= cte.height;
        
        cg.index = lower;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        h+= cte.height;
        
        h/= 2;
        if(*ascent < h + d)
           *ascent = h + d;
        if(*descent < h - d)
           *descent = h - d;
      }
    }
    else{
      uint16_t top, middle, bottom, special_center;
      v_stretch_big_glyphs(
        ch,
        &top,
        &middle,
        &bottom,
        &special_center);
      
      if(top && bottom){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
      
        float d = context->canvas->get_font_size() * 0.25;
        float h = 0;
        
        context->canvas->set_font_face(font(info.fontinfo));
        
        cg.index = top;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        h+= cte.height;
        
        cg.index = bottom;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        h+= cte.height;
        
        if(special_center){
          cg.index = special_center;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          
          h+= cte.height;
        }
        
        if(middle){
          cg.index = middle;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          
          if(special_center){
            h+= info.index * cte.height * 2;
          }
          else
            h+= info.index * cte.height;
        }
        
        h/= 2;
        if(*ascent < h + d)
           *ascent = h + d;
        if(*descent < h - d)
           *descent = h - d;
      }
    }
  }
  else
    TextShaper::vertical_glyph_size(context, ch, info, ascent, descent);
}

void SimpleMathShaper::show_glyph(
  Context         *context, 
  float            x,
  float            y,
  const uint16_t   ch,
  const GlyphInfo &info
){
  if(info.composed){
    if(get_style().italic){
      math_set_style(get_style() - Italic)->show_glyph(
        context, x, y, ch, info);
      
      return;
    }
      
    if(info.horizontal_stretch){
      uint16_t left, middle, right, special_center;
      h_stretch_big_glyphs(
        ch,
        &left,
        &middle,
        &right,
        &special_center);
      
      if(left || middle || right || special_center){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = x + info.x_offset;
        cg.y = y;
        
        context->canvas->set_font_face(font(info.fontinfo));
        
        if(left){
          cg.index = left;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          context->canvas->show_glyphs(&cg, 1);
          cg.x+= cte.x_advance;
        }
        
        if(middle && info.index > 0){
          cg.index = middle;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          for(int i = 0;i < info.index;++i){
            context->canvas->show_glyphs(&cg, 1);
            cg.x+= cte.x_advance;
          }
        }
        
        if(special_center){
          cg.index = special_center;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          context->canvas->show_glyphs(&cg, 1);
          cg.x+= cte.x_advance;
          
          if(middle && info.index > 0){
            cg.index = middle;
            cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
            for(int i = 0;i < info.index;++i){
              context->canvas->show_glyphs(&cg, 1);
              cg.x+= cte.x_advance;
            }
          }
        }
        
        if(right){
          cg.index = right;
          context->canvas->show_glyphs(&cg, 1);
        }
      }
    }
    else if(info.index == 0xFFFF){
      uint16_t upper, lower;
      v_stretch_pair_glyphs(
        ch,
        &upper,
        &lower);
      
      if(upper && lower){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = x + info.x_offset;
        cg.y = y - context->canvas->get_font_size() * 0.25;
        
        context->canvas->set_font_face(font(info.fontinfo));
        
        cg.index = upper;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        float th = cte.height;
        float ta = -cte.y_bearing;
        cg.y-= th - ta;
        
        context->canvas->show_glyphs(&cg, 1);
        
        cg.y+= th - ta;
        
        cg.index = lower;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        cg.y+= -cte.y_bearing;
        context->canvas->show_glyphs(&cg, 1);
      }
    }
    else{
      uint16_t top, middle, bottom, special_center;
      v_stretch_big_glyphs(
        ch,
        &top,
        &middle,
        &bottom,
        &special_center);
      
      if(top && bottom){
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = x + info.x_offset;
        cg.y = y - context->canvas->get_font_size() * 0.25;
        
        context->canvas->set_font_face(font(info.fontinfo));
        
        cg.index = top;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        float th = cte.height;
        float ta = -cte.y_bearing;
        
        float mh = 0;
        float ma = 0;
        if(middle && info.index > 0){
          cg.index = middle;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          mh = cte.height;
          ma = -cte.y_bearing;
        }
        
        float sh = 0;
        float sa = 0;
        if(special_center){
          cg.index = special_center;
          cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
          sh = cte.height;
          sa = -cte.y_bearing;
          
          cg.y-= sh/2 + info.index * mh + th - ta;
        }
        else
          cg.y-= info.index * mh / 2 + th - ta;
          
        cg.index = top;
        context->canvas->show_glyphs(&cg, 1);
        
        cg.y+= th - ta + ma;
        cg.index = middle;
        for(int i = 0;i < info.index;++i){
          context->canvas->show_glyphs(&cg, 1);
          cg.y+= mh;
        }
        
        if(special_center){
          cg.y+= sa - ma;
          cg.index = special_center;
          context->canvas->show_glyphs(&cg, 1);
          
          cg.y+= sh - sa + ma;
          cg.index = middle;
          for(int i = 0;i < info.index;++i){
            context->canvas->show_glyphs(&cg, 1);
            cg.y+= mh;
          }
        }
        
        cg.y-= ma;
        
        cg.index = bottom;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        cg.y+= -cte.y_bearing;
        context->canvas->show_glyphs(&cg, 1);
      }
    }
  }
  else
    TextShaper::show_glyph(context, x, y, ch, info);
}

bool SimpleMathShaper::horizontal_stretch_char(
  Context        *context,
  float           width,
  const uint16_t  ch,
  GlyphInfo      *result
){
  if(get_style().italic){
    return math_set_style(get_style() - Italic)->horizontal_stretch_char(
      context, width, ch, result);
  }
  
  if(result->right >= width)
    return true;
  
  const uint8_t  *fonts;
  const uint16_t *glyphs;
  int count = h_stretch_glyphs(ch, &fonts, &glyphs);
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = 0;
  cg.y = 0;
  
  uint16_t left, middle, right, special_center;
  int fontindex = h_stretch_big_glyphs(
    ch,
    &left,
    &middle,
    &right,
    &special_center);
  
  for(int i = 0;i < count;++i){
    context->canvas->set_font_face(font(fonts[i]));
    
    cg.index = glyphs[i];
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    if(width <= cte.x_advance || (i == count - 1 && (!left || !right))){
      result->fontinfo = fonts[i];
      result->index = cg.index;
      result->composed = 0;
      result->is_normal_text = 0;
      result->x_offset = 0;
      result->right = cte.x_advance;
      return true;
    }
  }
  
  if(!left && !middle && !right && !special_center)
    return false;
    
  context->canvas->set_font_face(font(fontindex));
    
  if(middle){
    float w = width;
    result->composed = 1;
    result->is_normal_text = 0;
    result->horizontal_stretch = 1;
    result->fontinfo = fontindex;
    result->x_offset = 0;
    result->right = 0;
    
    if(left){
      cg.index = left;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
      w-= cte.x_advance;
    }
    
    if(right){
      cg.index = right;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
      w-= cte.x_advance;
    }
    
    
    if(special_center){
      cg.index = special_center;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
      
      w-= cte.x_advance;
      
      w/= 2;
    }
    
    cg.index = middle;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    if(w < 0) w = 0;
    result->index = (uint16_t)floor(divide(w, cte.x_advance));
    result->right+= result->index * cte.x_advance;
    if(special_center)
      result->right+= result->index * cte.x_advance;
  }
  else{
    result->right = 0;
    if(left){
      cg.index = left;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
    }
    
    if(special_center){
      cg.index = special_center;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
    }
    
    if(right){
      cg.index = right;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      result->right+= cte.x_advance;
    }
    
    result->index = 0;
    result->composed = 1;
    result->is_normal_text = 0;
    result->horizontal_stretch = 1;
    result->fontinfo = fontindex;
    result->x_offset = 0;
  }
  
  return true;
}

void SimpleMathShaper::vertical_stretch_char(
  Context        *context,
  float           ascent,
  float           descent,
  bool            full_stretch,
  const uint16_t  ch,
  GlyphInfo      *result
){
  if(get_style().italic){
    math_set_style(get_style() - Italic)->vertical_stretch_char(
      context, ascent, descent, full_stretch, ch, result);
    
    return;
  }
  
  const uint8_t *fonts;
  const uint16_t *glyphs;
  int count = v_stretch_glyphs(ch, full_stretch, &fonts, &glyphs);
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = 0;
  cg.y = 0;
  
  uint16_t upper, lower, top, middle, bottom, special_center;
  int ulfontindex = v_stretch_pair_glyphs(ch, &upper, &lower);
  
  int fontindex = v_stretch_big_glyphs(
    ch,
    &top,
    &middle,
    &bottom,
    &special_center);
  
  float em = context->canvas->get_font_size();
  
  for(int i = 0;i < count;++i){
    context->canvas->set_font_face(font(fonts[i]));
    
    cg.index = glyphs[i];
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    if((ascent - em * 0.2 <= -cte.y_bearing && descent - em * 0.2 <= cte.height + cte.y_bearing)
    || (i == count - 1 
     && (((!top || !bottom) && (!upper || !lower))
      || ! full_stretch)))
    {
      result->index = cg.index;
      result->composed = 0;
      result->is_normal_text = 0;
      result->fontinfo = fonts[i];
      result->x_offset = 0;
      result->right = cte.x_advance;
      return;
    }
  }
  
  if(!top || !bottom || !full_stretch)
    return;
    
  context->canvas->set_font_face(font(fontindex));
    
  if(middle){
    float d = context->canvas->get_font_size() * 0.25;
    float h;
    if(ascent - d > descent + d)
      h = 2 * (ascent - d);
    else
      h = 2 * (descent + d);
      
    result->composed = 1;
    result->is_normal_text = 0;
    result->fontinfo = fontindex;
    result->x_offset = 0;
    
    cg.index = top;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    result->right = cte.x_advance;
    
    h-= cte.height;
    
    cg.index = bottom;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    h-= cte.height;
    
    if(special_center){
      cg.index = special_center;
      cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
      
      
      if(h - cte.height / 2 < 0 && upper && lower){
        context->canvas->set_font_face(font(ulfontindex));
        
        cg.index = upper;
        cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
        
        result->index = 0xFFFF;
        result->composed = 1;
        result->is_normal_text = 0;
        result->fontinfo = ulfontindex;
        result->x_offset = 0;
        result->right = cte.x_advance;
        return;
      }
      
      h-= cte.height;
      h/= 2;
    }
    
    cg.index = middle;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    if(h < 0) h = 0;
    result->index = (uint16_t)floor(divide(h, cte.height) + 0.5);
  }
  else{
    cg.index = top;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    result->index = 0;
    result->composed = 1;
    result->is_normal_text = 0;
    result->fontinfo = fontindex;
    result->x_offset = 0;
    result->right = cte.x_advance;
  }
}

void SimpleMathShaper::accent_positions(
  Context           *context,
  MathSequence          *base,
  MathSequence          *under,
  MathSequence          *over,
  float             *base_x,
  float             *under_x,
  float             *under_y,
  float             *over_x,
  float             *over_y
){
  float em = context->canvas->get_font_size();
  float w = base->extents().width;
  
  *under_y = base->extents().descent + 0.2f * em;
  if(under){
    *under_y+= under->extents().ascent;
    
    if(w < under->extents().width)
       w = under->extents().width;
  }
  
  *over_y = -base->extents().ascent - 0.2f * em;
  if(over){
    *over_y-= over->extents().descent;
    
    if(w < over->extents().width)
       w = over->extents().width;
  }
  
  *base_x = (w - base->extents().width) / 2;
  
  if(under)
    *under_x = (w - under->extents().width) / 2;
  else
    *under_x = 0;
    
  if(over)
    *over_x = (w - over->extents().width) / 2;
  else
    *over_x = 0;
}

void SimpleMathShaper::script_positions(
  Context           *context,
  float              base_ascent,
  float              base_descent,
  MathSequence          *sub,
  MathSequence          *super,
  float             *sub_y,
  float             *super_y
){
  float em = context->canvas->get_font_size();
  
  *sub_y = base_descent;// + 0.2 * em;
  if(*sub_y < 0.2 * em)
     *sub_y = 0.2 * em;
    
  if(sub){
    if(super){
      if(*sub_y < -0.3f * em + sub->extents().ascent)
         *sub_y = -0.3f * em + sub->extents().ascent;
    }
    else{
      if(*sub_y < -0.4f * em + sub->extents().ascent)
         *sub_y = -0.4f * em + sub->extents().ascent;
    }
  }
  
  *super_y = 0.5f * em - base_ascent;
  if(super){
    if(*super_y > - 0.55f * em - super->extents().descent)
       *super_y = - 0.55f * em - super->extents().descent;
      // 0.7 em
  }
}

void SimpleMathShaper::script_corrections(
  Context           *context,
  uint16_t           base_char, 
  const GlyphInfo   &base_info,
  MathSequence          *sub,
  MathSequence          *super,
  float              sub_y,
  float              super_y,
  float             *sub_x,
  float             *super_x
){
  *sub_x = *super_x = 0;
}

void SimpleMathShaper::shape_fraction(
  Context        *context,
  const BoxSize  &num,
  const BoxSize  &den,
  float          *num_y,
  float          *den_y,
  float          *width
){
  float em = context->canvas->get_font_size();
  
  *num_y =  -num.descent - 0.4 * em;
  *den_y =   den.ascent  - 0.1 * em;
  
  if(num.width > den.width)
    *width = num.width + 0.2 * em;
  else
    *width = den.width + 0.2 * em;
}

void SimpleMathShaper::show_fraction(
  Context        *context,
  float           width
){
  float em = context->canvas->get_font_size();
  float x1, y1, x2, y2;
  context->canvas->current_pos(&x1, &y1);
  
  x2 = x1 + width;
  y2 = y1-= 0.25 * em;
  y1-= 0.05 * em;
  y2-= 0.05 * em;
  
  context->canvas->align_point(&x1, &y1, false);
  context->canvas->align_point(&x2, &y2, false);
  
  if(y1 != y2){
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y1);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x1, y2);
    context->canvas->fill();
  }
  else{
    y2+= 0.75;
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y1);
    context->canvas->line_to(x2, y2);
    context->canvas->line_to(x1, y2);
    context->canvas->fill();
  }
}

float SimpleMathShaper::italic_correction(
  Context          *context,
  uint16_t          ch,
  const GlyphInfo  &info
){
  return 0;
}

void SimpleMathShaper::shape_radical(
  Context          *context,    // in
  BoxSize          *box,        // in/out
  float            *radicand_x, // out
  float            *exponent_x, // out
  float            *exponent_y, // out
  RadicalShapeInfo *info        // out
){
  if(get_style().italic){
    math_set_style(get_style() - Italic)->shape_radical(
      context, box, radicand_x, exponent_x, exponent_y, info);
      
    return;
  }
  
  context->canvas->set_font_face(font(_radical_font));
  const SmallRadicalGlyph *srg = small_radical_glyphs();
  
  if(box->ascent < 0)  box->ascent  = 0;
  if(box->descent < 0) box->descent = 0;
  
  float hbar_height = context->canvas->get_font_size() * 0.05;
  
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = cg.y = 0;
  int i;
  for(i = 0;srg[i].index;++i){
    cg.index = srg[i].index;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    *radicand_x = cte.x_advance;
    *exponent_x = srg[i].rel_exp_x * cte.width;
    *exponent_y = srg[i].rel_exp_y * cte.height
      + srg[i].rel_ascent * cte.height + hbar_height + cte.y_bearing;
    
    if(box->descent >= (1 - srg[i].rel_ascent) * cte.height)
      info->y_offset = box->descent - cte.height - cte.y_bearing;
    else
      info->y_offset = - srg[i].rel_ascent * cte.height - hbar_height - cte.y_bearing;
    
    if(info->y_offset + cte.y_bearing + hbar_height > - box->ascent)
      info->y_offset = - box->ascent - hbar_height - cte.y_bearing;
    
    if(box->height() < cte.height - hbar_height)
      break;
  }
  
  if(srg[i].index){
    info->size = -1 - i;
    
    cg.index = srg[i].hbar_index;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    info->hbar = (int)ceil(divide(cte.x_advance/2 + box->width, cte.x_advance));
    box->width = *radicand_x + (info->hbar) * cte.x_advance;
    
    cg.index = srg[i].index;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    box->ascent = -cte.y_bearing - info->y_offset;
    if(box->descent < cte.height - box->ascent)
       box->descent = cte.height - box->ascent;
    *exponent_y+= info->y_offset;
    
    return;
  }
  
  uint16_t bottom;
  uint16_t vertical;
  uint16_t edge;
  uint16_t horizontal;
  big_radical_glyphs(
    &bottom,
    &vertical,
    &edge,
    &horizontal,
    exponent_x,
    exponent_y);
  
  float h = box->height();
  cg.index = bottom;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  info->y_offset = box->descent - cte.height;
  *radicand_x = cte.x_advance;
  *exponent_x*= cte.x_advance;
  *exponent_y = *exponent_y * cte.height + info->y_offset;
  h-= cte.height;
  
  cg.index = edge;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  box->ascent+= cte.height;
  h+= cte.height;
  
  cg.index = vertical;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  info->size = (int)divide(h, cte.height);
  box->ascent-= h;
  box->ascent+= (1 + info->size) * cte.height;
  
  cg.index = horizontal;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  info->hbar = (int)ceil(divide(box->width, cte.x_advance));
  
  box->width = *radicand_x + (0.5 + info->hbar) * cte.x_advance;
}

void SimpleMathShaper::show_radical(
  Context                *context,
  const RadicalShapeInfo &info
){
  if(get_style().italic){
    math_set_style(get_style() - Italic)->show_radical(
      context, info);
      
    return;
  }
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  context->canvas->set_font_face(font(_radical_font));
  
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = x;
  cg.y = y + info.y_offset;
  
  if(info.size < 0){
    const SmallRadicalGlyph &g = small_radical_glyphs()[-1 - info.size];
    cg.index = g.index;
    context->canvas->show_glyphs(&cg, 1);
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    
    cg.y+= cte.y_bearing;
    cg.x+= cte.x_advance;
    cg.index = g.hbar_index;
    cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
    cg.y-= cte.y_bearing;
    for(int i = 0;i < info.hbar;++i){
      context->canvas->show_glyphs(&cg, 1);
      cg.x+= cte.x_advance;
    }
    return;
  }
  
  float dummyx, dummyy;
  uint16_t bottom;
  uint16_t vertical;
  uint16_t edge;
  uint16_t horizontal;
  big_radical_glyphs(
    &bottom,
    &vertical,
    &edge,
    &horizontal,
    &dummyx,
    &dummyy);
  
  cg.index = bottom;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  cg.y-= cte.y_bearing;
  context->canvas->show_glyphs(&cg, 1);
  
  cg.y+= cte.y_bearing;
  cg.index = vertical;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  cg.y-= cte.y_bearing;
  for(int i = 0;i < info.size;++i){
    cg.y-= cte.height;
    context->canvas->show_glyphs(&cg, 1);
  }
  
  cg.y+= cte.y_bearing;
  cg.index = edge;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  cg.y-= cte.height + cte.y_bearing;
  context->canvas->show_glyphs(&cg, 1);
  
  cg.y+= cte.y_bearing;
  cg.x+= cte.x_advance;
  cg.index = horizontal;
  cairo_glyph_extents(context->canvas->cairo(), &cg, 1, &cte);
  cg.y-= cte.y_bearing;
  for(int i = 0;i < info.hbar;++i){
    context->canvas->show_glyphs(&cg, 1);
    cg.x+= cte.x_advance;
  }
}

void SimpleMathShaper::get_script_size_multis(Array<float> *arr){
  arr->length(1, 0.71f);
}

int SimpleMathShaper::v_stretch_pair_glyphs(
  uint16_t  ch,
  uint16_t *upper,
  uint16_t *lower
){
  *upper = *lower = 0;
  return 0;
}

//} ... class SimpleMathShaper
