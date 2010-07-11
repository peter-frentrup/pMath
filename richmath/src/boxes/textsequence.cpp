#include <boxes/textsequence.h>

#include <cstdlib>

#include <graphics/context.h>

using namespace richmath;

static const char *Utf8BoxChar = "\xEF\xB7\x90";
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

int TextBuffer::insert(int pos, String s){
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
    GlobalPangoContext(){
      PangoCairoFontMap *fontmap = (PangoCairoFontMap*)pango_cairo_font_map_get_default();
      context = pango_cairo_font_map_create_context(fontmap);
    }
    
    ~GlobalPangoContext(){
      g_object_unref(context);
    }
    
    void update(Context *ctx){
      pango_cairo_update_context(ctx->canvas->cairo(), context);
      
      pango_cairo_context_set_shape_renderer(
        context,
        box_shape_renderer,
        ctx,
        0);
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
    
  public:
    PangoContext *context;
};

static GlobalPangoContext global_pango;

//{ class TextSequence ...

TextSequence::TextSequence()
: Box(),
  text(0,0),
  _layout(pango_layout_new(global_pango.context))
{
}

TextSequence::~TextSequence(){
  g_object_unref(_layout);
}

void TextSequence::resize(Context *context){
  ensure_boxes_valid();
  for(int i = 0;i < boxes.length();++i)
    boxes[i]->resize(context);
  
  ensure_text_valid();
  
  global_pango.update(context);
  pango_layout_context_changed(_layout);
  
  if(context->width < Infinity)
    pango_layout_set_width(_layout, pango_units_from_double(context->width));
  
  PangoRectangle rect;
  pango_layout_get_extents(_layout, 0, &rect);
  
  _extents.width   = pango_units_to_double(rect.width);
  _extents.ascent  = pango_units_to_double(pango_layout_get_baseline(_layout));
  _extents.descent = pango_units_to_double(rect.height) - _extents.ascent;
}

void TextSequence::paint(Context *context){
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  ensure_text_valid();
  
  global_pango.update(context);
  pango_layout_context_changed(_layout);
  
//  context->canvas->rel_move_to(0, -_extents.ascent);
//  pango_cairo_show_layout(context->canvas->cairo(), _layout);
  
  y0-= _extents.ascent;
  double clip_x1, clip_y1, clip_x2, clip_y2;
  cairo_clip_extents(
    context->canvas->cairo(),
    &clip_x1, &clip_y1, 
    &clip_x2,  &clip_y2);
  
  int cl_y1 = pango_units_from_double(clip_y1 - y0);
  int cl_y2 = pango_units_from_double(clip_y2 - y0);
  
  PangoLayoutIter *iter = get_iter();
  while(true){
    PangoRectangle rect;
    pango_layout_iter_get_line_extents(iter, 0, &rect);
    
    if(rect.y >= cl_y2)
      break;
    
    if(rect.y + rect.height >= cl_y1){
      int base = pango_layout_iter_get_baseline(iter);
      
      context->canvas->move_to(
        x0 + pango_units_to_double(rect.x),
        y0 + pango_units_to_double(base));
      
      pango_cairo_show_layout_line(
        context->canvas->cairo(), 
        pango_layout_iter_get_line_readonly(iter));
    }
    
    if(!pango_layout_iter_next_line(iter))
      break;
  }
  pango_layout_iter_free(iter);
  
  if(context->selection.get() == this && !context->canvas->show_only_text){
    context->canvas->move_to(x0, y0 + _extents.ascent);
    
    selection_path(
      context, 
      context->selection.start, 
      context->selection.end);
    
    context->draw_selection_path();
  }
}

void TextSequence::selection_path(Context *context, int start, int end){
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  y0-= _extents.ascent;
  
  ensure_text_valid();
  
  if(start == end){
    PangoRectangle rect;
    
    pango_layout_get_cursor_pos(_layout, start, &rect, 0);
    
    float x1 = x0 + pango_units_to_double(rect.x);
    float y1 = y0 + pango_units_to_double(rect.y);
    float x2 = x1;
    float y2 = y1 + pango_units_to_double(rect.height);
    
    context->canvas->align_point(&x1, &y1, true);
    context->canvas->align_point(&x2, &y2, true);
    
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y2);
  }
  else{
    PangoLayoutIter *iter = get_iter();
    
    while(true){
      PangoLayoutLine *line = pango_layout_iter_get_line_readonly(iter);
      
      if(end < line->start_index)
        break;
      
      if(start < line->start_index + line->length){
        int y1, y2;
        int *xranges;
        int num_xranges;
        
        pango_layout_iter_get_line_yrange(iter, &y1, &y2);
        pango_layout_line_get_x_ranges(line, start, end, &xranges, &num_xranges);
        
        for(int i = 0;i < num_xranges;++i){
          context->canvas->pixrect(
            x0 + pango_units_to_double(xranges[2*i]),
            y0 + pango_units_to_double(y1),
            x0 + pango_units_to_double(xranges[2*i+1]),
            y0 + pango_units_to_double(y2),
            false);
        }
        
        g_free(xranges);
      }
      
      if(!pango_layout_iter_next_line(iter))
        break;
    }
    
    pango_layout_iter_free(iter);
  }
}

void TextSequence::ensure_boxes_valid(){
  if(!boxes_invalid)
    return;
  
  boxes_invalid = false;
  if(boxes.length() == 0)
    return;
  
  int box = 0;
  for(int i = 0;i < text.length() - Utf8BoxCharLen;++i){
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
    
    pango_layout_set_attributes(_layout,attrs);
    pango_attr_list_unref(attrs);
  }
}

void TextSequence::insert(int pos, const char *utf8, int len){
  text.insert(pos, utf8, len);
  boxes_invalid = true;
  text_invalid = true;
  invalidate();
}

void TextSequence::insert(int pos, String s){
  text.insert(pos, s);
  boxes_invalid = true;
  text_invalid = true;
  invalidate();
}

void TextSequence::insert(int pos, Box *box){
  if(pos > text.length())
    pos = text.length();
    
  if(TextSequence *txt = dynamic_cast<TextSequence*>(box)){
    insert(pos, txt, 0, txt->length());
    delete txt;
    return;
  }
  
  ensure_boxes_valid();
  
  boxes_invalid = true;
  text.insert(pos, Utf8BoxChar, Utf8BoxCharLen);
  adopt(box, pos);
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < pos)
    ++i;
  boxes.insert(i, 1, &box);
  invalidate();
}

void TextSequence::insert(int pos, TextSequence *txt, int start, int end){
  if(pos > text.length())
    pos = text.length();
  
  txt->ensure_boxes_valid();
  
  const char *buf = txt->text.buffer();
  int box = -1;
  while(start < end){
    int next = start;
    while(next < end && !text.is_box_at(next))
      ++next;
    
    insert(pos, buf + start, next - start);
    pos+= next - start;
    
    if(next < end){
      if(box < 0){
        box = 0;
        while(box < txt->count() && txt->boxes[box]->index() < next)
          ++box;
      }
      
      insert(pos++, txt->extract_box(box++));
    }
      
    start = next + Utf8BoxCharLen;
  }
}

void TextSequence::remove(int start, int end){
  ensure_boxes_valid();
  
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
    
    const char *s = text.buffer() + *index;
    
    if(text.is_box_at(*index)){
      if(jumping){
        s = g_utf8_next_char(s);
        *index = g_utf8_pointer_to_offset(text.buffer(), s);
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
    
    s = g_utf8_next_char(s);
    *index = g_utf8_pointer_to_offset(text.buffer(), s);
    return this;
  }
  
  if(*index <= 0){
    if(_parent){
      *index = _index + 1;
      return _parent->move_logical(Backward, true, index);
    }
    return this;
  }
  
  const char *s = text.buffer() + *index;
  s = g_utf8_prev_char(s);
  
  *index = g_utf8_pointer_to_offset(text.buffer(), s);
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
      line = numlines;
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
    x-= lx;
    
    PangoLayoutLine *pll = pango_layout_get_line_readonly(_layout, line);
    
    int i, tr;
    pango_layout_line_x_to_index(pll, pango_units_from_double(x), &i, &tr);
    
    if(text.is_box_at(i)){
      ensure_boxes_valid();
      
      int px;
      pango_layout_line_index_to_x(pll, i, 0, &px);
      
      int b = 0;
      while(boxes[b]->index() < i)
        ++b;
      
      *index_rel_x = x - pango_units_to_double(px);
      *index = -1;
      return boxes[b]->move_vertical(direction, index_rel_x, index);
    }
    
    *index = i + tr;
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
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  ensure_text_valid();
  y+= _extents.ascent;
  
  int i, tr;
  *eol = !pango_layout_xy_to_index(
    _layout, 
    pango_units_from_double(x),
    pango_units_from_double(y),
    &i,
    &tr);
  
  if(text.is_box_at(i)){
    ensure_boxes_valid();
    
    int b = 0;
    while(boxes[b]->index() < i)
      ++b;
    
    PangoRectangle pos;
    pango_layout_index_to_pos(_layout, i, &pos);
    
    y-= pango_units_to_double(pos.y);
    y-= boxes[b]->extents().ascent;
    x-= pango_units_to_double(pos.x);
    if(pos.width < 0){
      x+= pango_units_to_double(pos.width);
    }
    
    return boxes[b]->mouse_selection(x, y, start, end, eol);
  }
  
  *start = *end = i + tr;
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
  
  while(*start > 0
  && (text.buffer()[*start] & 0xC0) == 0x80)
    --*start;
  
  return this;
}

PangoLayoutIter *TextSequence::get_iter(){
  ensure_text_valid();
  return pango_layout_get_iter(_layout);
}

void TextSequence::line_extents(PangoLayoutIter *iter, float *x, float *y, BoxSize *size){
  PangoRectangle rect = {0,0,0,0};
  pango_layout_iter_get_line_extents(iter, 0, &rect);
  int base = pango_layout_iter_get_baseline(iter);
  
  pango_layout_iter_free(iter);
  
  if(size){
    size->width   = pango_units_to_double(rect.width);
    size->ascent  = pango_units_to_double(base - rect.y);
    size->descent = pango_units_to_double(rect.height) - size->ascent;
  }
  
  if(x)
    *x = pango_units_to_double(rect.x);
  
  if(y)
    *y = pango_units_to_double(base) - _extents.ascent;
}

void TextSequence::line_extents(int line, float *x, float *y, BoxSize *size){
  PangoLayoutIter *iter = pango_layout_get_iter(_layout);
  while(line > 0 && pango_layout_iter_next_line(iter)){
    --line;
  }
  
  line_extents(iter, x, y, size);
  
  pango_layout_iter_free(iter);
}

//} ... class TextSequence
