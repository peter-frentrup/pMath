#include <boxes/textsequence.h>

#include <cstdlib>

#include <boxes/mathsequence.h>
#include <graphics/context.h>

using namespace richmath;

// using U+FFFC OBJECT REPLACEMENT CHARACTER instead of PMATH_CHAR_BOX (U+FDD0), 
// Because U+FFFD is an invalid unicode character and Pango treats it as three
// invalid bytes => 3 glyphs instead of one.
static const uint16_t BoxChar = 0xFFFF;
static const char *Utf8BoxChar = "\xEF\xBF\xBC";
static const int   Utf8BoxCharLen = 3;

//{ class TextBuffer ...

TextBuffer::TextBuffer(char *buf, int len)
: Base(),
  _capacity(len),
  _length(len),
  _buffer(buf)
{
  if(len < 0){
    _capacity = _length = strlen(buf);
  }
}

TextBuffer::~TextBuffer(){
  pmath_mem_free(_buffer);
}

int TextBuffer::insert(int pos, const char *ins, int inslen){
  if(inslen < 0)
    inslen = strlen(ins);
  
  if(_length + inslen > _capacity){
    _capacity = Array<char>::best_capacity(_length + inslen);
    _buffer = (char*)pmath_mem_realloc(_buffer, _capacity);
    
    if(!_buffer){
      assert("not enough memory" && 0);
      _length = 0;
      _capacity = 0;
      return 0;
    }
  }
  
  memmove(_buffer + pos + inslen, _buffer + pos, _length - pos);
  memcpy(_buffer + pos, ins, inslen);
  _length+= inslen;
  return inslen;
}

int TextBuffer::insert(int pos, const String &s){
  int tmplen;
  char *tmp = pmath_string_to_utf8(s.get_as_string(), &tmplen);
  
  // remove inner zeros:
  for(int i = 0;i < tmplen;++i){
    if(!tmp[i])
      tmp[i] = 1;
  }
  
  tmplen = insert(pos, tmp, tmplen);
  
  pmath_mem_free(tmp);
  return tmplen;
}

void TextBuffer::remove(int pos, int len){
  memmove(_buffer + pos, _buffer + pos + len, _length - pos - len);
  _length-= len;
}

bool TextBuffer::is_box_at(int i){
  return 0 <= i 
    && i + Utf8BoxCharLen <= _length 
    && memcmp(_buffer + i, Utf8BoxChar, Utf8BoxCharLen) == 0;
}

//} ... class TextBuffer

class GlobalPangoContext{
  public:
    static PangoContext *get_context(){
      if(!singleton.context){
        PangoCairoFontMap *fontmap = (PangoCairoFontMap*)pango_cairo_font_map_get_default();
      
        singleton.context = pango_cairo_font_map_create_context(fontmap);
      }
      
      return singleton.context;
    }
    
    static void update(Context *ctx){
      get_context();
      
      pango_cairo_update_context(ctx->canvas->cairo(), singleton.context);
      
      pango_cairo_context_set_shape_renderer(
        singleton.context,
        box_shape_renderer,
        ctx,
        0);
      
      PangoFontDescription *desc = pango_font_description_new();
      String name     = ctx->text_shaper->font_name(0);
      FontStyle style = ctx->text_shaper->get_style();
      
      int num_fonts = ctx->text_shaper->num_fonts();
      for(int i = 1;i < num_fonts;++i){
        String fn = ctx->text_shaper->font_name(i);
        
        name+= ",";
        name+= fn;
      }
      
      char *utf8_name = pmath_string_to_utf8(name.get_as_string(), NULL);
      if(utf8_name)
        pango_font_description_set_family_static(desc, utf8_name);
      
      pango_font_description_set_absolute_size(desc, ctx->canvas->get_font_size() * PANGO_SCALE);
      pango_font_description_set_style( desc, style.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
      pango_font_description_set_weight(desc, style.bold   ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
      
      pango_context_set_font_description(singleton.context, desc);
      
      pango_font_description_free(desc);
      pmath_mem_free(utf8_name);
    };
    
    static void box_shape_renderer(cairo_t *cr, PangoAttrShape *shape, gboolean do_path, void *data){
      Context *ctx = (Context*)data;
      Box     *box = (Box*)shape->data;
      
      assert(ctx != 0);
      assert(box != 0);
      assert(ctx->canvas->cairo() == cr);
      
      ctx->canvas->rel_move_to(pango_units_to_double(shape->ink_rect.x), 0);
      box->paint(ctx);
    }
    
  private:
    GlobalPangoContext(){
      // lazy initialization, because main() wants to add some fonts to the 
      // system.
      context = 0;
    }
    
    ~GlobalPangoContext(){
      if(context)
        g_object_unref(context);
    }
    
  private:
    PangoContext *context;
    
  public:
    static GlobalPangoContext singleton;
};

GlobalPangoContext GlobalPangoContext::singleton;

//{ class TextSequence ...

TextSequence::TextSequence()
: AbstractSequence(),
  text(0,0),
  _layout(pango_layout_new(GlobalPangoContext::get_context()))
{
  pango_layout_set_spacing(_layout, pango_units_from_double(1.5));
  pango_layout_set_wrap(   _layout, PANGO_WRAP_WORD_CHAR);
}

TextSequence::~TextSequence(){
  g_object_unref(_layout);
  for(int i = 0;i < boxes.length();++i)
    delete boxes[i];
}

void TextSequence::resize(Context *context){
  em = context->canvas->get_font_size();
  
  PangoTabArray *tabs = pango_tab_array_new(1, FALSE);
  pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, pango_units_from_double(72/2.54)); // 1cm
  pango_layout_set_tabs(_layout, tabs);
  pango_tab_array_free(tabs);
  
  ensure_boxes_valid();
  for(int i = 0;i < boxes.length();++i)
    boxes[i]->resize(context);
  
  text_invalid = true;
  ensure_text_valid();
  
  GlobalPangoContext::update(context);
  pango_layout_context_changed(_layout);
  
  if(context->width < Infinity){
    int w = pango_units_from_double(context->width);
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
  do{
    PangoRectangle ink, logical;
    pango_layout_iter_get_line_extents(iter, &ink, &logical);
    
    int base = pango_layout_iter_get_baseline(iter);
    
    if(base - ink.y < min_ascent){
      ink.height+= min_ascent - (base - ink.y);
      ink.y = base - min_ascent;
    }
    
    if((ink.y + ink.height) - base < min_descent){
      ink.height+= min_descent - ((ink.y + ink.height) - base);
    }
    
    line_y_corrections[i] = corr + ink.y - logical.y;
    corr+= logical.height - ink.height - spacing;
    
    ++i;
  }while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  PangoRectangle rect;
  pango_layout_get_extents(_layout, 0, &rect);
  
  _extents.width   = pango_units_to_double(rect.width);
  _extents.ascent  = pango_units_to_double(pango_layout_get_baseline(_layout) - line_y_corrections[0]);
  _extents.descent = pango_units_to_double(rect.height - corr) - _extents.ascent;
}

void TextSequence::paint(Context *context){
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  ensure_text_valid();
  
  GlobalPangoContext::update(context);
  
  y0-= _extents.ascent;
  double clip_x1, clip_y1, clip_x2, clip_y2;
  cairo_clip_extents(
    context->canvas->cairo(),
    &clip_x1, &clip_y1, 
    &clip_x2,  &clip_y2);
  
  int cl_y1 = pango_units_from_double(clip_y1 - y0);
  int cl_y2 = pango_units_from_double(clip_y2 - y0);
  
  int line = 0;
  PangoLayoutIter *iter = get_iter();
  do{
    PangoRectangle rect;
    pango_layout_iter_get_line_extents(iter, &rect, 0);
    
    rect.y-= line_y_corrections[line];
    
    if(rect.y >= cl_y2)
      break;
    
    if(rect.y + rect.height >= cl_y1){
      int base = pango_layout_iter_get_baseline(iter) - line_y_corrections[line];
      
      context->canvas->move_to(
        x0,
        y0 + pango_units_to_double(base));
      
      pango_cairo_show_layout_line(
        context->canvas->cairo(), 
        pango_layout_iter_get_line_readonly(iter));
    }
    
    ++line;
  }while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  if(context->selection.get() == this && !context->canvas->show_only_text){
    context->canvas->move_to(x0, y0 + _extents.ascent);
    
    selection_path(
      context->canvas, 
      context->selection.start, 
      context->selection.end);
    
    context->draw_selection_path();
  }
}

void TextSequence::selection_path(Canvas *canvas, int start, int end){
  float x0, y0;
  canvas->current_pos(&x0, &y0);
  
  ensure_text_valid();
  
  if(start == end){
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
    
    canvas->align_point(&x1, &y1, true);
    canvas->align_point(&x2, &y2, true);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x2, y2);
  }
  else{
    float last_bottom = 0;
    float spacing = pango_units_to_double(pango_layout_get_spacing(_layout));
    
    int line_index = 0;
    PangoLayoutIter *iter = get_iter();
    do{
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
      
      if(end <= line->start_index)
        break;
      
      if(start < line->start_index + line->length){
        int *xranges;
        int num_xranges;
        float x, y;
        BoxSize size;
        
        line_extents(iter, line_index, &x, &y, &size);
        if(end > line->start_index + line->length)
          size.descent+= spacing;
        
        if(start < line->start_index){
          size.ascent+=  spacing;
          canvas->pixrect(
            x0 + x,
            last_bottom,
            x0 + _extents.width,
            y0 + y - size.ascent,
            false);
        }
        
        last_bottom = y0 + y + size.descent;
        
        pango_layout_line_get_x_ranges(line, start, end, &xranges, &num_xranges);
        
        for(int i = 0;i < num_xranges;++i){
          canvas->pixrect(
            x0 + pango_units_to_double(xranges[2*i]),
            y0 + y - size.ascent,
            x0 + pango_units_to_double(xranges[2*i+1]),
            last_bottom,
            false);
        }
        
        g_free(xranges);
      }
      
      ++line_index;
    }while(pango_layout_iter_next_line(iter));
    
    pango_layout_iter_free(iter);
  }
}

Expr TextSequence::to_pmath(bool parseable){
  return to_pmath(parseable, 0, text.length());
}

Expr TextSequence::to_pmath(bool parseable, int start, int end){
  if(end <= start || start < 0 || end > text.length())
    return String("");
  
  int boxi = 0;
  while(boxi < boxes.length() && boxes[boxi]->index() < start)
    ++boxi;
  
  if(boxi >= boxes.length() || boxes[boxi]->index() >= end){
    return String::FromUtf8(text.buffer() + start, end - start);
  }
  
  Gather g;
  
  int next = boxes[boxi]->index();
  while(next < end){
    if(start < next)
      g.emit(String::FromUtf8(text.buffer() + start, next - start));
      
    g.emit(boxes[boxi]->to_pmath(parseable));
    
    start = next + Utf8BoxCharLen;
    ++boxi;
    if(boxi >= boxes.length())
      break;
    next = boxes[boxi]->index();
  }
  
  if(start < end)
    g.emit(String::FromUtf8(text.buffer() + start, end - start));
  
  return g.end();
}

void TextSequence::load_from_object(Expr object, int options){ // BoxOptionXXX
  for(int i = 0;i < boxes.length();++i)
    delete boxes[i];
  
  boxes.length(0);
  text.remove(0, text.length());
  
  boxes_invalid = true;
  text_invalid = true;
  
  if(object.instance_of(PMATH_TYPE_STRING))
    object = expand_string_boxes(String(object));
  
  if(object.instance_of(PMATH_TYPE_STRING)){
    String s(object.release());
    
    // skip BoxChar ...
    const uint16_t *buf = s.buffer();
    int             len = s.length();
    
    int start = 0;
    while(start < len){
      int next = start;
      while(next < len && buf[next] != BoxChar)
        ++next;
      
      text.insert(text.length(), s.part(start, next-start));
      text_invalid = true;
      
      start = next + 1;
    }
    
    return;
  }
  else if(object[0] == PMATH_SYMBOL_LIST){
    for(size_t i = 1;i <= object.expr_length();++i){
      Expr item = object[i];
      
      if(item.instance_of(PMATH_TYPE_STRING)){
        String s(item.release());
        
        // skip BoxChar ...
        const uint16_t *buf = s.buffer();
        int             len = s.length();
        
        int start = 0;
        while(start < len){
          int next = start;
          while(next < len && buf[next] != BoxChar)
            ++next;
          
          text.insert(text.length(), s.part(start, next-start));
          text_invalid = true;
          
          start = next + 1;
        }
      }
      else{
        MathSequence *box = new MathSequence(); // TODO: use special container?
        
        box->load_from_object(item, options);
        
        insert(text.length(), box);
      }
    }
  }
  else{
    MathSequence *box = new MathSequence(); // TODO: use special container?
    
    box->load_from_object(object, options);
    
    insert(text.length(), box);
  }
}

void TextSequence::ensure_boxes_valid(){
  if(!boxes_invalid)
    return;
  
  boxes_invalid = false;
  if(boxes.length() == 0)
    return;
  
  int box = 0;
  for(int i = 0;i <= text.length() - Utf8BoxCharLen;++i){
    if(text.is_box_at(i)){
      adopt(boxes[box++], i);
      
      if(box == boxes.length())
        return;
    }
  }
}

void TextSequence::ensure_text_valid(){
  ensure_boxes_valid();
  
  if(!text_invalid)
    return;
  
  text_invalid = false;
  pango_layout_set_text(_layout, text.buffer(), text.length());
  
  PangoRectangle rect = {0,0,0,0};
  {
    PangoAttrList *attrs;
    attrs = pango_attr_list_new();
    
    for(int i = 0;i < boxes.length();++i){
      rect.y      = pango_units_from_double(-boxes[i]->extents().ascent);
      rect.width  = pango_units_from_double( boxes[i]->extents().width);
      rect.height = pango_units_from_double( boxes[i]->extents().height());
      
      PangoAttribute *shape = pango_attr_shape_new_with_data(
        &rect, &rect, boxes[i], 0, 0);
      
      shape->start_index = boxes[i]->index();
      shape->end_index   = shape->start_index + Utf8BoxCharLen;
      pango_attr_list_insert(attrs, shape);
    }
    
    pango_layout_set_attributes(_layout, attrs);
    pango_attr_list_unref(attrs);
  }
}

int TextSequence::insert(int pos, const char *utf8, int len){
  pos+= text.insert(pos, utf8, len);
  boxes_invalid = true;
  text_invalid = true;
  invalidate();
  return pos;
}

int TextSequence::insert(int pos, const String &s){
  pos+= text.insert(pos, s);
  boxes_invalid = true;
  text_invalid = true;
  invalidate();
  return pos;
}

int TextSequence::insert(int pos, Box *box){
  if(pos > text.length())
    pos = text.length();
    
  if(TextSequence *txt = dynamic_cast<TextSequence*>(box)){
    pos = insert(pos, txt, 0, txt->length());
    delete txt;
    return pos;
  }
  
  ensure_boxes_valid();
  
  boxes_invalid = true;
  text_invalid = true;
  
  int result = pos + text.insert(pos, Utf8BoxChar, Utf8BoxCharLen);
  adopt(box, pos);
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < pos)
    ++i;
  boxes.insert(i, 1, &box);
  invalidate();
  return result;
}

int TextSequence::insert(int pos, TextSequence *txt, int start, int end){
  if(pos > text.length())
    pos = text.length();
  
  txt->ensure_boxes_valid();
  
  const char *buf = txt->text.buffer();
  int box = -1;
  while(start < end){
    int next = start;
    while(next < end && !txt->text.is_box_at(next))
      ++next;
    
    pos = insert(pos, buf + start, next - start);
    
    if(next < end){
      if(box < 0){
        box = 0;
        while(box < txt->count() && txt->boxes[box]->index() < next)
          ++box;
      }
      
      pos = insert(pos, txt->extract_box(box++));
    }
      
    start = next + Utf8BoxCharLen;
  }
  
  return pos;
}

void TextSequence::remove(int start, int end){
  ensure_boxes_valid();
  
  normalize_selection(&start, &end);
  
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < start)
    ++i;
  
  int j = i;
  while(j < boxes.length() && boxes[j]->index() < end)
    delete boxes[j++];
  
  boxes_invalid = i < boxes.length();
  text_invalid = start < end;
  boxes.remove(i, j - i);
  text.remove(start, end - start);
  invalidate();
}

Box *TextSequence::remove(int *index){
  remove(*index, *index + 1);
  return this;
}

Box *TextSequence::extract_box(int boxindex){
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
){
  ensure_text_valid();
  
  if(direction == Forward){
    if(*index >= length()){
      if(_parent){
        *index = _index;
        return _parent->move_logical(Forward, true, index);
      }
      return this;
    }
    
    if(*index < 0){
      *index = 0;
      return this;
    }
    
    const char *s     = text.buffer() + *index;
    const char *s_end = text.buffer() + text.length();
    
    if(text.is_box_at(*index)){
      if(jumping){
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
      return boxes[b]->move_logical(Forward, true, index);
    }
    
    if(jumping){ // next word
      PangoLogAttr *log_attrs;
      int n_attrs;
      
      pango_layout_get_log_attrs(_layout, &log_attrs, &n_attrs);
      
      int pos = *index;
      ++pos;
      while(pos < n_attrs && !log_attrs[pos].is_word_end)
        ++pos;
      
      g_free(log_attrs);
      
      *index = pos;
      return this;
    }
    
    s = g_utf8_find_next_char(s, s_end);
    if(s)
      *index = (int)((size_t)s - (size_t)text.buffer());
    else
      *index = text.length();
    
    return this;
  }
  
  if(*index <= 0){
    if(_parent){
      *index = _index + 1;
      return _parent->move_logical(Backward, true, index);
    }
    return this;
  }
  
  if(*index > text.length()){
    *index = text.length();
    return this;
  }
  
  const char *s = text.buffer() + *index;
  s = g_utf8_prev_char(s);
  
  *index = (int)((size_t)s - (size_t)text.buffer());
  if(text.is_box_at(*index)){
    if(jumping)
      return this;
    
    int b = 0;
    while(boxes[b]->index() < *index)
      ++b;
    
    *index = boxes[b]->length() + 1;
    return boxes[b]->move_logical(Backward, true, index);
  }
  
  if(jumping){ // prev. word
    PangoLogAttr *log_attrs;
    int n_attrs;
    
    pango_layout_get_log_attrs(_layout, &log_attrs, &n_attrs);
    
    int pos = *index;
    while(pos > 0 && !log_attrs[pos].is_word_start)
      --pos;
    
    g_free(log_attrs);
    
    *index = pos;
    return this;
  }
  
  return this;
}

Box *TextSequence::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  ensure_text_valid();
  
  int numlines = pango_layout_get_line_count(_layout);
  int line;
  float x = *index_rel_x;
  
  if(*index < 0){
    if(direction == Forward)
      line = 0;
    else
      line = numlines-1;
  }
  else{
    int px;
    pango_layout_index_to_line_x(_layout, *index, 0, &line, &px);
    
    float lx;
    line_extents(line, &lx, 0, 0);
    
    x+= lx + pango_units_to_double(px);
    if(direction == Forward)
      ++line;
    else
      --line;
  }
  
  if(line >= 0 && line < numlines){
    float lx;
    line_extents(line, &lx, 0, 0);
    PangoLayoutLine *pll = pango_layout_get_line_readonly(_layout, line);
    
    int i, tr;
    pango_layout_line_x_to_index(pll, pango_units_from_double(x - lx), &i, &tr);
    
    if(text.is_box_at(i)){
      ensure_boxes_valid();
      
      int px;
      pango_layout_line_index_to_x(pll, i, 0, &px);
      
      int b = 0;
      while(boxes[b]->index() < i)
        ++b;
      
      *index_rel_x = x - lx - pango_units_to_double(px);
      *index = -1;
      return boxes[b]->move_vertical(direction, index_rel_x, index);
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
  
  if(_parent){
    *index_rel_x = x;
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index);
  }
  
  return this;
}

Box *TextSequence::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
){
  BoxSize line_size;
  float line_x, line_y;
  
  int line_index = 0;
  PangoLayoutIter *iter = get_iter();
  do{
    line_extents(iter, line_index, &line_x, &line_y, &line_size);
    
    if(y <= line_y + line_size.descent)
      break;
  
    ++line_index;
  }while(pango_layout_iter_next_line(iter));
  
  PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
  pango_layout_iter_free(iter);
  
  int i, tr;
  pango_layout_line_x_to_index(
    line, 
    pango_units_from_double(x - line_x),
    &i, &tr);
  
  if(text.is_box_at(i)){
    ensure_boxes_valid();
    
    int b = 0;
    while(boxes[b]->index() < i)
      ++b;
    
    int pango_x;
    pango_layout_line_index_to_x(line, i, 0, &pango_x);
    
    y-= line_y;
    x-= line_x + pango_units_to_double(pango_x);
    
    return boxes[b]->mouse_selection(x, y, start, end, was_inside_start);
  }
  
  int x_pos;
  pango_layout_line_index_to_x(line, i, tr > 0, &x_pos);
  *was_inside_start = (x >= 0 && (tr == 0 || pango_units_to_double(x_pos) < x));
  char *s     = text.buffer() + i;
  char *s_end = text.buffer() + text.length();
  
  while(tr-- > 0)
    s = g_utf8_find_next_char(s, s_end);
  
  if(s)
    *start = *end = (int)((size_t)s - (size_t)text.buffer());
  else
    *start = *end = text.length();
  
  return this;
}

void TextSequence::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  ensure_text_valid();
  
  int line, px;
  pango_layout_index_to_line_x(_layout, index, 0, &line, &px);
  
  float x, y;
  BoxSize size;
  line_extents(line, &x, &y, &size);
  
  x+= pango_units_to_double(px);
  cairo_matrix_translate(matrix, x, y);
}

Box *TextSequence::normalize_selection(int *start, int *end){
  while(*end < text.length()
  && (text.buffer()[*end] & 0xC0) == 0x80)
    ++*end;
  
  if(*start < text.length()){
    while(*start > 0
    && (text.buffer()[*start] & 0xC0) == 0x80)
      --*start;
  }
  
  return this;
}

PangoLayoutIter *TextSequence::get_iter(){
  ensure_text_valid();
  return pango_layout_get_iter(_layout);
}

void TextSequence::line_extents(PangoLayoutIter *iter, int line, float *x, float *y, BoxSize *size){
  PangoRectangle ink, logic;
  pango_layout_iter_get_line_extents(iter, &ink, &logic);
  int base = pango_layout_iter_get_baseline(iter);
  
  if(size){
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
  
  if(y)
    *y = pango_units_to_double(base - line_y_corrections[line]) - _extents.ascent;
}

void TextSequence::line_extents(int line, float *x, float *y, BoxSize *size){
  int i = line;
  PangoLayoutIter *iter = get_iter();
  while(i > 0 && pango_layout_iter_next_line(iter)){
    --i;
  }
  
  line_extents(iter, line, x, y, size);
  
  pango_layout_iter_free(iter);
}

//} ... class TextSequence
