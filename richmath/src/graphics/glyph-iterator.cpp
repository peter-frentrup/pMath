#include <graphics/glyph-iterator.h>


using namespace richmath;

//{ class GlyphIterator ...

GlyphIterator::GlyphIterator(MathSequence &seq)
  : _basic_iter{seq, seq.glyph_to_text_iter(), seq.glyph_to_inline_sequence_iter()},
    _semantic_style_iter{seq.semantic_styles_array().begin()}
{
}

void GlyphIterator::move_by_glyphs(int delta) {
  ARRAY_ASSERT(delta >= 0 || glyph_index() + delta >= 0);
  
  if(delta > glyph_count() - glyph_index())
    delta = glyph_count() - glyph_index();
  
  _basic_iter.move_by_glyphs(delta);
}

void GlyphIterator::move_token_end() {
  if(!has_more_glyphs())
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

//} ... class GlyphIterator
