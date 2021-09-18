#include <graphics/glyph-iterator.h>


using namespace richmath;

namespace richmath {
  class GlyphIterator::Impl {
    public:
      Impl(GlyphIterator &self) : self{self} {}
      
      static int index_in_sequence(MathSequence *parent, Box *other, int fallback);
    
    private:
      GlyphIterator &self;
  };
}

//{ class GlyphIterator ...

GlyphIterator::GlyphIterator(MathSequence &seq)
  : _owning_seq{&seq},
    _current_buf{nullptr},
    g2t_iter{seq.glyph_to_text_iter()},
    g2seq_iter{seq.glyph_to_inline_sequence_iter()},
    _semantic_style_iter{seq.semantic_styles_array().begin()},
    _next_box_index{0},
    _current_char{0}
{
  move_by_glyphs(0);
}

void GlyphIterator::move_by_glyphs(int delta) {
  ARRAY_ASSERT(delta >= 0 || glyph_index() + delta >= 0);
  
  if(delta > _owning_seq->glyph_array().length() - glyph_index())
    delta = _owning_seq->glyph_array().length() - glyph_index();
  
  auto oldseq = current_sequence();
  
  g2t_iter+= delta;
  g2seq_iter+= delta;
  
  auto newseq = g2seq_iter.get();
  if(!newseq)
    newseq = _owning_seq;
  
  if(newseq != oldseq || !_current_buf) 
    _current_buf = newseq->text().buffer();
  
  int ti = text_index();
  if(ti < text_buffer_length()) {
    _current_char = _current_buf[ti];
  }
  else {
    _current_char = 0;
  }
}

void GlyphIterator::skip_forward_to_glyph_after_text_pos(MathSequence *seq, int pos) {
  assert(0 <= pos);
  assert(pos <= seq->length());
  
  // In the scematics below we use the following notation:
  //   [ and ]   begin and end of a sequence
  //   ooo       glyphs in the `_owning_seq`
  //   ccc       glyphs in the `current_sequence()` (before the move)
  //   ^         current position (`_text_index` inside `_current_seq`)
  //   sss       glyphs in the requested `seq`
  //   |         is requested position (`pos` inside `seq`)
  
  while(has_more_glyphs()) {
    int pos_in_cur = (seq == current_sequence()) ? pos : Impl::index_in_sequence(current_sequence(), seq, -1);
    if(pos_in_cur >= 0) { // [ccc[sss]ccc]
      if(text_index() >= pos_in_cur) // [ccc[ss|s]ccc^ccc]  or  [ccc[sss|]^ccc]
        return;
      
      // [cc^c[ss|s]ccc]
      skip_forward_to_glyph_after_current_text_pos(pos_in_cur);
      // [ccc[^ss|s]ccc]
      // or
      // [ccc[|]^ccc]
    }
    else {
      int pos_in_seq = (seq == current_sequence()) ? pos : Impl::index_in_sequence(seq, current_sequence(), -1);
      if(pos_in_seq >= 0) {
         // [sss[c^cc]s|ss]   or   [ss|s[c^cc]sss]
         if(pos_in_seq >= pos)
           break;
        
        // [sss[c^cc]s|ss]
      }
      else {
        // handle [ooo[sss|][^ccc]ooo]  and  [ooo[ss|s]ooo[cc^c]ooo]  case
        int order = box_order(seq, pos, current_sequence(), text_index());
        if(order <= 0)
          break;
      }
      
      // [ooo[cc^c]ooo[ss|s]ooo]   or   [sss[c^cc]s|ss]
      skip_forward_to_glyph_after_current_text_pos(text_buffer_length());
      // [ooo[ccc]^ooo[ss|s]ooo]   or   [sss[ccc]^s|ss]
    }
  }
}

void GlyphIterator::skip_forward_to_glyph_after_current_text_pos(int pos) {
  MathSequence *seq = current_sequence();
  
  assert(0 <= pos);
  assert(pos <= seq->length());
  
//  // slow: linear search
//  while(has_more_glyphs()) {
//    if(current_sequence() == seq) {
//      if(text_index() >= pos)
//        return;
//    }
//    else {
//      int pos_in_seq = index_in_sequence(seq, current_sequence(), -1);
//      if(pos_in_seq < 0) // not inside seq any more
//        return;
//      
//      if(pos_in_seq >= pos) // inside nested sequence after pos
//        return;
//    }
//    move_next_glyph();
//  }
//  return;

  for(;;) {
    int next_run = glyph_count();
    int i;
    if(g2t_iter.find_next_run(i)   && i < next_run) next_run = i;
    if(g2seq_iter.find_next_run(i) && i < next_run) next_run = i;
    
    int run_length = next_run - glyph_index();
    ARRAY_ASSERT(run_length >= 0);
    if(run_length == 0)
      break;
    
    int delta = pos - index_in_sequence(seq, pos);
    if(delta < 0) {
      pmath_debug_print("[text pos %d skipped]\n", pos);
      break;
    }
    
    if(delta < run_length) {
      g2t_iter.increment(  delta);
      g2seq_iter.increment(delta);
      break;
    }
    g2t_iter.increment(  run_length);
    g2seq_iter.increment(run_length);
  }
  
  move_by_glyphs(0);
}

void GlyphIterator::move_token_end() {
  if(glyph_index() >= _owning_seq->glyph_array().length())
    return;
  
  skip_forward_to_glyph_after_current_text_pos(find_token_end());
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
  skip_forward_to_glyph_after_current_text_pos(span.end());
}

int GlyphIterator::index_in_sequence(MathSequence *parent, int fallback) {
  auto sub = current_sequence();
  if(parent == sub)
    return text_index();
  
  return Impl::index_in_sequence(parent, sub, fallback);
}

int GlyphIterator::find_token_end() const {
  int ti = text_index();
  while(ti < text_buffer_length() && !text_span_array().is_token_end(ti))
    ++ti;
  
  return ti;
}

int GlyphIterator::find_next_token() const {
  int ti = find_token_end();
  
  if(ti < text_buffer_length())
    ++ti;
    
  return ti;
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

//{ class GlyphIterator::Impl ...

int GlyphIterator::Impl::index_in_sequence(MathSequence *parent, Box *other, int fallback) {
  assert(parent);
  
  int index = fallback;
  while(other && other != parent) {
    index = other->index();
    other = other->parent();
  }
  
  if(other)
    return index;
  else
    return fallback;
}

//} ... class GlyphIterator::Impl
