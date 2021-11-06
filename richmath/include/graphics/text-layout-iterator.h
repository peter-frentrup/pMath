#ifndef RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED
#define RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED

#include <graphics/basic-sequence-iterator.h>

#include <pango/pango-break.h>


namespace richmath {
  class TextSequence;
  
  class TextLayoutIterator {
    public:
      int byte_index() const { return _basic_iter.glyph_index(); }
      int text_index() const { return _basic_iter.text_index(); }
      
      bool has_more_bytes() const { return byte_index() < byte_count(); }
      bool is_cursor_position() const { return current_attr().is_cursor_position; }
      bool is_word_start()      const { return current_attr().is_word_start; }
      bool is_word_end()        const { return current_attr().is_word_end; }
      bool is_word_boundary()   const { return current_attr().is_word_boundary; }
      bool is_whitespace()      const { return current_attr().is_white; }
      
      int byte_count() const { return _buffer_size; }
      
      TextSequence       *current_sequence() const { return _basic_iter.current_sequence(); }
      uint16_t            current_char() const {     return _basic_iter.current_char(); }
      Box                *current_box() const {      return _basic_iter.current_box(); }
      const PangoLogAttr &current_attr() const { ARRAY_ASSERT(0 <= _attr_index && _attr_index < _num_attrs); return _attrs[_attr_index]; }
      
      int find_current_line_x(bool trailing, int *pango_x_coord);
      int find_current_line(bool trailing) { int _; return find_current_line_x(trailing, &_); }
      
      TextSequence *outermost_sequence() const { return _basic_iter.outermost_sequence(); }
      
      const uint16_t            *text_buffer_raw()    const { return _basic_iter.text_buffer_raw(); }
      int                        text_buffer_length() const { return _basic_iter.text_buffer_length(); }
      ArrayView<const uint16_t>  text_view() const {          return _basic_iter.text_view(); }
      
      int index_in_sequence(TextSequence *other, int fallback) { return _basic_iter.index_in_sequence(other, fallback, byte_count()); }
      
    public:
      explicit TextLayoutIterator(TextSequence &seq);
      
      void rewind_to_byte(int new_byte_index);
      void move_by_bytes(int delta);
      void move_next_char();
      void move_previous_char();
      
      void skip_forward_beyond_text_pos(TextSequence *seq, int pos);
      
    private:
      BasicSequenceIterator<TextSequence>  _basic_iter;
      const char                          *_buffer;
      const PangoLogAttr                  *_attrs;
      int                                  _buffer_size;
      int                                  _attr_index;
      int                                  _num_attrs;
  };
}

#endif // RICHMATH__GRAPHICS__TEXT_LAYOUT_ITERATOR_H__INCLUDED
