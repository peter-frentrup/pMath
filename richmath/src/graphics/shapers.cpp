#include <graphics/shapers.h>

#include <cmath>

#include <boxes/mathsequence.h>

#include <graphics/context.h>
#include <graphics/rectangle.h>


#ifdef RICHMATH_USE_WIN32_FONT
#  include <graphics/win32-shaper.h>
#elif defined(RICHMATH_USE_FT_FONT)
#  include <graphics/ft-shaper.h>
#else
#  error no support for font backend
#endif


extern pmath_symbol_t richmath_FE_DollarFallbackFonts;
extern pmath_symbol_t richmath_System_List;

using namespace richmath;

static float divide(float n, float d, float fail = 0) {
  return d == 0 ? fail : n / d;
}

class FontKey {
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

static Hashtable<FontKey, SharedPtr<TextShaper> > shapers;

static Hashtable<uint32_t, uint32_t> accent_chars;

//{ class TextShaper ...

TextShaper::TextShaper()
  : Shareable()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

void TextShaper::vertical_glyph_size(
  Context         &context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = 0;
  cg.y = 0;
  
  if(ch == '\n')
    return;
    
  context.canvas().set_font_face(font(info.fontinfo));
  cg.index = info.index;
  context.canvas().glyph_extents(&cg, 1, &cte);
  
  if(info.vertical_centered) {
    float a = cte.height / 2 + get_center_height(context, info.fontinfo);
    float d = cte.height - a;
    
    if(*ascent < a)
      *ascent = a;
    if(*descent < d)
      *descent = d;
    return;
  }
  
  if(*ascent < -cte.y_bearing)
    *ascent = -cte.y_bearing;
  if(*descent < cte.height + cte.y_bearing)
    *descent = cte.height + cte.y_bearing;
}

void TextShaper::show_glyph(
  Context         &context,
  Point            pos,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  bool workaround = false;
  
  cairo_surface_t *target = context.canvas().target();
  switch(cairo_surface_get_type(target)) {
    case CAIRO_SURFACE_TYPE_IMAGE:
    case CAIRO_SURFACE_TYPE_WIN32:
      workaround = (cairo_image_surface_get_format(target) == CAIRO_FORMAT_ARGB32);
      break;
      
    default:
      break;
  }
  
  FontFace ff = font(info.fontinfo);
  
  static GlyphInfo space_glyph;
  if(workaround) {
    /* Workaround a Cairo (1.8.8) bug:
        Platform: Windows, Cleartype on, ARGB32 image or HDC
        The last (cleartype-blured) pixel column of the last glyph and the zero-th
        column (also cleartype-blured) of the first pixel in a glyph-string wont
        be drawn.
        That looks ugly, so we add invisible glyphs at the first and the last
        index with adjusted x-positions.
    
        To see the difference, draw something to the glass area of the window (an
        ARGB32-image surface is used there) with and without this workaround.
     */
    static cairo_font_face_t *prev_font = nullptr;
    if(prev_font != ff.cairo()) {
      prev_font = ff.cairo();
      
      memset(&space_glyph, 0, sizeof(space_glyph));
      const uint16_t space = ' ';
      decode_token(context, 1, &space, &space_glyph);
    }
    
    workaround = space_glyph.fontinfo == info.fontinfo && space_glyph.index != 0xFFFF;
  }
  
  context.canvas().set_font_face(ff);
  
  if(info.vertical_centered) {
    cairo_text_extents_t cte;
    cairo_glyph_t cg;
    cg.index = info.index;
    cg.x = 0;
    cg.y = 0;
    
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    pos.y -= get_center_height(context, info.fontinfo);
    pos.y -= cte.height / 2;
    pos.y -= cte.y_bearing;
  }
  
  if(workaround) {
    cairo_glyph_t cg[3];
    cg[0].index = space_glyph.index;
    cg[0].x = pos.x - 3.0;
    cg[0].y = pos.y;
    cg[1].index = info.index;
    cg[1].x = pos.x + info.x_offset;
    cg[1].y = pos.y;
    cg[2].index = space_glyph.index;
    cg[2].x = pos.x + info.right + 3.0;
    cg[2].y = pos.y;
    
    context.canvas().show_glyphs(cg, 3);
  }
  else {
    cairo_glyph_t cg;
    cg.index = info.index;
    cg.x = pos.x + info.x_offset;
    cg.y = pos.y;
    
    context.canvas().show_glyphs(&cg, 1);
  }
}

float TextShaper::get_center_height(Context &context, uint8_t fontinfo) {
  return context.canvas().get_font_size() * 0.25;
}

SharedPtr<TextShaper> TextShaper::find(
  const String  &name,
  FontStyle      style
) {
  FontKey key(name, style);
  SharedPtr<TextShaper> *result = shapers.search(key);
  SharedPtr<TextShaper> fs;
  
  if(result)
    return *result;
    
  fs =
#ifdef RICHMATH_USE_WIN32_FONT
    new WindowsFontShaper(name, style);
#elif defined(RICHMATH_USE_FT_FONT)
    new FreetypeFontShaper(name, style);
#else
    no support for font backend
#endif
    
  shapers.set(key, fs);
  return fs;
}

uint32_t TextShaper::get_accent_char(uint32_t input_char) {
  if(accent_chars.size() == 0) {
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

void TextShaper::clear_cache() {
  shapers.clear();
  accent_chars.clear();
}

//} ... class TextShaper

//{ class FallbackTextShaper ...

static Expr default_fallback_fontlist;
static int fallback_shaper_count = 0;

FallbackTextShaper::FallbackTextShaper(SharedPtr<TextShaper> default_shaper)
  : TextShaper()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  RICHMATH_ASSERT(default_shaper.is_valid());
  
  if(auto fts = dynamic_cast<FallbackTextShaper *>(default_shaper.ptr())) {
    _shapers = fts->_shapers;
  }
  else
    _shapers.add(default_shaper);
  
  if(++fallback_shaper_count == 1) {
    default_fallback_fontlist = Expr{pmath_symbol_get_value(richmath_FE_DollarFallbackFonts)};
    
    if(default_fallback_fontlist[0] != richmath_System_List)
      default_fallback_fontlist = Expr();
  }
}

FallbackTextShaper::~FallbackTextShaper() {
  if(--fallback_shaper_count == 0)
    default_fallback_fontlist = Expr();
}

int FallbackTextShaper::fallback_index(uint8_t *fontinfo) {
  int i = 0;
  while(i < _shapers.length() - 1) {
    if(*fontinfo < _shapers[i]->num_fonts())
      return i;
      
    *fontinfo -= _shapers[i]->num_fonts();
    ++i;
  }
  
  return i;
}

int FallbackTextShaper::first_missing_glyph(int len, const GlyphInfo *glyphs) {
  int result = 0;
  while(result < len && glyphs[result].index != UnknownGlyph)
    ++result;
    
  return result;
}

void FallbackTextShaper::add(SharedPtr<TextShaper> fallback) {
  RICHMATH_ASSERT(fallback.is_valid());
  
  int own_num = num_fonts();
  if(own_num + fallback->num_fonts() > NumFontsPerGlyph) {
    if(auto fts = dynamic_cast<FallbackTextShaper *>(fallback.ptr())) {
      for(int i = 0; i < fts->_shapers.length(); ++i)
        add(fts->_shapers[i]);
    }
  }
  else {
    _shapers.add(fallback);
  }
}

void FallbackTextShaper::add_default() {
  for(size_t i = 1; i <= default_fallback_fontlist.expr_length(); ++i) {
    String s = String(default_fallback_fontlist[i]);
    
    if(s.length() > 0)
      add(TextShaper::find(s, get_style()));
  }
  
  // todo: Ensure that CharBoxTextShaper is allways available (i.e. no more
  //       than 15 other fonts before) Note that there might be other fonts
  //       before this FallbackTextShaper, so we do not know the number of fonts
  //       here.
  add(new CharBoxTextShaper);
}

void FallbackTextShaper::add_or_create(SharedPtr<FallbackTextShaper> &all, SharedPtr<TextShaper> fallback) {
  if(all)
    all->add(PMATH_CPP_MOVE(fallback));
  else
    all = new FallbackTextShaper(PMATH_CPP_MOVE(fallback));
}

uint8_t FallbackTextShaper::num_fonts() {
  uint8_t result = 0;
  
  for(int i = 0; i < _shapers.length() && result < NumFontsPerGlyph; ++i) {
    result += _shapers[i]->num_fonts();
  }
  
  return result;
}

FontFace FallbackTextShaper::font(uint8_t fontinfo) {
  int i = fallback_index(&fontinfo);
  
  return _shapers[i]->font(fontinfo);
}

String FallbackTextShaper::font_name(uint8_t fontinfo) {
  int i = fallback_index(&fontinfo);
  
  return _shapers[i]->font_name(fontinfo);
}

void FallbackTextShaper::decode_token(
  Context        &context,
  int             len,
  const uint16_t *str,
  GlyphInfo      *result
) {
  TextShaper *ts = _shapers[0].ptr();
  
  ts->decode_token(context, len, str, result);
  uint8_t inc_fontinfo = ts->num_fonts();
  
  for(int i = 1; i < _shapers.length(); ++i) {
    int first = first_missing_glyph(len, result);
    int start = first;
    int end = len;
    
    if(start >= len)
      break;
      
    ts = _shapers[i].ptr();
    
    do {
      int next = start;
      while(next < len && result[next].index == UnknownGlyph)
        ++next;
        
      ts->decode_token(
        context,
        next - start,
        str + start,
        result + start);
        
      for(; start < next; ++start) {
        result[start].fontinfo += inc_fontinfo;
      }
      
      end = next;
      while(start < len && result[start].index != UnknownGlyph)
        ++start;
    } while(start < len);
    
    str   += first;
    result += first;
    len    = end - first;
    
    inc_fontinfo += ts->num_fonts();
  }
}

void FallbackTextShaper::vertical_glyph_size(
  Context         &context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  uint8_t fontinfo = info.fontinfo;
  int i = fallback_index(&fontinfo);
  
  if(i == 0) {
    _shapers[0]->vertical_glyph_size(context, ch, info, ascent, descent);
  }
  else {
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(gi));
    
    gi.fontinfo = fontinfo;
    
    _shapers[i]->vertical_glyph_size(context, ch, gi, ascent, descent);
  }
}

void FallbackTextShaper::show_glyph(
  Context         &context,
  Point            pos,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  uint8_t fontinfo = info.fontinfo;
  int i = fallback_index(&fontinfo);
  
  if(i == 0) {
    _shapers[0]->show_glyph(context, pos, ch, info);
  }
  else {
    GlyphInfo gi;
    memcpy(&gi, &info, sizeof(gi));
    
    gi.fontinfo = fontinfo;
    
    _shapers[i]->show_glyph(context, pos, ch, gi);
  }
}

SharedPtr<TextShaper> FallbackTextShaper::set_style(FontStyle style) {
  if(style == get_style()) {
    ref();
    return this;
  }
  
  FallbackTextShaper *fts = new FallbackTextShaper(_shapers[0]->set_style(style));
  
  /* Array's effectively don't shrink, so we set the final buffer size once: */
  fts->_shapers.length(_shapers.length());
  fts->_shapers.length(1);
  
  for(int i = 0; i < _shapers.length(); ++i) {
    fts->add(_shapers[i]->set_style(style));
  }
  
  return SharedPtr<TextShaper>(fts);
}

FontStyle FallbackTextShaper::get_style() {
  return _shapers[0]->get_style();
}

float FallbackTextShaper::get_center_height(Context &context, uint8_t fontinfo) {
  int i = fallback_index(&fontinfo);
  
  return _shapers[i]->get_center_height(context, fontinfo);
}

//} ... class FallbackTextShaper

//{ class CharBoxTextShaper ...

static const uint16_t CharBoxError  = 1;
static const uint16_t CharBoxSingle = 2;

FontFace digit_font;
static int num_cbts = 0;

CharBoxTextShaper::CharBoxTextShaper()
  : TextShaper()
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  
  if(++num_cbts == 1) {
    digit_font = FontFace("sans", NoStyle);
  }
}

CharBoxTextShaper::~CharBoxTextShaper() {
  if(--num_cbts == 0) {
    digit_font = FontFace();
  }
}

void CharBoxTextShaper::decode_token(
  Context        &context,
  int             len,
  const uint16_t *str,
  GlyphInfo      *result
) {
  if(!context.boxchar_fallback_enabled) {
    for(int i = 0; i < len; ++i)
      result[i].index = UnknownGlyph;
      
    return;
  }
  
  float em = context.canvas().get_font_size();
  
  for(int i = 0; i < len; ++i) {
    if( i + 1 < len &&
        is_utf16_high(str[i]) &&
        is_utf16_low(str[i + 1]))
    {
      result[i].index = str[i + 1];
      result[i].right = em;
      
      result[i + 1].index = IgnoreGlyph;
      result[i + 1].right = 0.0;
      ++i;
    }
    else if(is_utf16_low(str[i]) || is_utf16_high(str[i])) {
      result[i].index = CharBoxError;
      result[i].right = em;
    }
    else {
      result[i].index = CharBoxSingle;
      result[i].right = em;
    }
  }
}

void CharBoxTextShaper::vertical_glyph_size(
  Context         &context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  float em = context.canvas().get_font_size();
  
  if(*ascent < 0.75 * em)
    *ascent = 0.75 * em;
  if(*descent < 0.25 * em)
    *descent = 0.25 * em;
}

void CharBoxTextShaper::show_glyph(
  Context         &context,
  Point            pos,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  static const char *hex = "0123456789ABCDEF";
  char str1[4] = "? ?";
  char str2[4] = "? ?";
  
  if(is_utf16_low(info.index)) {
    uint32_t unicode = 0x10000 + ((((uint32_t)ch & 0x03FF) << 10) | (info.index & 0x03FF));
    
    str1[0] = hex[(unicode & 0xF00000) >> 20];
    str1[1] = hex[(unicode & 0x0F0000) >> 16];
    str1[2] = hex[(unicode & 0x00F000) >> 12];
    
    str2[0] = hex[(unicode & 0x000F00) >> 8];
    str2[1] = hex[(unicode & 0x0000F0) >> 4];
    str2[2] = hex[ unicode & 0x00000F];
    
  }
  else if(info.index == CharBoxSingle) {
    str1[0] = hex[(ch & 0xF000) >> 12];
    str1[2] = hex[(ch & 0x0F00) >> 8];
    
    str2[0] = hex[(ch & 0x00F0) >> 4];
    str2[2] = hex[ ch & 0x000F];
  }
  else if(info.index != CharBoxError)
    return;
    
  float em = context.canvas().get_font_size();
  pos.y -= 0.75 * em;
  
  bool sot = context.canvas().show_only_text;
  context.canvas().show_only_text = false;
  context.canvas().set_font_size(0.4 * em);
  context.canvas().set_font_face(digit_font);
  
  {
    RectangleF rect(pos, Vector2F{em, em});
    //BoxRadius radii(0.1 * em);
    
    rect.normalize();
    rect.pixel_align(context.canvas(), false, +1);
    
    //radii.normalize(rect.width, rect.height);
    rect.add_rect_path(context.canvas(), false);
    
    Vector2F delta(-0.1f * em, -0.1f * em);
    delta.pixel_align_distance(context.canvas());
    
    rect.grow(delta);
    rect.normalize_to_zero();
    
    rect.add_rect_path(context.canvas(), true);
    
    context.canvas().fill();
  }
  
  float inner = 0.8 * em;
  pos.x += 0.1 * em;
  pos.y += 0.1 * em;
  
  cairo_text_extents_t te1, te2;
  cairo_text_extents(context.canvas().cairo(), str1, &te1);
  cairo_text_extents(context.canvas().cairo(), str2, &te2);
  
  context.canvas().move_to(
    pos.x + (inner     - te1.width) / 2  - te1.x_bearing,
    pos.y + (inner / 2 - te1.height) / 2 - te1.y_bearing);
    
  if(context.canvas().native_show_glyphs) {
    cairo_show_text(context.canvas().cairo(), str1);
  }
  else {
    cairo_text_path(context.canvas().cairo(), str1);
    context.canvas().fill();
  }
  
  context.canvas().move_to(
    pos.x +             (inner     - te2.width) / 2  - te2.x_bearing,
    pos.y + inner / 2 + (inner / 2 - te2.height) / 2 - te2.y_bearing);
    
  if(context.canvas().native_show_glyphs) {
    cairo_show_text(context.canvas().cairo(), str2);
  }
  else {
    cairo_text_path(context.canvas().cairo(), str2);
    context.canvas().fill();
  }
  
  context.canvas().set_font_size(em);
  context.canvas().show_only_text = sot;
}

SharedPtr<TextShaper> CharBoxTextShaper::set_style(FontStyle style) {
  ref();
  return this;
}

//} ... class CharBoxTextShaper

//{ class MathShaper ...

const float MathShaper::SpanMaxRestrictedSizeFactor = 1.5f; 
Hashtable<String, SharedPtr<MathShaper> > MathShaper::available_shapers;

SharedPtr<MathShaper> MathShaper::math_set_style(FontStyle style) {
  SharedPtr<TextShaper> ts = set_style(style);
  SharedPtr<MathShaper> ms(static_cast<MathShaper *>(ts.release()));
  
  return ms;
}

//} ... class MathShaper

//{ class SimpleMathShaper ...

SimpleMathShaper::SimpleMathShaper(int radical_font)
  : MathShaper(),
    _radical_font(radical_font)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

SimpleMathShaper::~SimpleMathShaper() {
}

float SimpleMathShaper::flattened_accent_base_height(Context &context) {
  return context.canvas().get_font_size();
}

void SimpleMathShaper::vertical_glyph_size(
  Context         &context,
  const uint16_t   ch,
  const GlyphInfo &info,
  float           *ascent,
  float           *descent
) {
  if(info.composed) {
    if(info.horizontal_stretch) {
      uint16_t left, middle, right, special_center;
      h_stretch_big_glyphs(
        ch,
        &left,
        &middle,
        &right,
        &special_center);
        
      if(left || middle || right || special_center) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        
        if(left) {
          cg.index = left;
          context.canvas().glyph_extents(&cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
            *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
            *descent = cte.height + cte.y_bearing;
        }
        
        if(middle) {
          cg.index = middle;
          context.canvas().glyph_extents(&cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
            *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
            *descent = cte.height + cte.y_bearing;
        }
        
        if(right) {
          cg.index = right;
          context.canvas().glyph_extents(&cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
            *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
            *descent = cte.height + cte.y_bearing;
        }
        
        if(special_center) {
          cg.index = special_center;
          context.canvas().glyph_extents(&cg, 1, &cte);
          if(*ascent < -cte.y_bearing)
            *ascent = -cte.y_bearing;
          if(*descent < cte.height + cte.y_bearing)
            *descent = cte.height + cte.y_bearing;
        }
      }
    }
    else if(info.index == UnknownGlyph) {
      uint16_t upper, lower;
      v_stretch_pair_glyphs(
        ch,
        &upper,
        &lower);
        
      if(upper && lower) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        
        float d = context.canvas().get_font_size() * 0.25;
        float h = 0;
        
        context.canvas().set_font_face(font(info.fontinfo));
        
        cg.index = upper;
        context.canvas().glyph_extents(&cg, 1, &cte);
        h += cte.height;
        
        cg.index = lower;
        context.canvas().glyph_extents(&cg, 1, &cte);
        h += cte.height;
        
        h /= 2;
        if(*ascent < h + d)
          *ascent = h + d;
        if(*descent < h - d)
          *descent = h - d;
      }
    }
    else {
      uint16_t top, middle, bottom, special_center;
      v_stretch_big_glyphs(
        ch,
        &top,
        &middle,
        &bottom,
        &special_center);
        
      if(top && bottom) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = 0;
        cg.y = 0;
        
        float d = context.canvas().get_font_size() * 0.25;
        float h = 0;
        
        context.canvas().set_font_face(font(info.fontinfo));
        
        cg.index = top;
        context.canvas().glyph_extents(&cg, 1, &cte);
        h += cte.height;
        
        cg.index = bottom;
        context.canvas().glyph_extents(&cg, 1, &cte);
        h += cte.height;
        
        if(special_center) {
          cg.index = special_center;
          context.canvas().glyph_extents(&cg, 1, &cte);
          
          h += cte.height;
        }
        
        if(middle) {
          cg.index = middle;
          context.canvas().glyph_extents(&cg, 1, &cte);
          
          if(special_center) {
            h += info.ext.num_extenders * cte.height * 2;
          }
          else
            h += info.ext.num_extenders * cte.height;
        }
        
        h /= 2;
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
  Context         &context,
  Point            pos,
  const uint16_t   ch,
  const GlyphInfo &info
) {
  if(info.composed) {
    if(get_style().italic) {
      math_set_style(get_style() - Italic)->show_glyph(context, pos, ch, info);
        
      return;
    }
    
    if(info.horizontal_stretch) {
      uint16_t left, middle, right, special_center;
      h_stretch_big_glyphs(
        ch,
        &left,
        &middle,
        &right,
        &special_center);
        
      if(left || middle || right || special_center) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = pos.x + info.x_offset;
        cg.y = pos.y;
        
        context.canvas().set_font_face(font(info.fontinfo));
        
        if(left) {
          cg.index = left;
          context.canvas().glyph_extents(&cg, 1, &cte);
          context.canvas().show_glyphs(&cg, 1);
          cg.x += cte.x_advance;
        }
        
        if(middle && info.ext.num_extenders > 0) {
          cg.index = middle;
          context.canvas().glyph_extents(&cg, 1, &cte);
          for(int i = 0; i < info.ext.num_extenders; ++i) {
            context.canvas().show_glyphs(&cg, 1);
            cg.x += cte.x_advance;
          }
        }
        
        if(special_center) {
          cg.index = special_center;
          context.canvas().glyph_extents(&cg, 1, &cte);
          context.canvas().show_glyphs(&cg, 1);
          cg.x += cte.x_advance;
          
          if(middle && info.ext.num_extenders > 0) {
            cg.index = middle;
            context.canvas().glyph_extents(&cg, 1, &cte);
            for(int i = 0; i < info.ext.num_extenders; ++i) {
              context.canvas().show_glyphs(&cg, 1);
              cg.x += cte.x_advance;
            }
          }
        }
        
        if(right) {
          cg.index = right;
          context.canvas().show_glyphs(&cg, 1);
        }
      }
    }
    else if(info.index == UnknownGlyph) {
      uint16_t upper, lower;
      v_stretch_pair_glyphs(
        ch,
        &upper,
        &lower);
        
      if(upper && lower) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = pos.x + info.x_offset;
        cg.y = pos.y - context.canvas().get_font_size() * 0.25;
        
        context.canvas().set_font_face(font(info.fontinfo));
        
        cg.index = upper;
        context.canvas().glyph_extents(&cg, 1, &cte);
        float th = cte.height;
        float ta = -cte.y_bearing;
        cg.y -= th - ta;
        
        context.canvas().show_glyphs(&cg, 1);
        
        cg.y += th - ta;
        
        cg.index = lower;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        cg.y += -cte.y_bearing;
        context.canvas().show_glyphs(&cg, 1);
      }
    }
    else {
      uint16_t top, middle, bottom, special_center;
      v_stretch_big_glyphs(
        ch,
        &top,
        &middle,
        &bottom,
        &special_center);
        
      if(top && bottom) {
        cairo_text_extents_t cte;
        cairo_glyph_t cg;
        cg.x = pos.x + info.x_offset;
        cg.y = pos.y - context.canvas().get_font_size() * 0.25;
        
        context.canvas().set_font_face(font(info.fontinfo));
        
        cg.index = top;
        context.canvas().glyph_extents(&cg, 1, &cte);
        float th = cte.height;
        float ta = -cte.y_bearing;
        
        cg.index = bottom;
        context.canvas().glyph_extents(&cg, 1, &cte);
        float bh = cte.height;
        float ba = -cte.y_bearing;
        
        float mh = 0;
        float ma = 0;
        if(middle && info.ext.num_extenders > 0) {
          cg.index = middle;
          context.canvas().glyph_extents(&cg, 1, &cte);
          mh = cte.height;
          ma = -cte.y_bearing;
        }
        
        float sh = 0;
        float sa = 0;
        if(special_center) {
          cg.index = special_center;
          context.canvas().glyph_extents(&cg, 1, &cte);
          sh = cte.height;
          sa = -cte.y_bearing;
          
          cg.y -= (th + sh + bh + 2 * info.ext.num_extenders * mh) / 2;
        }
        else
          cg.y -= (th + bh + info.ext.num_extenders * mh) / 2;
        
        cg.y+= ta;
        cg.index = top;
        context.canvas().show_glyphs(&cg, 1);
        
        cg.y += th - ta + ma;
        cg.index = middle;
        for(int i = 0; i < info.ext.num_extenders; ++i) {
          context.canvas().show_glyphs(&cg, 1);
          cg.y += mh;
        }
        
        if(special_center) {
          cg.y += sa - ma;
          cg.index = special_center;
          context.canvas().show_glyphs(&cg, 1);
          
          cg.y += sh - sa + ma;
          cg.index = middle;
          for(int i = 0; i < info.ext.num_extenders; ++i) {
            context.canvas().show_glyphs(&cg, 1);
            cg.y += mh;
          }
        }
        
        cg.y -= ma;
        
        cg.index = bottom;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        cg.y += -cte.y_bearing;
        context.canvas().show_glyphs(&cg, 1);
      }
    }
  }
  else
    TextShaper::show_glyph(context, pos, ch, info);
}

bool SimpleMathShaper::horizontal_stretch_char(
  Context        &context,
  float           width,
  const uint16_t  ch,
  GlyphInfo      *result
) {
  if(get_style().italic) {
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
                    
  for(int i = 0; i < count; ++i) {
    context.canvas().set_font_face(font(fonts[i]));
    
    cg.index = glyphs[i];
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    if(width <= cte.x_advance || (i == count - 1 && (!left || !right))) {
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
    
  context.canvas().set_font_face(font(fontindex));
  
  if(middle) {
    float w = width;
    result->composed = 1;
    result->is_normal_text = 0;
    result->horizontal_stretch = 1;
    result->fontinfo = fontindex;
    result->x_offset = 0;
    result->right = 0;
    
    if(left) {
      cg.index = left;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
      w -= cte.x_advance;
    }
    
    if(right) {
      cg.index = right;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
      w -= cte.x_advance;
    }
    
    
    if(special_center) {
      cg.index = special_center;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
      
      w -= cte.x_advance;
      
      w /= 2;
    }
    
    cg.index = middle;
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    if(w < 0) w = 0;
    result->ext.num_extenders = (uint16_t)floor(divide(w, cte.x_advance));
    result->ext.rel_overlap = 0;
    result->right += result->ext.num_extenders * cte.x_advance;
    if(special_center)
      result->right += result->ext.num_extenders * cte.x_advance;
  }
  else {
    result->right = 0;
    if(left) {
      cg.index = left;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
    }
    
    if(special_center) {
      cg.index = special_center;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
    }
    
    if(right) {
      cg.index = right;
      context.canvas().glyph_extents(&cg, 1, &cte);
      result->right += cte.x_advance;
    }
    
    result->ext.num_extenders = 0;
    result->ext.rel_overlap = 0;
    result->composed = 1;
    result->is_normal_text = 0;
    result->horizontal_stretch = 1;
    result->fontinfo = fontindex;
    result->x_offset = 0;
  }
  
  return true;
}

void SimpleMathShaper::vertical_stretch_char(
  Context        &context,
  float           ascent,
  float           descent,
  bool            full_stretch,
  const uint16_t  ch,
  GlyphInfo      *result
) {
  if(get_style().italic) {
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
                    
  float em = context.canvas().get_font_size();
  float max_height = Infinity;
  if(!full_stretch)
    max_height = MathShaper::SpanMaxRestrictedSizeFactor * em;
  
  for(int i = 0; i < count; ++i) {
    context.canvas().set_font_face(font(fonts[i]));
    
    cg.index = glyphs[i];
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    bool is_large_enough = full_stretch ? ascent + descent <= cte.height : max_height <= cte.height;
    if(is_large_enough
        || (i == count - 1
            && (((!top || !bottom) && (!upper || !lower)) || !full_stretch)))
    {
      result->index          = cg.index;
      result->composed       = 0;
      result->is_normal_text = 0;
      result->fontinfo       = fonts[i];
      result->x_offset       = 0;
      result->right          = cte.x_advance;
      return;
    }
  }
  
  if(!top || !bottom || !full_stretch)
    return;
    
  context.canvas().set_font_face(font(fontindex));
  
  if(middle) {
    float d = context.canvas().get_font_size() * 0.25;
    float h;
    if(ascent - d > descent + d)
      h = 2 * (ascent - d);
    else
      h = 2 * (descent + d);
      
    result->composed      = 1;
    result->is_normal_text = 0;
    result->fontinfo       = fontindex;
    result->x_offset       = 0;
    
    cg.index = top;
    context.canvas().glyph_extents(&cg, 1, &cte);
    result->right = cte.x_advance;
    
    h -= cte.height;
    
    cg.index = bottom;
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    h -= cte.height;
    
    if(special_center) {
      cg.index = special_center;
      context.canvas().glyph_extents(&cg, 1, &cte);
      
      
      if(h - cte.height / 2 < 0 && upper && lower) {
        context.canvas().set_font_face(font(ulfontindex));
        
        cg.index = upper;
        context.canvas().glyph_extents(&cg, 1, &cte);
        
        result->index          = UnknownGlyph;
        result->composed       = 1;
        result->is_normal_text = 0;
        result->fontinfo       = ulfontindex;
        result->x_offset       = 0;
        result->right          = cte.x_advance;
        return;
      }
      
      h -= cte.height;
      h /= 2;
    }
    
    cg.index = middle;
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    if(h < 0) h = 0;
    result->ext.num_extenders = (uint16_t)round(divide(h, cte.height));
    result->ext.rel_overlap   = 0;
  }
  else {
    cg.index = top;
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    result->ext.num_extenders = 0;
    result->ext.rel_overlap   = 0;
    result->composed          = 1;
    result->is_normal_text    = 0;
    result->fontinfo          = fontindex;
    result->x_offset          = 0;
    result->right             = cte.x_advance;
  }
}

void SimpleMathShaper::accent_positions(
  Context           &context,
  MathSequence      *base,
  MathSequence      *under,
  MathSequence      *over,
  float             *base_x,
  Vector2F          *underscript_offset,
  Vector2F          *overscript_offset,
  bool               under_is_stretched,
  bool               over_is_stretched,
  AutoBoolValues     limits_positioning
) {
  uint16_t base_char = 0;
  if(base->length() == 1)
    base_char = base->text()[0];
    
  bool is_integral = pmath_char_is_integral(base_char);
  
  if(!under_is_stretched && !over_is_stretched) {
    bool use_subsuper = false;
    
    switch(limits_positioning) {
      case AutoBoolTrue:      use_subsuper = true;  break;
      case AutoBoolFalse:     use_subsuper = false; break;
      case AutoBoolAutomatic: use_subsuper = context.script_level > 0 && is_integral; break;
    }
    
    if(use_subsuper) {
      script_positions(
        context, base->extents().ascent, base->extents().descent,
        under, over,
        &underscript_offset->y, &overscript_offset->y);
        
      script_corrections(
        context, base_char, base->glyph_array()[0],
        under, over, 
        underscript_offset->y,  overscript_offset->y,
        &underscript_offset->x, &overscript_offset->x);
        
      *base_x = 0;
      underscript_offset->x += base->extents().width;
      overscript_offset->x += base->extents().width;
      return;
    }
  }
  
  float em = context.canvas().get_font_size();
  float w = base->extents().width;
  
  underscript_offset->y = base->extents().descent + 0.2f * em;
  if(under) {
    underscript_offset->y += under->extents().ascent;
    
    if(w < under->extents().width)
      w = under->extents().width;
  }
  
  overscript_offset->y = -base->extents().ascent - 0.2f * em;
  if(over) {
    overscript_offset->y -= over->extents().descent;
    
    if(w < over->extents().width)
      w = over->extents().width;
  }
  
  *base_x = (w - base->extents().width) / 2;
  
  if(is_integral) {
    float dummy_uy, dummy_oy;
    script_positions(
      context, base->extents().ascent, base->extents().descent,
      under, over,
      &dummy_uy, &dummy_oy);
      
    script_corrections(
      context, base_char, base->glyph_array()[0],
      under, over, 
      dummy_uy, dummy_oy,
      &underscript_offset->x, &overscript_offset->x);
      
    float diff = overscript_offset->x - underscript_offset->x;
    
    if(under)
      underscript_offset->x = (w + overscript_offset->x - diff - under->extents().width) / 2;
      
    if(over)
      overscript_offset->x = (w + overscript_offset->x + diff - over->extents().width) / 2;
      
    return;
  }
  
  if(under)
    underscript_offset->x = (w - under->extents().width) / 2;
  else
    underscript_offset->x = 0;
    
  if(over)
    overscript_offset->x = (w - over->extents().width) / 2;
  else
    overscript_offset->x = 0;
}

void SimpleMathShaper::script_positions(
  Context           &context,
  float              base_ascent,
  float              base_descent,
  MathSequence      *sub,
  MathSequence      *super,
  float             *sub_y,
  float             *super_y
) {
  float em = context.canvas().get_font_size();
  
  *sub_y = base_descent;// + 0.2 * em;
  if(*sub_y < 0.2 * em)
    *sub_y = 0.2 * em;
    
  if(sub) {
    if(super) {
      if(*sub_y < -0.3f * em + sub->extents().ascent)
        *sub_y = -0.3f * em + sub->extents().ascent;
    }
    else {
      if(*sub_y < -0.4f * em + sub->extents().ascent)
        *sub_y = -0.4f * em + sub->extents().ascent;
    }
  }
  
  *super_y = 0.5f * em - base_ascent;
  if(super) {
    if(*super_y > - 0.55f * em - super->extents().descent)
      *super_y = - 0.55f * em - super->extents().descent;
    // 0.7 em
  }
}

void SimpleMathShaper::script_corrections(
  Context           &context,
  uint16_t           base_char,
  const GlyphInfo   &base_info,
  MathSequence      *sub,
  MathSequence      *super,
  float              sub_y,
  float              super_y,
  float             *sub_x,
  float             *super_x
) {
  *sub_x = *super_x = 0;
}

void SimpleMathShaper::shape_fraction(
  Context        &context,
  const BoxSize  &num,
  const BoxSize  &den,
  float          *num_y,
  float          *den_y,
  float          *width
) {
  float em = context.canvas().get_font_size();
  
  *num_y =  -num.descent - 0.4 * em;
  *den_y =   den.ascent  - 0.1 * em;
  
  if(num.width > den.width)
    *width = num.width + 0.2 * em;
  else
    *width = den.width + 0.2 * em;
}

void SimpleMathShaper::show_fraction(Context &context, float width) {
  float em = context.canvas().get_font_size();
  float x1, y1, x2, y2;
  context.canvas().current_pos(&x1, &y1);
  
  x2 = x1 + width;
  y2 = y1 -= 0.25 * em;
  y1 -= 0.05 * em;
  y2 -= 0.05 * em;
  
  context.canvas().align_point(&x1, &y1, false);
  context.canvas().align_point(&x2, &y2, false);
  
  if(y1 != y2) {
    context.canvas().move_to(x1, y1);
    context.canvas().line_to(x2, y1);
    context.canvas().line_to(x2, y2);
    context.canvas().line_to(x1, y2);
    context.canvas().fill();
  }
  else {
    y2 += 0.75;
    context.canvas().move_to(x1, y1);
    context.canvas().line_to(x2, y1);
    context.canvas().line_to(x2, y2);
    context.canvas().line_to(x1, y2);
    context.canvas().fill();
  }
}

float SimpleMathShaper::italic_correction(
  Context          &context,
  uint16_t          ch,
  const GlyphInfo  &info
) {
  return 0;
}

void SimpleMathShaper::shape_radical(
  Context          &context,
  BoxSize          *box,        // in/out
  float            *radicand_x, // out
  Vector2F         *exponent_offset, // out
  RadicalShapeInfo *info        // out
) {
  if(get_style().italic) {
    math_set_style(get_style() - Italic)->shape_radical(
      context, box, radicand_x, exponent_offset, info);
      
    return;
  }
  
  context.canvas().set_font_face(font(_radical_font));
  const SmallRadicalGlyph *srg = small_radical_glyphs();
  
  if(box->ascent < 0)  box->ascent  = 0;
  if(box->descent < 0) box->descent = 0;
  
  float hbar_height = context.canvas().get_font_size() * 0.05;
  info->surd_form = 0;

  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = cg.y = 0;
  int i;
  for(i = 0; srg[i].index; ++i) {
    cg.index = srg[i].index;
    context.canvas().glyph_extents(&cg, 1, &cte);
    *radicand_x = cte.x_advance;
    exponent_offset->x = srg[i].rel_exp_offset.x * cte.width;
    exponent_offset->y = srg[i].rel_exp_offset.y * cte.height
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
  
  if(srg[i].index) {
    info->size = -1 - i;
    
    cg.index = srg[i].hbar_index;
    context.canvas().glyph_extents(&cg, 1, &cte);
    info->hbar = (unsigned)ceil(divide(cte.x_advance / 2 + box->width, cte.x_advance));
    box->width = *radicand_x + (info->hbar) * cte.x_advance;
    
    cg.index = srg[i].index;
    context.canvas().glyph_extents(&cg, 1, &cte);
    box->ascent = -cte.y_bearing - info->y_offset;
    if(box->descent < cte.height - box->ascent)
      box->descent = cte.height - box->ascent;
    exponent_offset->y += info->y_offset;
    
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
    exponent_offset);
    
  float h = box->height();
  cg.index = bottom;
  context.canvas().glyph_extents(&cg, 1, &cte);
  info->y_offset = box->descent - cte.height;
  *radicand_x = cte.x_advance;
  exponent_offset->x *= cte.x_advance;
  exponent_offset->y = exponent_offset->y * cte.height + info->y_offset;
  h -= cte.height;
  
  cg.index = edge;
  context.canvas().glyph_extents(&cg, 1, &cte);
  box->ascent += cte.height;
  h += cte.height;
  
  cg.index = vertical;
  context.canvas().glyph_extents(&cg, 1, &cte);
  info->size = (int)divide(h, cte.height);
  box->ascent -= h;
  box->ascent += (1 + info->size) * cte.height;
  
  cg.index = horizontal;
  context.canvas().glyph_extents(&cg, 1, &cte);
  info->hbar = (unsigned)ceil(divide(box->width, cte.x_advance));
  
  box->width = *radicand_x + (0.5 + info->hbar) * cte.x_advance;
}

void SimpleMathShaper::show_radical(
  Context                &context,
  const RadicalShapeInfo &info
) {
  if(get_style().italic) {
    math_set_style(get_style() - Italic)->show_radical(
      context, info);
      
    return;
  }
  
  float x, y;
  context.canvas().current_pos(&x, &y);
  context.canvas().set_font_face(font(_radical_font));
  
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  cg.x = x;
  cg.y = y + info.y_offset;
  
  if(info.size < 0) {
    const SmallRadicalGlyph &g = small_radical_glyphs()[-1 - info.size];
    cg.index = g.index;
    context.canvas().show_glyphs(&cg, 1);
    context.canvas().glyph_extents(&cg, 1, &cte);
    
    cg.y += cte.y_bearing;
    cg.x += cte.x_advance;
    cg.index = g.hbar_index;
    context.canvas().glyph_extents(&cg, 1, &cte);
    cg.y -= cte.y_bearing;
    for(int i = 0; i < info.hbar; ++i) {
      context.canvas().show_glyphs(&cg, 1);
      cg.x += cte.x_advance;
    }
  }
  else {
    Vector2F dummyxy;
    uint16_t bottom;
    uint16_t vertical;
    uint16_t edge;
    uint16_t horizontal;
    big_radical_glyphs(
      &bottom,
      &vertical,
      &edge,
      &horizontal,
      &dummyxy);
      
    cg.index = bottom;
    context.canvas().glyph_extents(&cg, 1, &cte);
    cg.y -= cte.y_bearing;
    context.canvas().show_glyphs(&cg, 1);
    
    cg.y += cte.y_bearing;
    cg.index = vertical;
    context.canvas().glyph_extents(&cg, 1, &cte);
    cg.y -= cte.y_bearing;
    for(int i = 0; i < info.size; ++i) {
      cg.y -= cte.height;
      context.canvas().show_glyphs(&cg, 1);
    }
    
    cg.y += cte.y_bearing;
    cg.index = edge;
    context.canvas().glyph_extents(&cg, 1, &cte);
    cg.y -= cte.height + cte.y_bearing;
    context.canvas().show_glyphs(&cg, 1);
    
    cg.y += cte.y_bearing;
    cg.x += cte.x_advance;
    cg.index = horizontal;
    context.canvas().glyph_extents(&cg, 1, &cte);
    cg.y -= cte.y_bearing;
    for(unsigned i = info.hbar; i > 0; --i) {
      context.canvas().show_glyphs(&cg, 1);
      cg.x += cte.x_advance;
    }
  }

  // Now, cg is horizontal extender glyph with extends in cte
  if(info.surd_form) {
    double em = context.canvas().get_font_size();
    double hook_width = em * 0.05;
    double hook_height = em * 0.25;

    bool sot = context.canvas().show_only_text;
    context.canvas().show_only_text = false;
    context.canvas().pixrect(cg.x - hook_width, cg.y + cte.y_bearing, cg.x, cg.y + cte.y_bearing + hook_height, false);
    context.canvas().fill();
    context.canvas().show_only_text = sot;
  }
}

void SimpleMathShaper::get_script_size_multis(Array<float> *arr) {
  arr->length(1, 0.71f);
}

int SimpleMathShaper::v_stretch_pair_glyphs(
  uint16_t  ch,
  uint16_t *upper,
  uint16_t *lower
) {
  *upper = *lower = 0;
  return 0;
}

//} ... class SimpleMathShaper
