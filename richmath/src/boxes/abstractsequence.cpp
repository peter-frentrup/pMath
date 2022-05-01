#include <boxes/abstractsequence.h>

#include <boxes/errorbox.h>
#include <boxes/ownerbox.h>


using namespace richmath;

//{ class AbstractSequence ...

AbstractSequence::AbstractSequence()
  : base(),
    str(""),
    em(0.0f)
{
}

AbstractSequence::~AbstractSequence() {
  for(int i = 0; i < boxes.length(); ++i)
    delete_owned(boxes[i]);
}

bool AbstractSequence::try_load_from_object(Expr object, BoxInputFlags options) {
  load_from_object(object, options);
  return true;
}

Box *AbstractSequence::item(int i) {
  ensure_boxes_valid();
  return boxes[i];
}

String AbstractSequence::raw_substring(int start, int length) {
  RICHMATH_ASSERT(start >= 0);
  RICHMATH_ASSERT(length >= 0);
  RICHMATH_ASSERT(start + length <= str.length());
  
  return str.part(start, length);
}

uint32_t AbstractSequence::char_at(int pos) {
  if(pos < 0 || pos > str.length())
    return 0;
    
  const uint16_t *buf = str.buffer();
  
  if(is_utf16_high(buf[pos]) && is_utf16_low((buf[pos + 1]))) {
    uint32_t hi = buf[pos];
    uint32_t lo = buf[pos + 1];
    
    return 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
  }
  
  return buf[pos];
}

void AbstractSequence::ensure_boxes_valid() {
  if(!boxes_invalid())
    return;
    
  boxes_invalid(false);
  const uint16_t *buf = str.buffer();
  int len = str.length();
  int box = 0;
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_BOX)
      adopt(boxes[box++], i);
}

VolatileSelection AbstractSequence::normalize_selection(int start, int end) {
  if(start <= 0)
    start = 0;
  
  if(end >= str.length())
    end = str.length();
  else if(is_utf16_low(str[end])) {
    ++end;
    if(start > 0 && is_utf16_high(str[start - 1]))
      --start;
  }

  return {this, start, end};
}

bool AbstractSequence::is_placeholder() {
  return str.length() == 1 && is_placeholder(0);
}

bool AbstractSequence::is_placeholder(int i) {
  if(i < 0 || i >= str.length())
    return false;
    
  if(str[i] == PMATH_CHAR_PLACEHOLDER || str[i] == PMATH_CHAR_SELECTIONPLACEHOLDER)
    return true;
    
  if(str[i] == PMATH_CHAR_BOX) {
    ensure_boxes_valid();
    int b = 0;
    
    while(boxes[b]->index() < i)
      ++b;
      
    return boxes[b]->get_own_style(Placeholder);
  }
  
  return false;
}

//{ insert/remove ...

int AbstractSequence::insert(int pos, uint32_t chr) {
  if(chr == PMATH_CHAR_BOX) 
    return insert(pos, new ErrorBox(String::FromChar(chr)));
  
  text_changed(true);
  boxes_invalid(true);
  if(chr <= 0xFFFF) {
    uint16_t u16 = (uint16_t)chr;
    str.insert(pos, &u16, 1);
    invalidate();
    return pos + 1;
  }
  else {
    uint16_t u16[2];
    chr -= 0x10000;
    u16[0] = 0xD800 | (chr >> 10);
    u16[1] = 0xDC00 | (chr & 0x3FF);
    str.insert(pos, u16, 2);
    invalidate();
    return pos + 2;
  }
}

int AbstractSequence::insert(int pos, const uint16_t *ucs2, int len) {
  if(len < 0) {
    len = 0;
    const uint16_t *buf = ucs2;
    while(*buf++)
      ++len;
  }
  
  int boxpos = 0;
  while(boxpos < len && ucs2[boxpos] != PMATH_CHAR_BOX)
    ++boxpos;
  
  if(boxpos < len) {
    ensure_boxes_valid();
    
    int b = 0;
    while(b < boxes.length() && boxes[b]->index() < pos)
      ++b;
    
    while(boxpos < len) {
      ++boxpos;
      str.insert(pos, ucs2, boxpos);
      
      pos+= boxpos;
      ucs2+= boxpos;
      len-= boxpos;
      
      Box *box = new ErrorBox(String::FromChar(PMATH_CHAR_BOX));
      adopt(box, pos - 1);
      boxes.insert(b, 1, &box);
      ++b;
      
      boxpos = 0;
      while(boxpos < len && ucs2[boxpos] != PMATH_CHAR_BOX)
        ++boxpos;
    }
  }
  str.insert(pos, ucs2, len);
  
  text_changed(true);
  boxes_invalid(true);
  invalidate();
  return pos + len;
}

int AbstractSequence::insert(int pos, const char *latin1, int len) {
  if(len < 0)
    len = strlen(latin1);
  
  text_changed(true);
  boxes_invalid(true);
  str.insert(pos, latin1, len);
  invalidate();
  return pos + len;
}

int AbstractSequence::insert(int pos, const String &s) {
  return insert(pos, s.buffer(), s.length());
}

int AbstractSequence::insert(int pos, Box *box) {
  if(pos > length())
    pos = length();
    
  // TODO: check whether box is actually the same type as this.
  // Or alternatively introduce real inline Sections (Cells)
  if(AbstractSequence *sequence = dynamic_cast<AbstractSequence *>(box)) {
    if(kind() == sequence->kind()) {
      pos = insert(pos, sequence, 0, sequence->length());
      sequence->safe_destroy();
      return pos;
    }
    else
      box = new InlineSequenceBox(sequence);
  }
  
  ensure_boxes_valid();
  
  text_changed(true);
  boxes_invalid(true);
  uint16_t ch = PMATH_CHAR_BOX;
  str.insert(pos, &ch, 1);
  adopt(box, pos);
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < pos)
    ++i;
  boxes.insert(i, 1, &box);
  invalidate();
  return pos + 1;
}

int AbstractSequence::insert(int pos, AbstractSequence *seq, int start, int end) {
  if(pos > length())
    pos = length();
    
  seq->ensure_boxes_valid();
  
  int box = 0;
  while(box < seq->count() && seq->item(box)->index() < start)
    ++box;
    
  String s = seq->raw_substring(start, end - start);
  const uint16_t *buf = s.buffer();
  start = 0;
  end   = s.length();
  
  while(start < end) {
    int next = start;
    while(next < end && buf[next] != PMATH_CHAR_BOX)
      ++next;
      
    pos = insert(pos, s.part(start, next - start));
    
    if(next < end)
      pos = insert(pos, seq->extract_box(box++));
      
    start = next + 1;
  }
  
  return pos;
}

void AbstractSequence::remove(int start, int end) {
  ensure_boxes_valid();
  
  text_changed(true);
  
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < start)
    ++i;
    
  int j = i;
  while(j < boxes.length() && boxes[j]->index() < end)
    boxes[j++]->safe_destroy();
    
  boxes_invalid(i < boxes.length());
  boxes.remove(i, j - i);
  str.remove(start, end - start);
  invalidate();
}

Box *AbstractSequence::remove(int *index) {
  remove(*index, *index + 1);
  return this;
}

Box *AbstractSequence::extract_box(int boxindex) {
  Box *box = boxes[boxindex];
  
  DummyBox *dummy = new DummyBox();
  adopt(dummy, box->index());
  boxes.set(boxindex, dummy);
  
  abandon(box);
  return box;
}

//} ... insert/remove

int AbstractSequence::get_box(int index, int guide) {
  RICHMATH_ASSERT(str[index] == PMATH_CHAR_BOX);
  
  ensure_boxes_valid();
  if(guide < 0)
    guide = 0;
    
  for(int box = guide; box < boxes.length(); ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  
  if(guide >= boxes.length())
    guide = boxes.length();
    
  for(int box = 0; box < guide; ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  RICHMATH_ASSERT(0 && "no box found at index.");
  return -1;
}

VolatileSelection AbstractSequence::dynamic_to_literal(int start, int end) {
  int b = 0;
  while(b < count()) {
    Box *box = item(b);
    if(box->index() >= start)
      break;
      
    ++b;
  }
  
  while(b < count()) {
    Box *box = item(b);
    
    if(box->index() >= end)
      break;
    
    VolatileSelection next = box->all_dynamic_to_literal();
    if(next.box == this) {
      end += next.length() - 1; // TODO: -1 for the old PMATH_CHAR_BOX is only correct for MathSequence 
      
      while(b < count()) {
        box = item(b);
        if(box->index() >= next.end)
          break;
        ++b;
      }
    }
    else
      ++b;
  }
  
  return {this, start, end};
}

bool AbstractSequence::request_repaint_range(int start, int end) {
  // TODO: Move this to TextSequence, because MathSequence now has its own implementation.
  if(text_changed())
    return request_repaint_all();
  
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
    
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child_transformation(start, &mat);
  
  Point p1 = Canvas::transform_point(mat, Point(0, 0));
  
  Point p2;
  if(start == end) {
    p2 = p1;
  }
  else {
    cairo_matrix_init_identity(&mat);
    child_transformation(end, &mat);
    p2 = Canvas::transform_point(mat, Point(0, 0));
  }
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  if(l1 == l2)
    return request_repaint({p1.x, p1.y - a1, p2.x - p1.x, a1 + d1});
             
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  bool result = true;
  result = request_repaint({p1.x, p1.y - a1, extents().width - p1.x, a1 + d1}) || result;
  result = request_repaint({0.0f, p1.y + d1, extents().width, p2.y - a2 - p1.y - d1}) || result;
  result = request_repaint({0.0f, p2.y - a2, p2.x, a2 + d2}) || result;
  return result;
}

RectangleF AbstractSequence::range_rect(int start, int end) {
  // TODO: Move this to TextSequence, because MathSequence now has its own implementation.
  
  if(text_changed())
    return _extents.to_rectangle();
  
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
    
  cairo_matrix_t mat;
  cairo_matrix_init_identity(&mat);
  child_transformation(start, &mat);
  
  Point p1 = Canvas::transform_point(mat, Point(0, 0));
  
  Point p2;
  if(start == end) {
    p2 = p1;
  }
  else {
    cairo_matrix_init_identity(&mat);
    child_transformation(end, &mat);
    p2 = Canvas::transform_point(mat, Point(0, 0));
  }
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  if(l1 == l2)
    return {p1.x, p1.y - a1, p2.x - p1.x, a1 + d1};
  
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  return RectangleF{p1.x, p1.y - a1, extents().width - p1.x, a1 + d1}
    .union_hull({0.0f, p1.y + d1, extents().width, p2.y - a2 - p1.y - d1})
    .union_hull({0.0f, p2.y - a2, p2.x, a2 + d2});
  
}

//} ... class AbstractSequence
