#ifndef RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED
#define RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED

#include <graphics/basic-sequence-iterator.h>

#include <pango/pango-break.h>


namespace richmath {
  class TextSequence;
  
  class TextLayoutIterator {
    public:
      int attribute_index() const { return _basic_iter.glyph_index(); }
      int text_index() const {      return _basic_iter.text_index(); }
      
      bool has_more_attributes() const { return attribute_index() < attr_count(); }
      bool is_cursor_position() const { ARRAY_ASSERT(has_more_attributes()); return current_attr().is_cursor_position; }
      bool is_word_start()      const { ARRAY_ASSERT(has_more_attributes()); return current_attr().is_word_start; }
      bool is_word_end()        const { ARRAY_ASSERT(has_more_attributes()); return current_attr().is_word_end; }
      bool is_word_boundary()   const { ARRAY_ASSERT(has_more_attributes()); return current_attr().is_word_boundary; }
      bool is_whitespace()      const { ARRAY_ASSERT(has_more_attributes()); return current_attr().is_white; }
      
      int attr_count() const { return _num_attrs; }
      
      TextSequence       *current_sequence() const { return _basic_iter.current_sequence(); }
      uint16_t            current_char() const {     return _basic_iter.current_char(); }
      Box                *current_box() const {      return _basic_iter.current_box(); }
      const PangoLogAttr &current_attr() const { return _attrs[attribute_index()]; }
      
      int find_current_line_x(bool trailing, int *pango_x_coord);
      int find_current_line(bool trailing) { int _; return find_current_line_x(trailing, &_); }
      
      const PangoLogAttr *all_attributes() const { return _attrs; }
      TextSequence *outermost_sequence() const { return _basic_iter.outermost_sequence(); }
      
      const uint16_t            *text_buffer_raw()    const { return _basic_iter.text_buffer_raw(); }
      int                        text_buffer_length() const { return _basic_iter.text_buffer_length(); }
      ArrayView<const uint16_t>  text_view() const {          return _basic_iter.text_view(); }
      
      int index_in_sequence(TextSequence *other, int fallback) { return _basic_iter.index_in_sequence(other, fallback, attr_count()); }
      
    public:
      explicit TextLayoutIterator(TextSequence &seq);
      
      TextLayoutIterator &operator+=(int delta) { move_by(delta);  return *this; }
      TextLayoutIterator &operator-=(int delta) { move_by(-delta); return *this; }
      TextLayoutIterator &operator++() { return *this+= 1; }
      TextLayoutIterator &operator--() { return *this-= 1; }
      
      void rewind_to(int attr_index);
      void move_by(int delta);
      void move_next_char();
      
      void skip_forward_beyond_text_pos(TextSequence *seq, int pos) { _basic_iter.skip_forward_to_glyph_after_text_pos(seq, pos, attr_count()); }
      
    private:
      BasicSequenceIterator<TextSequence>  _basic_iter;
      const PangoLogAttr                  *_attrs;
      int                                  _num_attrs;
  };
}

#endif // RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED
