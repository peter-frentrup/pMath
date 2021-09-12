#ifndef RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
#define RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED

#include <boxes/mathsequence.h>

namespace richmath {  
  class GlyphIterator {
      class Impl;
    public:
      using style_iter_t = RleArray<SyntaxGlyphStyle>::iterator_type;
      
    public:
      int glyph_index() const { return g2t_iter.index(); }
      int text_index() const { return g2t_iter.get(); }
      
      // caution: methods may invalidate later iterators.
      style_iter_t semantic_style_iter() const { _semantic_style_iter.rewind_to(text_index()); return _semantic_style_iter; }
      
      bool has_more_glyphs() const { return glyph_index() < glyph_count(); }
      bool is_operand_start() const { return has_more_glyphs() && text_span_array().is_operand_start(text_index()); }
      bool is_at_token_end() const {  return has_more_glyphs() && text_span_array().is_token_end(text_index()); }
      
      int glyph_count() const { return _owning_seq->glyph_array().length(); }
      
      uint16_t   current_char() const { return _current_char; }
      Box       *current_box() const;
      GlyphInfo &current_glyph() const { return _owning_seq->glyph_array()[glyph_index()]; }
      
      MathSequence *outermost_sequence() const { return _owning_seq; }
      
      MathSequence *current_sequence() const { return _current_seq; }
      const uint16_t *text_buffer() const { return _current_buf; }
      const SpanArray &text_span_array() const { return _current_seq->span_array(); }
      
      int index_in_sequence(MathSequence *parent);
      
      int text_buffer_length() const { return _current_seq->length(); }
      
      const uint16_t *text_at_glyph() const { return text_buffer() + text_index(); }
      const uint16_t *text_end() const { return text_buffer() + text_buffer_length(); }
      
      ArrayView<const uint16_t> find_token() const { return ArrayView<const uint16_t>{find_next_token() - text_index(), text_at_glyph()}; }
      
      int find_token_end() const;
      int find_next_token() const;
      
    public:
      explicit GlyphIterator(MathSequence &seq);
      
      void move_next_glyph() { skip_glyphs(1); }
      void skip_glyphs(int count);
      void skip_to_glyph(int new_glyph_index) { skip_glyphs(new_glyph_index - glyph_index()); }
      void skip_to_glyph_after_text_pos(MathSequence *seq, int pos);
      void skip_to_glyph_after_current_text_pos(int pos);
      void move_token_end();
      void move_next_token();
      void move_deepest_span_end();
      
    private:
      MathSequence *_owning_seq;
      MathSequence *_current_seq;
      const uint16_t *_current_buf;
      RleArrayIterator<const RleLinearPredictorArray<int>> g2t_iter;
      mutable style_iter_t  _semantic_style_iter;
      mutable int _next_box_index;
      uint16_t _current_char;
  };

  inline void GlyphIterator::move_next_token() {
    move_token_end();
    move_next_glyph();
  }
}

#endif // RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
