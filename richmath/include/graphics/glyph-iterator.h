#ifndef RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
#define RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED

#include <boxes/mathsequence.h>

namespace richmath {  
  class GlyphIterator {
    public:
      int glyph_index() const { return _glyph_index; }
      int text_index() const { return _text_index; }
      
      bool has_more_glyphs() const { return _glyph_index < _owning_seq->glyph_array().length(); }
      bool is_operand_start() const { return has_more_glyphs() && text_span_array().is_operand_start(text_index()); }
      
      uint16_t   current_char() const { return _current_char; }
      Box       *current_box() const;
      GlyphInfo &current_glyph() const { return _owning_seq->glyph_array()[glyph_index()]; }
      
      MathSequence *current_sequence() const { return _current_seq; }
      const uint16_t *text_buffer() const { return _current_buf; }
      const SpanArray &text_span_array() const { return _current_seq->span_array(); }
      
      int index_in_sequence(MathSequence *parent);
      
      int text_buffer_length() const { return _current_seq->length(); }
      
      const uint16_t *text_at_glyph() const { return text_buffer() + text_index(); }
      const uint16_t *text_end() const { return text_buffer() + text_buffer_length(); }
      
    public:
      explicit GlyphIterator(MathSequence &seq);
      
      void move_next_glyph();
      void skip_to_glyph_after_text_pos(MathSequence *seq, int pos);
      void skip_to_glyph_after_current_text_pos(int pos);
      void move_token_end();
      void move_next_token();
      void move_deepest_span_end();
      
    private:
      static int index_in_sequence(MathSequence *parent, Box *other);
      
    private:
      MathSequence *_owning_seq;
      MathSequence *_current_seq;
      const uint16_t *_current_buf;
      int _glyph_index;
      int _text_index;
      mutable int _next_box_index;
      uint16_t _current_char;
  };
  
  inline GlyphIterator::GlyphIterator(MathSequence &seq)
  : _owning_seq{&seq},
    _current_seq{nullptr},
    _current_buf{nullptr},
    _glyph_index{-1},
    _text_index{-1},
    _next_box_index{0},
    _current_char{0}
  {
    move_next_glyph();
  }

  inline void GlyphIterator::move_next_token() {
    move_token_end();
    move_next_glyph();
  }
}

#endif // RICHMATH__GRAPHICS__GLYPH_ITERATOR_H__INCLUDED
