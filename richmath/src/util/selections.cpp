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

//{ class VolatileLocation ...

int richmath::document_order(VolatileLocation left, VolatileLocation right) {
  int orig_depth_left;
  int orig_depth_right;
  int depth_left;
  int depth_right;
  
  orig_depth_left  = depth_left  = Box::depth(left.box);
  orig_depth_right = depth_right = Box::depth(right.box);
  
  while(depth_left > depth_right) {
    left = left.parent();
    --depth_left;
  }
  
  while(depth_right > depth_left) {
    right = right.parent();
    --depth_right;
  }
  
  while(left.box != right.box && left.box && right.box) {
    left = left.parent();
    right = right.parent();
  }
  
  if(left.index == right.index)
    return orig_depth_left - orig_depth_right;
    
  return left.index - right.index;
}

bool VolatileLocation::selectable() const {
  if(!box)
    return false;
  
  return box->selectable(index);
}

bool VolatileLocation::exitable() const {
  if(!box)
    return false;
  
  return box->exitable();
}

bool VolatileLocation::selection_exitable() const {
  if(!box)
    return false;
  
  return box->selection_exitable();
}

VolatileLocation VolatileLocation::move_logical(LogicalDirection direction, bool jumping) const {
  VolatileLocation copy = *this;
  copy.move_logical_inplace(direction, jumping);
  return copy;
}

void VolatileLocation::move_logical_inplace(LogicalDirection direction, bool jumping) {
  if(box)
    box = box->move_logical(direction, jumping, &index);
}

VolatileLocation VolatileLocation::move_vertical(LogicalDirection direction, float *index_rel_x) const {
  VolatileLocation copy = *this;
  copy.move_vertical_inplace(direction, index_rel_x);
  return copy;
}

void VolatileLocation::move_vertical_inplace(LogicalDirection direction, float *index_rel_x) {
  if(box)
    box = box->move_vertical(direction, index_rel_x, &index, false);
}

VolatileLocation VolatileLocation::parent(LogicalDirection direction) const {
  if(!box)
    return {nullptr, 0};
  
  return {box->parent(), box->index() + ((direction == LogicalDirection::Forward) ? 1 : 0)};
}

bool VolatileLocation::find_next(String string, bool complete_token, const VolatileLocation &stop) {
RESTART:
  if(!box)
    return false;
  
  if(auto seq = dynamic_cast<AbstractSequence *>(box)) {
    const uint16_t *buf = string.buffer();
    int             len = string.length();
    
    const uint16_t *seqbuf = seq->text().buffer();
    int             seqlen = seq->text().length();
    
    if(stop.box == seq)
      seqlen = stop.index;
    
    for(; index <= seqlen - len; index++) {
      if(0 == memcmp(seqbuf + index, buf, len * sizeof(buf[0]))) {
        if(!complete_token) {
          index+= len;
          return true;
        }
        
        if(seq->is_word_boundary(index) && seq->is_word_boundary(index + len)) {
          int j = 0;
          
          for(; j < len; ++j) {
            if(j > 0 && seq->is_word_boundary(index + j))
              break;
          }
          
          if(j == len) {
            index+= len;
            return true;
          }
        }
      }
      
      if(seqbuf[index] == PMATH_CHAR_BOX) {
        box = box->item(seq->get_box(index));
        index = 0;
        goto RESTART;
      }
    }
    
    for(; index < seqlen; index++) {
      if(seqbuf[index] == PMATH_CHAR_BOX) {
        box = box->item(seq->get_box(index));
        index = 0;
        goto RESTART;
      }
    }
  }
  else if(box == stop.box) {
    if(index >= stop.index || index >= box->count())
      return false;
    
    box = box->item(index);
    index = 0;
    goto RESTART;
  }
  else if(index < box->count()) {
    box = box->item(index);
    index = 0;
    goto RESTART;
  }
  
  if(box) {
    *this = parent(LogicalDirection::Forward);
    goto RESTART;
  }
  
  return false;
}

bool VolatileLocation::find_selection_placeholder(LogicalDirection direction, const VolatileLocation &stop, bool stop_early) {
  VolatileLocation match { nullptr, 0 };
  
  while(box) {
    if(box == stop.box) {
      if(direction == LogicalDirection::Forward) {
        if(index >= stop.index)
          break;
      }
      else {
        if(index <= stop.index)
          break;
      }
    }
    
    AbstractSequence *current_seq = dynamic_cast<AbstractSequence *>(box);
    
    if(current_seq && current_seq->is_placeholder(index)) {
      if(stop_early || current_seq->char_at(index) == PMATH_CHAR_SELECTIONPLACEHOLDER)
        return true;
      
      if(!match) 
        match = *this;
    }
    
    auto old = *this;
    move_logical_inplace(direction, false);
    if(*this == old)
      break;
  }
  
  *this = match;
  return match.box != nullptr;
}

//} ... class VolatileLocation

//{ class VolatileSelection ...

bool VolatileSelection::visually_contains(VolatileSelection other) const {
  if(is_empty())
    return *this == other;
  
  // Section selections are only at the right margin, the section content is
  // not inside the selection-frame
  if(auto seclist = dynamic_cast<SectionList*>(box) && other.box != box)
    return false;
  
  return logically_contains(other);
}

bool VolatileSelection::logically_contains(VolatileSelection other) const {
  if(is_empty())
    return *this == other;
  
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

bool VolatileSelection::request_repaint() const {
  if(!box)
    return false;
  
  return box->request_repaint_range(start, end);
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
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(box);
  if(!seq)
    return false;
    
  const uint16_t *buf = seq->text().buffer();
  for(int i = start; i < end; ++i) {
    if(!pmath_char_is_digit(buf[i]) && !pmath_char_is_name(buf[i]))
      return false;
    
    if(start < i && i < end && seq->is_word_boundary(i))
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
    if(start + 1 != end || seq->char_at(start) != PMATH_CHAR_BOX) 
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

void VolatileSelection::add_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0) const {
  if(box)
    box->selection_rectangles(rects, flags, p0, start, end);
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
    
    if( document_order(next.start_only(),  sibling.start_only()) <= 0 &&
        document_order(sibling.end_only(), next.end_only())      <= 0
    ) {
      return;
    }
    
    *this = next;
  }
}

void VolatileSelection::expand_nearby_placeholder(float *index_rel_x) {
  if(auto seq = dynamic_cast<MathSequence*>(box)) {
    seq->select_nearby_placeholder(&start, &end, index_rel_x);
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

VolatileLocation VolatileSelection::find_selection_placeholder(LogicalDirection direction, bool stop_early) {
  VolatileLocation loc = start_end_only(opposite_direction(direction));
  if(loc.find_selection_placeholder(direction, start_end_only(direction), stop_early))
    return loc;
  return {nullptr, 0};
}

//} ... class VolatileSelection

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

VolatileLocation LocationReference::get_all() {
  if(!id)
    return {nullptr, 0};
    
  Box *result = FrontEndObject::find_cast<Box>(id);
  if(!result)
    return {nullptr, 0};
    
  if(index > result->length())
    index = result->length();
  
  return {result, index};
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

Expr SelectionReference::to_pmath() const {
  if(id == FrontEndReference::None)
    return Expr();
  
  return Call(
           Symbol(richmath_Language_SourceLocation),
           id.to_pmath(),
           Call(Symbol(richmath_System_Range), start, end));
}

SelectionReference SelectionReference::from_pmath(Expr expr) {
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

SelectionReference SelectionReference::from_debug_metadata_of(Expr expr) {
  return from_pmath(Expr{pmath_get_debug_metadata(expr.get())});
}

SelectionReference SelectionReference::from_debug_metadata_of(pmath_t expr) {
  return from_pmath(Expr{pmath_get_debug_metadata(expr)});
}

//} ... class SelectionReference

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
  
  TextLayoutIterator iter_start = seq->outermost_layout_iter();
  iter_start.skip_forward_beyond_text_pos(seq, self.start);
  
  TextLayoutIterator iter_end = iter_start;
  iter_end.skip_forward_beyond_text_pos(seq, self.end);
  
  TextLayoutIterator new_start = iter_start;
  
  while(new_start.byte_index() > 0 && !new_start.is_word_start()) {
    new_start.move_previous_char();
    if(new_start.current_char() == '\n') {
      new_start.move_next_char();
      break;
    }
  }
  
  TextLayoutIterator new_end = iter_start; // not iter_end!
  while(new_end.has_more_bytes() && !new_end.is_word_end() && new_end.current_char() != '\n')
    new_end.move_next_char();
  
  auto large_enough = [&]() { 
    return (new_start.byte_index() <= iter_start.byte_index() && 
      iter_end.byte_index() <= new_end.byte_index() &&
      iter_end.byte_index() - iter_start.byte_index() < new_end.byte_index() - new_start.byte_index());
  };
  
  if(!large_enough()) {
    // TODO: try sentence boundaries next.
    
    if(iter_end.current_char() == '\n')
      iter_end.move_next_char();
  
    const GSList *lines = pango_layout_get_lines_readonly(iter_start.outermost_sequence()->get_layout());
    
    int paragraph_start = 0;
    while(lines) {
      const PangoLayoutLine *line = (const PangoLayoutLine *)lines->data;
      
      if(line->is_paragraph_start) {
        if(line->start_index <= iter_start.byte_index()) {
          paragraph_start = line->start_index;
        }
        else if(line->start_index <= iter_end.byte_index()) {
          // embedded paragraph break => select whole text
          new_start.rewind_to_byte(0);
          new_end.skip_forward_beyond_text_pos(seq, seq->length());
          break;
        }
      }
      
      if(line->start_index + line->length >= iter_end.byte_index()) {
        if(line->start_index <= iter_start.byte_index() && iter_end.byte_index() - iter_start.byte_index() < line->length) {
          new_start.rewind_to_byte(line->start_index);
          new_end.rewind_to_byte(line->start_index + line->length);
          break;
        }
        
        lines = lines->next;
        while(lines) {
          const PangoLayoutLine *line = (const PangoLayoutLine *)lines->data;
          if(line->is_paragraph_start && line->start_index >= iter_end.byte_index()) {
            new_end.rewind_to_byte(line->start_index);
            break;
          }
          
          lines = lines->next;
        }
        
        if(!lines)
          new_end.rewind_to_byte(new_end.byte_count());
        
        if(iter_end.byte_index() - iter_start.byte_index() < new_end.byte_index() - paragraph_start)
          new_start.rewind_to_byte(paragraph_start);
        else
          new_start.rewind_to_byte(0);
          
        break;
      }
      
      lines = lines->next;
    }
  }
  
  if(large_enough()) {
    Box *common_parent = Box::common_parent(new_start.current_sequence(), new_end.current_sequence());
    
    if(TextSequence *new_seq = dynamic_cast<TextSequence*>(common_parent)) {
      self.box = new_seq;
      self.start = new_start.index_in_sequence(new_seq, 0);
      self.end   = new_end.index_in_sequence(new_seq, new_seq->length());
      return;
    }
  }
  
  self.start = 0;
  self.end = seq->length();
}

//} ... class VolatileSelectionImpl
