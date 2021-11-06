#ifndef RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
#define RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED

#include <graphics/basic-sequence-iterator.h>
#include <boxes/mathsequence.h>

namespace richmath {
  class GlyphIterator {
    public:
      using style_iter_t = RleArray<SyntaxGlyphStyle>::iterator_type;
      
    public:
      int glyph_index() const { return _basic_iter.glyph_index(); }
      int text_index() const {  return _basic_iter.text_index(); }
      
      // caution: methods may invalidate later iterators.
      style_iter_t semantic_style_iter() const { _semantic_style_iter.rewind_to(text_index()); return _semantic_style_iter; }
      SyntaxGlyphStyle semantic_style() const { return semantic_style_iter().get(); }
      
      bool has_more_glyphs() const { return glyph_index() < glyph_count(); }
      bool is_operand_start() const { return has_more_glyphs() && text_span_array().is_operand_start(text_index()); }
      bool is_at_token_end() const {  return has_more_glyphs() && text_span_array().is_token_end(text_index()); }
      
      int glyph_count() const { return _basic_iter.outermost_sequence()->glyph_array().length(); }
      
      MathSequence *current_sequence() const { return _basic_iter.current_sequence(); }
      uint16_t      current_char() const {     return _basic_iter.current_char(); }
      Box          *current_box() const {      return _basic_iter.current_box(); }
      GlyphInfo    &current_glyph() const {    return all_glyphs()[glyph_index()]; }
      
      const Array<GlyphInfo> &all_glyphs() const { return _basic_iter.outermost_sequence()->glyph_array(); };
      MathSequence *outermost_sequence() const { return _basic_iter.outermost_sequence(); }
      
      const uint16_t            *text_buffer_raw()    const { return _basic_iter.text_buffer_raw(); }
      int                        text_buffer_length() const { return _basic_iter.text_buffer_length(); }
      ArrayView<const uint16_t>  text_view() const {          return _basic_iter.text_view(); }
      
      const SpanArray &text_span_array() const { return current_sequence()->span_array(); }
      
      int index_in_sequence(MathSequence *other, int fallback) { return _basic_iter.index_in_sequence(other, fallback, glyph_count()); }
      
      const uint16_t *text_at_glyph() const { return text_buffer_raw() + text_index(); }
      const uint16_t *text_end() const { return text_buffer_raw() + text_buffer_length(); }
      
      ArrayView<const uint16_t> find_token() const { return ArrayView<const uint16_t>{find_next_token() - text_index(), text_at_glyph()}; }
      
      int find_token_end() const;
      int find_next_token() const;
      
    public:
      explicit GlyphIterator(MathSequence &seq);
      
      void move_next_glyph() { move_by_glyphs(1); }
      void move_by_glyphs(int delta);
      void move_to_glyph(int new_glyph_index) { move_by_glyphs(new_glyph_index - glyph_index()); }
      void skip_forward_to_glyph_after_text_pos(MathSequence *seq, int pos) { _basic_iter.skip_forward_to_glyph_after_text_pos(seq, pos, glyph_count()); }
      void skip_forward_to_glyph_after_current_text_pos(int pos) {            _basic_iter.skip_forward_to_glyph_after_current_text_pos(pos, glyph_count()); }
      void move_token_end();
      void move_next_token();
      void move_deepest_span_end();
      
    private:
      BasicSequenceIterator<MathSequence> _basic_iter;
      mutable style_iter_t                _semantic_style_iter;
  };
  
  inline void GlyphIterator::move_next_token() {
    move_token_end();
    move_next_glyph();
  }
}

#endif // RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
