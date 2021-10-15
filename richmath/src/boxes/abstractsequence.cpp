#include <boxes/abstractsequence.h>


using namespace richmath;

//{ class AbstractSequence ...

AbstractSequence::AbstractSequence()
  : Box()
{
}

AbstractSequence::~AbstractSequence() {
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

bool AbstractSequence::try_load_from_object(Expr object, BoxInputFlags options) {
  load_from_object(object, options);
  return true;
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

//} ... class AbstractSequence
