#include <boxes/textsequence.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <cstdlib>
#include <algorithm>

#include <boxes/mathsequence.h>
#include <boxes/ownerbox.h>
#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
}}

extern pmath_symbol_t richmath_System_List;

// using U+FFFC OBJECT REPLACEMENT CHARACTER instead of PMATH_CHAR_BOX (U+FDD0),
// Because U+FDD0 is an invalid unicode character and Pango treats it as three
// invalid bytes => 3 glyphs instead of one.
static const uint16_t BoxChar = 0xFFFC;
static const char *Utf8BoxChar = "\xEF\xBF\xBC";
static const int   Utf8BoxCharLen = 3;
static const char *Utf8ReplacementChar = "\xEF\xBF\xBD";

static bool is_utf8_first_byte(char c) {
  return ((unsigned char)c & 0xC0) != 0x80;
}

//{ class TextBuffer ...

TextBuffer::TextBuffer(char *buf, int len)
  : Base(),
    _capacity(len),
    _length(len),
    _buffer(buf)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
  if(len < 0) {
    _capacity = _length = strlen(buf);
  }
}

TextBuffer::~TextBuffer() {
  pmath_mem_free(_buffer);
}

uint32_t TextBuffer::char_at(int pos) {
  if(pos < 0 || pos >= _length)
    return 0;
    
  static int8_t chr_lens[16] = {
    1, // 0000 = 0xxx
    1, // 0001 = 0xxx
    1, // 0010 = 0xxx
    1, // 0011 = 0xxx
    1, // 0100 = 0xxx
    1, // 0101 = 0xxx
    1, // 0110 = 0xxx
    1, // 0111 = 0xxx
    0, // 1000 = 10xx
    0, // 1001 = 10xx
    0, // 1010 = 10xx
    0, // 1011 = 10xx
    2, // 1100 = 110x
    2, // 1101 = 110x
    3, // 1110 = 1110...
    4  // 1111 = 11110xxx
  };
  
  int len = chr_lens[((uint8_t)_buffer[pos]) >> 4];
  if(len == 0 || pos + len > _length)
    return 0;
    
  switch(len) {
    case 1:
      return (unsigned char)_buffer[pos];
      
    case 2: {
        uint32_t c1 = (unsigned char)_buffer[pos];
        uint32_t c2 = (unsigned char)_buffer[pos + 1];
        
        c1 = c1 & 0x1Fu;
        c2 = c2 & 0x3Fu;
        
        return (c1 << 6) | c2;
      }
      
    case 3: {
        uint32_t c1 = (unsigned char)_buffer[pos];
        uint32_t c2 = (unsigned char)_buffer[pos + 1];
        uint32_t c3 = (unsigned char)_buffer[pos + 2];
        
        c1 = c1 & 0x0Fu;
        c2 = c2 & 0x3Fu;
        c3 = c3 & 0x3Fu;
        
        return (c1 << 12) | (c2 << 6) | c3;
      }
      
    case 4: {
        uint32_t c1 = (unsigned char)_buffer[pos];
        uint32_t c2 = (unsigned char)_buffer[pos + 1];
        uint32_t c3 = (unsigned char)_buffer[pos + 2];
        uint32_t c4 = (unsigned char)_buffer[pos + 3];
        
        c1 = c1 & 0x07u;
        c2 = c2 & 0x3Fu;
        c3 = c3 & 0x3Fu;
        c4 = c4 & 0x3Fu;
        
        return (c1 << 18) | (c2 << 12) | (c3 << 6) | c4;
      }
  }
  
  return 0;
}

int TextBuffer::insert(int pos, const char *ins, int inslen) {
  if(inslen < 0)
    inslen = strlen(ins);
    
  if(_length + inslen > _capacity) {
    _capacity = Array<char>::best_capacity(_length + inslen);
    _buffer = (char *)pmath_mem_realloc(_buffer, _capacity);
    
    if(!_buffer) {
      assert("not enough memory" && 0);
      _length = 0;
      _capacity = 0;
      return 0;
    }
  }
  
  memmove(_buffer + pos + inslen, _buffer + pos, _length - pos);
  memcpy(_buffer + pos, ins, inslen);
  _length += inslen;
  return pos + inslen;
}

int TextBuffer::insert(int pos, const String &s) {
  int tmplen;
  char *tmp = pmath_string_to_utf8(s.get_as_string(), &tmplen);
  
  // remove inner zeros:
  for(int i = 0; i < tmplen; ++i) {
    if(!tmp[i])
      tmp[i] = 1;
  }
  
  pos = insert(pos, tmp, tmplen);
  
  pmath_mem_free(tmp);
  return pos;
}

void TextBuffer::remove(int pos, int len) {
  assert(len >= 0);
  assert(pos + len <= _length);
  memmove(_buffer + pos, _buffer + pos + len, _length - pos - len);
  _length -= len;
}

bool TextBuffer::is_box_at(int i) const {
  return 0 <= i
         && i + Utf8BoxCharLen <= _length
         && memcmp(_buffer + i, Utf8BoxChar, Utf8BoxCharLen) == 0;
}

bool TextBuffer::is_box_at(int start, int end) const {
  if(end != start + Utf8BoxCharLen)
    return false;
  
  return is_box_at(start);
}

//} ... class TextBuffer

static void update_pango_context(Context *ctx) {
}

class PangoContextUtil {
  public:
    static PangoContext *create_context() {
      PangoCairoFontMap *fontmap = (PangoCairoFontMap *)pango_cairo_font_map_get_default();
        
      return pango_cairo_font_map_create_context(fontmap);
    }
    
    static PangoLayout *create_layout() {
      PangoContext *context = create_context();
      PangoLayout *layout = pango_layout_new(context);
      g_object_unref(context);
      return layout;
    }
    
    static void update(PangoContext *pango, Context &ctx) {
      pango_cairo_update_context(ctx.canvas().cairo(), pango);
      
      pango_cairo_context_set_shape_renderer(
        pango,
        box_shape_renderer,
        &ctx,
        0);
        
      PangoFontDescription *desc = pango_font_description_new();
      String name     = ctx.text_shaper->font_name(0);
      FontStyle style = ctx.text_shaper->get_style();
      
      int num_fonts = ctx.text_shaper->num_fonts();
      for(int i = 1; i < num_fonts; ++i) {
        String fn = ctx.text_shaper->font_name(i);
        
        name += ",";
        name += fn;
      }
      
      char *utf8_name = pmath_string_to_utf8(name.get_as_string(), nullptr);
      if(utf8_name)
        pango_font_description_set_family_static(desc, utf8_name);
        
      pango_font_description_set_absolute_size(desc, ctx.canvas().get_font_size() * PANGO_SCALE);
      pango_font_description_set_style(        desc, style.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
      pango_font_description_set_weight(       desc, style.bold   ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
      
      pango_context_set_font_description(pango, desc);
      
      pango_font_description_free(desc);
      pmath_mem_free(utf8_name);
    };
    
    static void box_shape_renderer(cairo_t *cr, PangoAttrShape *shape, gboolean do_path, void *data) {
      Context &ctx = *(Context *)data;
      Box     *box = (Box *)shape->data;
      
      assert(box != nullptr);
      assert(ctx.canvas().cairo() == cr);
      
      ctx.canvas().rel_move_to(pango_units_to_double(shape->ink_rect.x), 0);
      box->paint(ctx);
    }
};

namespace richmath {
  class TextSequence::Impl {
      TextSequence &self;
    public:
      Impl(TextSequence &self) : self(self) {}
      
      Expr to_pmath(BoxOutputFlags flags, int start, int end);
      
      void append_object(Expr object, BoxInputFlags options);
      
    private:
      Expr add_debug_info(Expr expr, BoxOutputFlags flags, int start, int end);
  };
}

//{ class TextSequence ...

TextSequence::TextSequence()
  : base(),
    text(0, 0),
    _layout(PangoContextUtil::create_layout())
{
  pango_layout_set_spacing(_layout, pango_units_from_double(1.5));
  pango_layout_set_wrap(_layout, PANGO_WRAP_WORD_CHAR);
}

TextSequence::~TextSequence() {
  g_object_unref(_layout);
  for(auto box : boxes)
    delete_owned(box);
}

String TextSequence::raw_substring(int start, int length) {
  assert(start >= 0);
  assert(length >= 0);
  assert(start + length <= text.length());
  
  String result;
  const char *prev = text.buffer() + start;
  const char *next = prev;
  while(length > 0) {
    if(Utf8BoxCharLen <= length) {
      if(memcmp(next, Utf8BoxChar, Utf8BoxCharLen) == 0) {
        result+= String::FromUtf8(prev, next - prev);
        result+= PMATH_CHAR_BOX;
        next+= Utf8BoxCharLen;
        length-= Utf8BoxCharLen;
        prev = next;
        continue;
      }
    }
    ++next;
    --length;
  }
  
  result+= String::FromUtf8(prev, next - prev);
  return result;
}

bool TextSequence::is_placeholder(int i) {
  uint32_t ch = char_at(i);
  
  return ch == PMATH_CHAR_PLACEHOLDER || ch == PMATH_CHAR_SELECTIONPLACEHOLDER;
}

void TextSequence::resize(Context &context) {
  em = context.canvas().get_font_size();
  
  PangoTabArray *tabs = pango_tab_array_new(1, FALSE);
  pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, pango_units_from_double(72 / 2.54)); // 1cm
  pango_layout_set_tabs(_layout, tabs);
  pango_tab_array_free(tabs);
  
  ensure_boxes_valid();
  for(auto box : boxes)
    box->resize(context);
    
  text_invalid(true);
  ensure_text_valid();
  
  PangoContext *pango = pango_layout_get_context(_layout);
  PangoContextUtil::update(pango, context);
  pango_layout_context_changed(_layout);
  
  if(context.width < Infinity) {
    int w = pango_units_from_double(context.width);
    if(w < 0)
      w = 0;
    pango_layout_set_width(_layout, w);
  }
  else
    pango_layout_set_width(_layout, -1);
    
  /* We cannot use pango_layout_get_extents() for the height because Pango uses
     the logical rect instead of the ink rect for line height calculation.
     That is too big for math fonts like Cambria Math or Asana Math, which we
     also want to support as fallback fonts, so we must adjust every single line
     on our own.
   */
  line_y_corrections.length(pango_layout_get_line_count(_layout));
  int corr = 0;
  int spacing = pango_layout_get_spacing(_layout);
  int min_ascent  = pango_units_from_double(em * 0.75);
  int min_descent = pango_units_from_double(em * 0.25);
  
  PangoLayoutIter *iter = get_iter();
  int i = 0;
  do {
    PangoRectangle ink, logical;
    pango_layout_iter_get_line_extents(iter, &ink, &logical);
    
    int base = pango_layout_iter_get_baseline(iter);
    
    if(base - ink.y < min_ascent) {
      ink.height += min_ascent - (base - ink.y);
      ink.y = base - min_ascent;
    }
    
    if((ink.y + ink.height) - base < min_descent) {
      ink.height += min_descent - ((ink.y + ink.height) - base);
    }
    
    line_y_corrections[i] = corr + ink.y - logical.y;
    corr += logical.height - ink.height - spacing;
    
    ++i;
  } while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  PangoRectangle rect;
  pango_layout_get_extents(_layout, 0, &rect);
  
  _extents.width   = pango_units_to_double(rect.width);
  _extents.ascent  = pango_units_to_double(pango_layout_get_baseline(_layout) - line_y_corrections[0]);
  _extents.descent = pango_units_to_double(rect.height - corr) - _extents.ascent;
  
  context.sequence_unfilled_width = _extents.width;
}

void TextSequence::paint(Context &context) {
  float x0, y0;
  context.canvas().current_pos(&x0, &y0);
  ensure_text_valid();
  
  AutoCallPaintHooks auto_hooks(this, context);
  PangoContext *pango = pango_layout_get_context(_layout);
  PangoContextUtil::update(pango, context);
  
  y0 -= _extents.ascent;
  double clip_x1, clip_y1, clip_x2, clip_y2;
  context.canvas().clip_extents(&clip_x1, &clip_y1, &clip_x2,  &clip_y2);
    
  int cl_y1 = pango_units_from_double(clip_y1 - y0);
  int cl_y2 = pango_units_from_double(clip_y2 - y0);
  
  int line = 0;
  PangoLayoutIter *iter = get_iter();
  do {
    if(line >= line_y_corrections.length()) {
      /* It might happen that pango_layout_iter_next_line() gives more lines than during resize().
         This could happen if the current font/text_shaper was disposed since the last resize()
         and its set_style() thus does not yield the same fonts any more.

         For example, this scenario happened while the current math shaper was "Matematica 10 Sans" 
         (a ConfigShaper) and FrontEnd`AddConfigShaper("...path\to\mathematica10_sans.pmath") was called:
         The call to FrontEnd`AddConfigShaper disposed the old ConfigShaperDB, but some of the shapers 
         where still in use by the document. 
      */
      break;
    }

    PangoRectangle rect;
    pango_layout_iter_get_line_extents(iter, &rect, 0);
    
    rect.y -= line_y_corrections[line];
    
    if(rect.y >= cl_y2)
      break;
      
    if(rect.y + rect.height >= cl_y1) {
      int base = pango_layout_iter_get_baseline(iter) - line_y_corrections[line];
      
      context.canvas().move_to(
        x0,
        y0 + pango_units_to_double(base));
        
      pango_cairo_show_layout_line(
        context.canvas().cairo(),
        pango_layout_iter_get_line_readonly(iter));
    }
    
    ++line;
  } while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  if(context.selection.get() == this && !context.canvas().show_only_text) {
    context.canvas().move_to(x0, y0 + _extents.ascent);
    
    selection_path(
      context.canvas(),
      context.selection.start,
      context.selection.end);
      
    context.draw_selection_path();
  }
}

void TextSequence::selection_path(Canvas &canvas, int start, int end) {
  float x0, y0;
  canvas.current_pos(&x0, &y0);
  
  ensure_text_valid();
  
  if(start == end) {
    int line;
    int x_pos;
    pango_layout_index_to_line_x(_layout, start, 0, &line, &x_pos);
    
    BoxSize size;
    float x, y;
    line_extents(line, &x, &y, &size);
    
    float x1 = x0 + x + pango_units_to_double(x_pos);
    float y1 = y0 + y - size.ascent - 0.75;
    
    float x2 = x1;
    float y2 = y0 + y + size.descent + 0.75;
    
    canvas.align_point(&x1, &y1, true);
    canvas.align_point(&x2, &y2, true);
    
    canvas.move_to(x1, y1);
    canvas.line_to(x2, y2);
  }
  else {
    float last_bottom = 0;
    float spacing = pango_units_to_double(pango_layout_get_spacing(_layout));
    
    int line_index = 0;
    PangoLayoutIter *iter = get_iter();
    
    // adjust start...
    PangoLayoutLine *prev = nullptr;
    do {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
      
      if(start < line->start_index) {
        if(prev && prev->start_index + prev->length < start)
          start = prev->start_index + prev->length;
          
        break;
      }
      
      prev = line;
    } while(pango_layout_iter_next_line(iter));
    pango_layout_iter_free(iter);
    prev = nullptr;
    
    iter = get_iter();
    do {
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
      
      if(end <= line->start_index)
        break;
        
      if(start <= line->start_index + line->length) {
        int *xranges;
        int num_xranges;
        float x, y;
        BoxSize size;
        
        line_extents(iter, line_index, &x, &y, &size);
        if(end > line->start_index + line->length)
          size.descent += spacing;
          
        if(start < line->start_index) {
          size.ascent +=  spacing;
          
          RectangleF rect(x0 + x, last_bottom, _extents.width - x, y0 + y - size.ascent - last_bottom);
          rect.normalize();
          rect.pixel_align(  canvas, false, 0);
          rect.add_rect_path(canvas, false);
          
          //canvas.pixrect(
          //  x0 + x,
          //  last_bottom,
          //  x0 + _extents.width,
          //  y0 + y - size.ascent,
          //  false);
        }
        
        last_bottom = y0 + y + size.descent;
        
        pango_layout_line_get_x_ranges(line, start, end, &xranges, &num_xranges);
        
        for(int i = 0; i < num_xranges; ++i) {
          RectangleF rect(
            Point(x0 + pango_units_to_double(xranges[2 * i]),
                  y0 + y - size.ascent),
            Point(x0 + pango_units_to_double(xranges[2 * i + 1]),
                  last_bottom));
                  
          rect.pixel_align(  canvas, false, 0);
          rect.add_rect_path(canvas, false);
          
          //canvas.pixrect(
          //  x0 + pango_units_to_double(xranges[2 * i]),
          //  y0 + y - size.ascent,
          //  x0 + pango_units_to_double(xranges[2 * i + 1]),
          //  last_bottom,
          //  false);
        }
        
        g_free(xranges);
      }
      
      ++line_index;
    } while(pango_layout_iter_next_line(iter));
    
    pango_layout_iter_free(iter);
  }
}

Expr TextSequence::to_pmath(BoxOutputFlags flags) {
  return to_pmath(flags, 0, text.length());
}

Expr TextSequence::to_pmath(BoxOutputFlags flags, int start, int end) {
  return Impl(*this).to_pmath(flags, start, end);
}

void TextSequence::load_from_object(Expr object, BoxInputFlags options) {
  for(auto box : boxes)
    box->safe_destroy();
    
  boxes.length(0);
  text.remove(0, text.length());
  
  boxes_invalid(true);
  text_invalid(true);
  
  if(object.is_string())
    object = expand_string_boxes(String(object));
    
  if(object[0] == richmath_System_List) {
    for(size_t i = 1; i <= object.expr_length(); ++i) 
      Impl(*this).append_object(object[i], options);
  }
  else 
    Impl(*this).append_object(object, options);
  
  finish_load_from_object(std::move(object));
}

void TextSequence::ensure_boxes_valid() {
  if(!boxes_invalid())
    return;
    
  boxes_invalid(false);
  if(boxes.length() == 0)
    return;
    
  int box = 0;
  for(int i = 0; i <= text.length() - Utf8BoxCharLen; ++i) {
    if(text.is_box_at(i)) {
      adopt(boxes[box++], i);
      
      if(box == boxes.length())
        return;
    }
  }
}

void TextSequence::ensure_text_valid() {
  ensure_boxes_valid();
  
  if(!text_invalid())
    return;
    
  text_invalid(false);
  pango_layout_set_text(_layout, text.buffer(), text.length());
  
  PangoRectangle rect = {0, 0, 0, 0};
  {
    PangoAttrList *attrs;
    attrs = pango_attr_list_new();
    
    for(auto box : boxes) {
      rect.y      = pango_units_from_double(-box->extents().ascent);
      rect.width  = pango_units_from_double(box->extents().width);
      rect.height = pango_units_from_double(box->extents().height());
      
      PangoAttribute *shape = pango_attr_shape_new_with_data(&rect, &rect, box, 0, 0);
                                
      shape->start_index = box->index();
      shape->end_index   = shape->start_index + Utf8BoxCharLen;
      pango_attr_list_insert(attrs, shape);
    }
    
    pango_layout_set_attributes(_layout, attrs);
    pango_attr_list_unref(attrs);
  }
}

int TextSequence::insert(int pos, const char *utf8, int len) {
  if(len < 0) {
    len = (int)strlen(utf8);
    if(len <= 0)
      return pos;
  }
  
  char *buf = text.buffer();
  // ensure that we do not cut a Utf8BoxChar (or any other utf8 character)
  while(pos < text.length() && !is_utf8_first_byte(buf[pos]))
    ++pos;
  
  int start_search = std::max(0, pos - Utf8BoxCharLen + 1);
  
  pos = text.insert(pos, utf8, len);
  buf = text.buffer();
  
  int end_search = std::min(pos + Utf8BoxCharLen - 1, text.length());
  
  // replace all Utf8BoxChar in the newly inserted section by Utf8ReplacementChar 
  while(start_search <= end_search - Utf8BoxCharLen) {
    if(0 == memcmp(buf + start_search, Utf8BoxChar, Utf8BoxCharLen)) {
      memcpy(buf + start_search, Utf8ReplacementChar, Utf8BoxCharLen);
      start_search+= 3;
    }
    else
      ++start_search;
  }
  
  boxes_invalid(true);
  text_invalid(true);
  invalidate();
  return pos;
}

int TextSequence::insert(int pos, const String &s) {
  char *buf = text.buffer();
  // ensure that we do not cut a Utf8BoxChar (or any other utf8 character)
  while(pos < text.length() && !is_utf8_first_byte(buf[pos]))
    ++pos;
  
  int start_search = std::max(0, pos - Utf8BoxCharLen + 1);
  
  pos = text.insert(pos, s);
  buf = text.buffer();
  
  int end_search = std::min(pos + Utf8BoxCharLen - 1, text.length());
  
  // replace all Utf8BoxChar in the newly inserted section by Utf8ReplacementChar 
  while(start_search <= end_search - Utf8BoxCharLen) {
    if(0 == memcmp(buf + start_search, Utf8BoxChar, Utf8BoxCharLen)) {
      memcpy(buf + start_search, Utf8ReplacementChar, Utf8BoxCharLen);
      start_search+= 3;
    }
    else
      ++start_search;
  }
  
  boxes_invalid(true);
  text_invalid(true);
  invalidate();
  return pos;
}

int TextSequence::insert(int pos, Box *box) {
  if(pos > text.length())
    pos = text.length();
    
  if(TextSequence *txt = dynamic_cast<TextSequence *>(box)) {
    pos = insert(pos, txt, 0, txt->length());
    txt->safe_destroy();
    return pos;
  }
  
  ensure_boxes_valid();
  
  boxes_invalid(true);
  text_invalid(true);
  
  int newpos = text.insert(pos, Utf8BoxChar, Utf8BoxCharLen);
  adopt(box, pos);
  int i = boxes.length();
  while(i > 0 && boxes[i - 1]->index() >= pos)
    --i;
  boxes.insert(i, 1, &box);
  invalidate();
  return newpos;
}

int TextSequence::insert(int pos, TextSequence *txt, int start, int end) {
  if(pos > text.length())
    pos = text.length();
    
  txt->ensure_boxes_valid();
  
  const char *buf = txt->text.buffer();
  while(start < end && !is_utf8_first_byte(buf[start]))
    ++start;
  
  if(start == end)
    return pos;
  
  int box = -1;
  while(start < end) {
    int next = start;
    while(next < end && !txt->text.is_box_at(next))
      ++next;
      
    pos = text.insert(pos, buf + start, next - start);
    
    if(next < end) {
      if(box < 0) {
        box = 0;
        while(box < txt->count() && txt->boxes[box]->index() < next)
          ++box;
      }
      
      pos = insert(pos, txt->extract_box(box++));
    }
    
    start = next + Utf8BoxCharLen;
  }
  
  boxes_invalid(true);
  text_invalid(true);
  invalidate();
  return pos;
}

int TextSequence::insert(int pos, AbstractSequence *seq, int start, int end) {
  if(auto ts = dynamic_cast<TextSequence *>(seq))
    return insert(pos, ts, start, end);
    
  return base::insert(pos, seq, start, end);
}

void TextSequence::remove(int start, int end) {
  ensure_boxes_valid();
  
  VolatileSelection sel = normalize_selection(start, end);
  assert(sel.box == this);
  start = sel.start;
  end   = sel.end;
  
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < start)
    ++i;
    
  int j = i;
  while(j < boxes.length() && boxes[j]->index() < end)
    boxes[j++]->safe_destroy();
    
  boxes_invalid(i < boxes.length());
  text_invalid(start < end);
  boxes.remove(i, j - i);
  text.remove(start, end - start);
  invalidate();
}

Box *TextSequence::remove(int *index) {
  remove(*index, *index + 1);
  return this;
}

Box *TextSequence::extract_box(int boxindex) {
  Box *box = boxes[boxindex];
  
  DummyBox *dummy = new DummyBox();
  adopt(dummy, box->index());
  boxes.set(boxindex, dummy);
  
  abandon(box);
  return box;
}

Box *TextSequence::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  ensure_text_valid();
  
  if(direction == LogicalDirection::Forward) {
    if(*index >= length()) {
      if(auto par = parent()) {
        if(!par->exitable())
          return this;
          
        *index = _index;
        return par->move_logical(LogicalDirection::Forward, true, index);
      }
      return this;
    }
    
    if(*index < 0) {
      *index = 0;
      return this;
    }
    
    const char *s     = text.buffer() + *index;
    const char *s_end = text.buffer() + text.length();
    
    if(text.is_box_at(*index)) {
      if(jumping) {
        s = g_utf8_find_next_char(s, s_end);
        if(s)
          *index = (int)((size_t)s - (size_t)text.buffer());
        else
          *index = text.length();
          
        return this;
      }
      
      int b = 0;
      while(boxes[b]->index() < *index)
        ++b;
        
      *index = -1;
      return boxes[b]->move_logical(LogicalDirection::Forward, true, index);
    }
    
    if(jumping) { // next word
      const PangoLogAttr *log_attrs;
      int n_attrs;
      
      log_attrs = pango_layout_get_log_attrs_readonly(_layout, &n_attrs);
      
      int c = g_utf8_pointer_to_offset(text.buffer(), s);
      ++c;
      while(c < n_attrs && (!log_attrs[c].is_word_boundary || !log_attrs[c].is_cursor_position))
        ++c;
      
      while(c < n_attrs && (log_attrs[c].is_white || !log_attrs[c].is_cursor_position))
        ++c;
      
      *index = (int)(g_utf8_offset_to_pointer(text.buffer(), c) - text.buffer());
      return this;
    }
    
    s = g_utf8_find_next_char(s, s_end);
    if(s)
      *index = (int)(s - text.buffer());
    else
      *index = text.length();
      
    return this;
  }
  
  if(*index <= 0) {
    if(auto par = parent()) {
      if(!par->exitable())
        return this;
      
      *index = _index + 1;
      return par->move_logical(LogicalDirection::Backward, true, index);
    }
    return this;
  }
  
  if(*index > text.length()) {
    *index = text.length();
    return this;
  }
  
  const char *s       = text.buffer() + *index;
  s = g_utf8_find_prev_char(text.buffer(), s);
  
  if(!s)
    s = text.buffer();
    
  *index = (int)(s - text.buffer());
  
  if(text.is_box_at(*index)) {
    if(jumping)
      return this;
      
    int b = 0;
    while(boxes[b]->index() < *index)
      ++b;
      
    *index = boxes[b]->length() + 1;
    return boxes[b]->move_logical(LogicalDirection::Backward, true, index);
  }
  
  if(jumping) { // prev. word
    const PangoLogAttr *log_attrs;
    int n_attrs;
    
    log_attrs = pango_layout_get_log_attrs_readonly(_layout, &n_attrs);
    
    int c = g_utf8_pointer_to_offset(text.buffer(), s);
    while(c > 0 && (log_attrs[c].is_white || !log_attrs[c].is_cursor_position))
      --c;
    while(c > 0 && (!log_attrs[c].is_word_boundary || !log_attrs[c].is_cursor_position))
      --c;
    
    *index = (int)(g_utf8_offset_to_pointer(text.buffer(), c) - text.buffer());
    return this;
  }
  
  return this;
}

Box *TextSequence::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  ensure_text_valid();
  
  int numlines = pango_layout_get_line_count(_layout);
  int line;
  float x = *index_rel_x;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward)
      line = 0;
    else
      line = numlines - 1;
  }
  else {
    int px;
    pango_layout_index_to_line_x(_layout, *index, 0, &line, &px);
    
    float lx;
    line_extents(line, &lx, 0, 0);
    
    x += lx + pango_units_to_double(px);
    if(direction == LogicalDirection::Forward)
      ++line;
    else
      --line;
  }
  
  if(line >= 0 && line < numlines) {
    float lx;
    line_extents(line, &lx, 0, 0);
    PangoLayoutLine *pll = pango_layout_get_line_readonly(_layout, line);
    
    int i, tr;
    pango_layout_line_x_to_index(pll, pango_units_from_double(x - lx), &i, &tr);
    
    if(text.is_box_at(i)) {
      ensure_boxes_valid();
      
      int px;
      pango_layout_line_index_to_x(pll, i, 0, &px);
      
      int b = 0;
      while(boxes[b]->index() < i)
        ++b;
        
      *index_rel_x = x - lx - pango_units_to_double(px);
      *index = -1;
      return boxes[b]->move_vertical(direction, index_rel_x, index, false);
    }
    
    int px;
    pango_layout_line_index_to_x(pll, i, tr > 0, &px);
    *index_rel_x = x - pango_units_to_double(px);
    
    char *s     = text.buffer() + i;
    char *s_end = text.buffer() + text.length();
    while(tr-- > 0)
      s = g_utf8_find_next_char(s, s_end);
      
    if(s)
      *index = (int)((size_t)s - (size_t)text.buffer());
    else
      *index = text.length();
      
    return this;
  }
  
  if(auto par = parent()) {
    *index_rel_x = x;
    *index = _index;
    return par->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

VolatileSelection TextSequence::mouse_selection(Point pos, bool *was_inside_start) {
  BoxSize line_size;
  float line_x, line_y;
  
  int line_index = 0;
  PangoLayoutIter *iter = get_iter();
  do {
    line_extents(iter, line_index, &line_x, &line_y, &line_size);
    
    if(pos.y <= line_y + line_size.descent)
      break;
      
    ++line_index;
  } while(pango_layout_iter_next_line(iter));
  
  PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
  pango_layout_iter_free(iter);
  
  int i, tr;
  pango_layout_line_x_to_index(
    line,
    pango_units_from_double(pos.x - line_x),
    &i, &tr);
    
  if(text.is_box_at(i)) {
    ensure_boxes_valid();
    
    int b = 0;
    while(boxes[b]->index() < i)
      ++b;
      
    int pango_x;
    pango_layout_line_index_to_x(line, i, 0, &pango_x);
    
    pos.y -= line_y;
    pos.x -= line_x + pango_units_to_double(pango_x);
    
    return boxes[b]->mouse_selection(pos, was_inside_start);
  }
  
  int x_pos;
  pango_layout_line_index_to_x(line, i, tr > 0, &x_pos);
  *was_inside_start = (0 <= pos.x && (tr == 0 || pango_units_to_double(x_pos) < pos.x));
  char *s     = text.buffer() + i;
  char *s_end = text.buffer() + text.length();
  
  while(tr-- > 0)
    s = g_utf8_find_next_char(s, s_end);
    
  if(s)
    return VolatileSelection(this, (int)((size_t)s - (size_t)text.buffer()));
  else
    return VolatileSelection(this, text.length());
}

void TextSequence::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  ensure_text_valid();
  
  int line, px;
  pango_layout_index_to_line_x(_layout, index, 0, &line, &px);
  
  float x, y;
  BoxSize size;
  line_extents(line, &x, &y, &size);
  
  x += pango_units_to_double(px);
  cairo_matrix_translate(matrix, x, y);
}

VolatileSelection TextSequence::normalize_selection(int start, int end) {
  while(end < text.length() && (text.buffer()[end] & 0xC0) == 0x80)
    ++end;
    
  if(start < text.length()) {
    while(start > 0 && (text.buffer()[start] & 0xC0) == 0x80)
      --start;
  }
  
  return {this, start, end};
}

PangoLayoutIter *TextSequence::get_iter() {
  ensure_text_valid();
  return pango_layout_get_iter(_layout);
}

int TextSequence::get_line(int index, int guide) {
  int line = guide, x;
  pango_layout_index_to_line_x(_layout, index, 0, &line, &x);
  return line;
}

void TextSequence::get_line_heights(int line, float *ascent, float *descent) {
  if(line < 0 || line >= line_y_corrections.length()) {
    *ascent  = 0;
    *descent = 0;
    return;
  }
  
  BoxSize size;
  float x, y;
  line_extents(line, &x, &y, &size);
  *ascent  = size.ascent;
  *descent = size.descent;
}

void TextSequence::line_extents(PangoLayoutIter *iter, int line, float *x, float *y, BoxSize *size) {
  PangoRectangle ink, logic;
  pango_layout_iter_get_line_extents(iter, &ink, &logic);
  int base = pango_layout_iter_get_baseline(iter);
  
  if(size) {
    size->width   = pango_units_to_double(logic.width);
    size->ascent  = pango_units_to_double(base - ink.y);
    size->descent = pango_units_to_double(ink.height) - size->ascent;
    
    if(size->ascent < 0.75 * em)
      size->ascent = 0.75 * em;
      
    if(size->descent < 0.25 * em)
      size->descent = 0.25 * em;
  }
  
  if(x)
    *x = pango_units_to_double(logic.x);
    
  if(line < line_y_corrections.length())
    base -= line_y_corrections[line];
    
  if(y)
    *y = pango_units_to_double(base) - _extents.ascent;
}

void TextSequence::line_extents(int line, float *x, float *y, BoxSize *size) {
  int i = line;
  PangoLayoutIter *iter = get_iter();
  while(i > 0 && pango_layout_iter_next_line(iter)) {
    --i;
  }
  
  line_extents(iter, line, x, y, size);
  
  pango_layout_iter_free(iter);
}

//} ... class TextSequence

//{ class TextSequence::Impl ...

Expr TextSequence::Impl::to_pmath(BoxOutputFlags flags, int start, int end) {
  if(end <= start || start < 0 || end > self.text.length())
    return strings::EmptyString;
    
  int boxi = 0;
  while(boxi < self.boxes.length() && self.boxes[boxi]->index() < start)
    ++boxi;
    
  if(boxi >= self.boxes.length() || self.boxes[boxi]->index() >= end) {
    return add_debug_info(
             String::FromUtf8(self.text.buffer() + start, end - start),
             flags,
             start,
             end);
  }
  
  Gather g;
  
  int next = self.boxes[boxi]->index();
  while(next < end) {
    if(start < next)
      g.emit(
        add_debug_info(
          String::FromUtf8(self.text.buffer() + start, next - start),
          flags,
          start,
          next));
      
    g.emit(
      add_debug_info(
        self.boxes[boxi]->to_pmath(flags),
        flags,
        next,
        next + Utf8BoxCharLen));
    
    start = next + Utf8BoxCharLen;
    ++boxi;
    if(boxi >= self.boxes.length())
      break;
    next = self.boxes[boxi]->index();
  }
  
  if(start < end)
    g.emit(
      add_debug_info(
        String::FromUtf8(self.text.buffer() + start, end - start),
        flags,
        start,
        end));
    
  return add_debug_info(g.end(), flags, start, end);
}

Expr TextSequence::Impl::add_debug_info(Expr expr, BoxOutputFlags flags, int start, int end) {
  if(!has(flags, BoxOutputFlags::WithDebugInfo))
    return expr;
  
  pmath_t obj = expr.release();
  obj = pmath_try_set_debug_info(
          obj, 
          SelectionReference(self.id(), start, end).to_debug_info().release());
  return Expr{ obj };
}

void TextSequence::Impl::append_object(Expr object, BoxInputFlags options) {
  if(object.is_string()) {
    String s(object.release());
    
    // skip BoxChar ...
    const uint16_t *buf = s.buffer();
    int             len = s.length();
    
    int start = 0;
    while(start < len) {
      int next = start;
      while(next < len && buf[next] != BoxChar)
        ++next;
        
      self.text.insert(self.text.length(), s.part(start, next - start));
      self.text_invalid(true);
      
      start = next + 1;
    }
    
    return;
  }
  
  InlineSequenceBox *box = new InlineSequenceBox();
  
  box->content()->load_from_object(object, options);
  
  self.insert(self.text.length(), box);
}

//} ... class TextSequence::Impl
