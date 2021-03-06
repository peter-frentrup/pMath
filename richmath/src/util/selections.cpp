#include <util/selections.h>

#include <boxes/box.h>
#include <boxes/gridbox.h>
#include <boxes/mathsequence.h>
#include <boxes/sectionlist.h>
#include <boxes/textsequence.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_Range;
extern pmath_symbol_t richmath_Language_SourceLocation;

namespace {
  class VolatileSelectionImpl {
    private:
      VolatileSelection &self;
    
    public:
      VolatileSelectionImpl(VolatileSelection &_self) : self(_self) {}
      
      static bool is_inside_string(VolatileSelection sel);
      
      void expand();
      
    private:
      void expand_default();
      void expand_math();
      void expand_text();
  };
}

//{ class LocationReference ...

LocationReference::LocationReference()
  : id(FrontEndReference::None),
    index(0)
{
}

LocationReference::LocationReference(Box *_box, int _index)
  : id(_box ? _box->id() : FrontEndReference::None),
    index(_box ? _index : 0)
{
}

LocationReference::LocationReference(FrontEndReference _id, int _index)
  : id(_id),
    index(_index)
{
}

Box *LocationReference::get() {
  if(!id)
    return nullptr;
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return nullptr;
    
  if(index > result->length())
    index = result->length();
  
  return result;
}

void LocationReference::set_raw(Box *box, int _index) {
  if(box) {
    id = box->id();
    index = _index;
  }
  else {
    id = FrontEndReference::None;
    index = 0;
  }
}

bool LocationReference::equals(Box *box, int _index) const {
  if(!box)
    return !id.is_valid();
    
  return id == box->id() && index == _index;
}

//} ... class LocationReference

//{ class VolatileSelection ...

bool VolatileSelection::visually_contains(VolatileSelection other) const {
  if(is_empty())
    return *this == other;
  
  // Section selections are only at the right margin, the section content is
  // not inside the selection-frame
  if(auto seclist = dynamic_cast<SectionList*>(box) && other.box != box)
    return false;
  
  while(other.box != box) {
    if(!other)
      return false;
    
    other.expand_to_parent();
  }
  
  if(auto grid = dynamic_cast<GridBox*>(box)) {
    auto this_rect  = grid->get_enclosing_range(start,       end);
    auto other_rect = grid->get_enclosing_range(other.start, other.end);
    
    return this_rect.contains(other_rect);
  }
  
  return start <= other.start && other.end <= end;
}

bool VolatileSelection::null_or_selectable() const {
  if(!box)
    return true;
  
  return box->selectable();
}

bool VolatileSelection::selectable() const {
  if(!box)
    return false;
  
  return box->selectable();
}

bool VolatileSelection::is_name() const {
  MathSequence *seq = dynamic_cast<MathSequence *>(box);
  if(!seq)
    return false;
    
  const uint16_t *buf = seq->text().buffer();
  for(int i = start; i < end; ++i) {
    if(!pmath_char_is_digit(buf[i]) && !pmath_char_is_name(buf[i]))
      return false;
    
    if(seq->span_array().is_token_end(i) && i < end - 1)
      return false;
  }
  
  return true;
}

bool VolatileSelection::is_inside_string() const {
  return VolatileSelectionImpl::is_inside_string(*this);
}

Box *VolatileSelection::contained_box() const {
  if(!box)
    return nullptr;
  
  if(auto seq = dynamic_cast<AbstractSequence*>(box)) {
    if(auto tseq = dynamic_cast<TextSequence*>(seq)) {
      if(!tseq->text_buffer().is_box_at(start, end))
        return nullptr;
    }
    else if(start + 1 != end || seq->char_at(start) != PMATH_CHAR_BOX) 
      return nullptr;
      
    for(int i = seq->count() - 1; i >= 0; --i) {
      Box *item = seq->item(i);
      if(item->index() == start)
        return item;
    }
    
    return nullptr;
  }
  
  if(start + 1 == end && start >= 0 && end <= box->count())
    return box->item(start);
    
  return nullptr;
}

Expr VolatileSelection::to_pmath(BoxOutputFlags flags) const {
  if(!box)
    return Expr();
  
  return box->to_pmath(flags, start, end);
} 

void VolatileSelection::add_path(Canvas &canvas) const {
  if(box) {
    canvas.save();
    
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    box->transformation(0, &mat);
    
    canvas.transform(mat);
    
    canvas.move_to(0, 0);
    box->selection_path(canvas, start, end);
    
    canvas.restore();
  }
}

void VolatileSelection::expand() {
  VolatileSelectionImpl(*this).expand();
}

void VolatileSelection::expand_to_parent() {
  if(!box)
    return;
  
  start = box->index();
  end = start + 1;
  box = box->parent();
}

void VolatileSelection::expand_up_to_sibling(const VolatileSelection &sibling, int max_steps) {
  Box *stop = Box::common_parent(box, sibling.box);
  
  while(max_steps-- > 0) {
    auto next = expanded();
    
    if(!next || (box == stop && next.box != stop) || next == *this)
      return;
    
    if( box_order(next.box, next.start, sibling.box, sibling.start) <= 0 &&
        box_order(sibling.box, sibling.end, next.box, next.end) <= 0
    ) {
      return;
    }
    
    *this = next;
  }
}

void VolatileSelection::normalize() {
  if(box) 
    *this = box->normalize_selection(start, end); 
}

void VolatileSelection::dynamic_to_literal() {
  if(box)
    *this = box->dynamic_to_literal(start, end);
}

//} ... class VolatileSelection

//{ class SelectionReference ...

SelectionReference::SelectionReference()
  : id(FrontEndReference::None),
    start(0),
    end(0)
{
}

SelectionReference::SelectionReference(Box *box, int start, int end)
  : id(box ? box->id() : FrontEndReference::None),
    start(box ? start : 0),
    end(box ? end : 0)
{
}

SelectionReference::SelectionReference(FrontEndReference id, int start, int end)
  : id(id),
    start(start),
    end(end)
{
}

Box *SelectionReference::get() {
  if(!id)
    return nullptr;
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return nullptr;
    
  if(start > result->length())
    start = result->length();
    
  if(end > result->length())
    end = result->length();
    
  return result;
}

VolatileSelection SelectionReference::get_all() {
  Box *box = get();
  return VolatileSelection(box, start, end);
}

void SelectionReference::set(Box *new_box, int new_start, int new_end) {
  if(new_box)
    set_raw(new_box->normalize_selection(new_start, new_end));
  else
    set_raw(nullptr, 0, 0);
}

void SelectionReference::set_raw(Box *new_box, int new_start, int new_end) {
  if(new_box) {
    id = new_box->id();
    start = new_start;
    end = new_end;
  }
  else
    id = FrontEndReference::None;
}

void SelectionReference::move_after_edit(const SelectionReference &before_edit, const SelectionReference &after_edit) {
  if(!id || !after_edit.id)
    return;
  
  if(id == before_edit.id) {
    if(after_edit.id != before_edit.id || after_edit.start != before_edit.start) {
      *this = after_edit;
      return;
    }
    
    if(end <= before_edit.start)
      return;
    
    if(end <= before_edit.end)
      end = after_edit.end;
    else
      end+= after_edit.end - before_edit.end;
    
    if(start <= before_edit.start)
      return;
    
    if(start < before_edit.end)
      start = after_edit.start;
    else
      start+= after_edit.end - before_edit.end;
  }
  else if(Box *box = get()) {
    Box *top_most = box;
    while(top_most->parent())
      top_most = top_most->parent();
    
    if(!top_most->is_parent_of(FrontEndObject::find_cast<Box>(after_edit.id))) {
      *this = after_edit;
      return;
    }
  }
  else {
    *this = after_edit;
    return;
  }
}

bool SelectionReference::equals(Box *other_box, int other_start, int other_end) const {
  if(!other_box)
    return !id.is_valid();
    
  SelectionReference other;
  other.set(other_box, other_start, other_end);
  return equals(other);
}

int SelectionReference::cmp_lexicographic(const SelectionReference &other) const {
  if(id < other.id) return -1;
  if(id > other.id) return 1;
  
  if(start < other.start) return -1;
  if(start > other.start) return 1;
  
  if(end < other.end) return -1;
  if(end > other.end) return 1;
  
  return 0;
}

Expr SelectionReference::to_debug_info() const {
  if(id == FrontEndReference::None)
    return Expr();
  
  return Call(
           Symbol(richmath_Language_SourceLocation),
           id.to_pmath(),
           Call(Symbol(richmath_System_Range), start, end));
}

SelectionReference SelectionReference::from_debug_info(Expr expr) {
  SelectionReference result;
  
  if(expr[0] != richmath_Language_SourceLocation)
    return result;
  
  if(expr.expr_length() != 2)
    return result;
  
  result.id = FrontEndReference::from_pmath(expr[1]);
  Expr range = expr[2];
  if( range.expr_length() == 2 && 
      range[0] == richmath_System_Range &&
      range[1].is_int32() &&
      range[2].is_int32())
  {
    result.start = PMATH_AS_INT32(range[1].get());
    result.end   = PMATH_AS_INT32(range[2].get());
    
    return result;
  }
  
  result.id = FrontEndReference::None;
  return result; 
}

SelectionReference SelectionReference::from_debug_info_of(Expr expr) {
  return from_debug_info(Expr{pmath_get_debug_info(expr.get())});
}

SelectionReference SelectionReference::from_debug_info_of(pmath_t expr) {
  return from_debug_info(Expr{pmath_get_debug_info(expr)});
}

//} ... class SelectionReference

int richmath::box_depth(Box *box) {
  int result = 0;
  while(box) {
    ++result;
    box = box->parent();
  }
  return result;
}

int richmath::box_order(Box *b1, int i1, Box *b2, int i2) {
  int od1, od2, d1, d2;
  od1 = d1 = box_depth(b1);
  od2 = d2 = box_depth(b2);
  
  while(d1 > d2) {
    i1 = b1->index();
    b1 = b1->parent();
    --d1;
  }
  
  while(d2 > d1) {
    i2 = b2->index();
    b2 = b2->parent();
    --d2;
  }
  
  while(b1 != b2 && b1 && b2) {
    i1 = b1->index();
    b1 = b1->parent();
    
    i2 = b2->index();
    b2 = b2->parent();
  }
  
  if(i1 == i2)
    return od1 - od2;
    
  return i1 - i2;
}

//{ class VolatileSelectionImpl ...

bool VolatileSelectionImpl::is_inside_string(VolatileSelection sel) {
  for(; sel.box; sel.expand_to_parent()) {
    if(auto seq = dynamic_cast<MathSequence *>(sel.box)) {
      int string_end;
      int string_start = seq->find_string_start(sel.start, &string_end);
      if(string_start >= 0 && sel.end <= string_end)
        return true;
    }
    else if(dynamic_cast<TextSequence *>(sel.box))
      return true;
  }
  
  return false;
}

void VolatileSelectionImpl::expand() {
  if(!self.box)
    return;
    
  if(dynamic_cast<MathSequence *>(self.box))
    expand_math();
  else if(dynamic_cast<TextSequence *>(self.box))
    expand_text();
  else
    expand_default();
}

void VolatileSelectionImpl::expand_default() {
  int index = self.box->index();
  Box *box2 = self.box->parent();
  while(box2) {
    if(dynamic_cast<AbstractSequence *>(box2)) {
      if(box2->selectable()) {
        self.box = box2;
        self.start = index;
        self.end = index + 1;
        return;
      }
    }
    
    index = box2->index();
    box2 = box2->parent();
  }
}

void VolatileSelectionImpl::expand_math() {
  MathSequence * const seq = (MathSequence*)self.box;
  
  for(int i = self.start; i < self.end; ++i) {
    if(seq->span_array().is_token_end(i))
      goto MULTIPLE_TOKENS;
  }
  
  if( self.start == self.end &&
      self.start > 0 &&
      !seq->span_array().is_operand_start(self.start))
  {
    --self.start;
    --self.end;
  }
  
  while(self.start > 0 && !seq->span_array().is_token_end(self.start - 1))
    --self.start;
    
  while(self.end < seq->length() && !seq->span_array().is_token_end(self.end))
    ++self.end;
    
  if(self.end < seq->length())
    ++self.end;
  return;
  
MULTIPLE_TOKENS:
  if(self.start < seq->length()) {
    if(Span s = seq->span_array()[self.start]) {
      int e = s.end();
      while(s && s.end() >= self.end) {
        e = s.end();
        s = s.next();
      }
      
      if(e >= self.end) {
        self.end = e + 1;
        return;
      }
    }
  }
  
  int orig_start = self.start;
  int orig_end = self.end;
  const uint16_t *buf = seq->text().buffer();
  
  int a = self.start;
  while(--a >= 0) {
    if(Span s = seq->span_array()[a]) {
      int e = s.end();
      while(s && s.end() + 1 >= self.end) {
        e = s.end();
        s = s.next();
      }
      
      if(e + 1 >= self.end) {
        self.start = a;
        while(self.start < orig_start && buf[self.start] == '\n')
          ++self.start;
        self.end = e + 1;
        if(self.start == orig_start && self.end == orig_end && orig_end < seq->length()) {
          ++self.end;
          ++a;
          continue;
        }
        return;
      }
    }
  }
  
  if(self.start > 0 || self.end < seq->length()) {
    self.start = 0;
    self.end = seq->length();
    return;
  }
  
  expand_default();
}

void VolatileSelectionImpl::expand_text() {
  TextSequence * const seq = (TextSequence*)self.box;
  
  if(self.start == 0 && self.end == seq->length()) {
    expand_default();
    return;
  }
    
  int n_attrs;
  const PangoLogAttr *attrs = pango_layout_get_log_attrs_readonly(seq->get_layout(), &n_attrs);
  
  const char *buf = seq->text_buffer().buffer();
  const char *s              = buf;
  const char *s_end          = buf + seq->length();
  const char *word_start     = buf;
  
  int i = 0;
  while(s && (size_t)s - (size_t)buf <= (size_t)self.start) {
    if(attrs[i].is_word_start)
      word_start = s;
      
    ++i;
    s = g_utf8_find_next_char(s, s_end);
  }
  
  const char *word_end = nullptr;
  
  while(s && !word_end) {
    if(attrs[i].is_word_boundary && word_end != word_start)
      word_end = s;
      
    ++i;
    s = g_utf8_find_next_char(s, s_end);
  }
  
  attrs = nullptr;
  
  if(!word_end)
    word_end = s_end;
    
  if( (size_t)word_end - (size_t)buf       >= (size_t)self.end &&
      (size_t)word_end - (size_t)word_start > (size_t)self.end - (size_t)self.start)
  {
    self.start = (int)((size_t)word_start - (size_t)buf);
    self.end   = (int)((size_t)word_end   - (size_t)buf);
  }
  else {
    GSList *lines = pango_layout_get_lines_readonly(seq->get_layout());
    
    int prev_par_start = 0;
    int paragraph_start = 0;
    while(lines) {
      PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
      
      if(line->is_paragraph_start && line->start_index <= self.start) {
        prev_par_start = paragraph_start;
        paragraph_start = line->start_index;
      }
      
      if(line->start_index + line->length >= self.end) {
        if(line->start_index <= self.start && self.end - self.start < line->length) {
          self.start = line->start_index;
          self.end = line->start_index + line->length;
          break;
        }
        
        int old_end = self.end;
        
        lines = lines->next;
        while(lines) {
          PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
          if(line->is_paragraph_start && line->start_index >= self.end) {
            self.end = line->start_index;
            break;
          }
          
          lines = lines->next;
        }
        
        if(!lines)
          self.end = seq->length();
          
        if(old_end - self.start < self.end - paragraph_start)
          self.start = paragraph_start;
        else
          self.start = prev_par_start;
          
        break;
      }
      
      lines = lines->next;
    }
  }
}

//} ... class VolatileSelectionImpl
