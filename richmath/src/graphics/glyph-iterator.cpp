#include <graphics/glyph-iterator.h>


using namespace richmath;

//{ class GlyphIterator ...

void GlyphIterator::move_next_glyph() {
  if(_glyph_index >= _owning_seq->glyph_array().length())
    return;
  
  ++_glyph_index;
  
  if(_current_seq != _owning_seq) { // _owning_seq->glyph_to_seq[_glyph_index]
    _current_seq = _owning_seq;
    _current_buf = _current_seq->text().buffer();
  }
  ++_text_index; // _text_index = _owning_seq->glyph_to_text[_glyph_index]
  
  if(_text_index < text_buffer_length()) {
    _current_char = _current_buf[_text_index];
  }
  else {
    _current_char = 0;
  }
}

void GlyphIterator::skip_to_glyph_after_text_pos(MathSequence *seq, int pos) {
  if(!has_more_glyphs())
    return;
  
  // In the scematics below we use the following notation:
  //   [ and ]   begin and end of a sequence
  //   ooo       glyphs in the `_owning_seq`
  //   ccc       glyphs in the `_current_seq` (before the move)
  //   ^         current position (`_text_index` inside `_current_seq`)
  //   sss       glyphs in the requested `seq`
  //   |         is requested position (`pos` inside `seq`)
  
  while(has_more_glyphs()) {
    int pos_in_cur = seq == current_sequence() ? pos : index_in_sequence(current_sequence(), seq);
    if(pos_in_cur >= 0) { // [ccc[sss]ccc]
      if(text_index() >= pos_in_cur) // [ccc[ss|s]ccc^ccc]  or  [ccc[sss|]^ccc]
        return;
      
      // [cc^c[ss|s]ccc]
      skip_to_glyph_after_current_text_pos(pos_in_cur);
      // [ccc[^ss|s]ccc]
      // or
      // [ccc[|]^ccc]
    }
    else {
      // [ooo[cc^c]ooo[ss|s]ooo]  or  [ooo[ss|s]ooo[cc^c]ooo]
      skip_to_glyph_after_current_text_pos(_current_seq->length());
      // [ooo[ccc]^ooo[ss|s]ooo]  or  [ooo[ss|s]ooo[ccc]^ooo]
    }
  }
}

void GlyphIterator::skip_to_glyph_after_current_text_pos(int pos) {
  if(!has_more_glyphs())
    return;
  
  MathSequence *seq = current_sequence();
  
  assert(0 <= pos);
  assert(pos <= seq->length());
  
  // slow: linear search
  while(has_more_glyphs()) {
    if(current_sequence() == seq) {
      if(text_index() >= pos)
        return;
    }
    else {
      int pos_in_seq = index_in_sequence(seq, current_sequence());
      if(pos_in_seq < 0) // not inside seq any more
        return;
      
      if(pos_in_seq >= pos) // inside nested sequence after pos
        return;
    }
    move_next_glyph();
  }
}

void GlyphIterator::move_token_end() {
  if(_glyph_index >= _owning_seq->glyph_array().length())
    return;
  
  while(_text_index < text_buffer_length()) {
    _current_char = _current_buf[_text_index];
    if(text_span_array().is_token_end(_text_index)) 
      break;
    
    ++_text_index;
    ++_glyph_index; // TODO: repeat until glyph points to new _text_index;
  }
}

void GlyphIterator::move_deepest_span_end() {
  if(!has_more_glyphs())
    return;
  
  Span span = text_span_array()[text_index()];
  if(!span) {
    move_token_end();
    return;
  }
  for(;;) {
    Span next = span.next();
    if(!next) 
      break;
    span = next;
  }
  skip_to_glyph_after_current_text_pos(span.end());
}

int GlyphIterator::index_in_sequence(MathSequence *parent) {
  if(parent == _current_seq)
    return text_index();
  
  return index_in_sequence(parent, _current_seq);
}

int GlyphIterator::index_in_sequence(MathSequence *parent, Box *other) {
  assert(parent);
  
  int index = -1;
  while(other && other != parent) {
    index = other->index();
    other = other->parent();
  }
  
  if(other)
    return index;
  else
    return -1;
}

Box *GlyphIterator::current_box() const {
  // TODO: use MathSequence::get_box ?
  
  if(current_char() != PMATH_CHAR_BOX)
    return nullptr;
  
  int count = current_sequence()->count();
  
  for(int i = _next_box_index; i < count; ++i) {
    Box *b = current_sequence()->item(i);
    
    int idx = b->index();
    if(idx == text_index()) {
      _next_box_index = i;
      return b;
    }
    
    if(idx > text_index())
      break;
  }
  
  for(int i = 0; i < _next_box_index; ++i) {
    Box *b = current_sequence()->item(i);
    
    int idx = b->index();
    if(idx == text_index()) {
      _next_box_index = i;
      return b;
    }
    
    if(idx > text_index())
      break;
  }
  
  return nullptr;
}

//} ... class GlyphIterator
