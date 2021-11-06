#include <graphics/text-layout-iterator.h>

#include <boxes/textsequence.h>


using namespace richmath;

static bool is_utf8_first_byte(char c) {
  return ((unsigned char)c & 0xC0) != 0x80;
}

//{ class TextLayoutIterator ...

TextLayoutIterator::TextLayoutIterator(TextSequence &seq)
  : _basic_iter{seq, seq.buffer_to_text_iter(), seq.buffer_to_inline_sequence_iter()},
    _buffer{pango_layout_get_text(seq.get_layout())},
    _buffer_size{seq.buffer_size()},
    _attr_index{0}
{
  _attrs = pango_layout_get_log_attrs_readonly(seq.get_layout(), &_num_attrs);
}

int TextLayoutIterator::find_current_line_x(bool trailing, int *pango_x_coord) {
  PangoLayout *layout = outermost_sequence()->get_layout();
  
  int line;
  pango_layout_index_to_line_x(layout, byte_index(), trailing, &line, pango_x_coord);
  return line;
}

void TextLayoutIterator::rewind_to_byte(int new_byte_index) {
  ARRAY_ASSERT(0 <= new_byte_index && new_byte_index <= byte_count());
  
  //_basic_iter.rewind_to(new_byte_index);
  //
  //_attr_index = 0;
  //for(int i = 0; i < new_byte_index; ++i)
  //  if(is_utf8_first_byte(_buffer[i]))
  //    ++_attr_index;
  
  move_by_bytes(new_byte_index - byte_index());
}

void TextLayoutIterator::move_by_bytes(int delta) {
  ARRAY_ASSERT(delta >= 0 || byte_index() + delta >= 0);
  
  if(delta == 0)
    return;
  
  if(delta > byte_count() - byte_index())
    delta = byte_count() - byte_index();
  
  const char *s = _buffer + byte_index();
  
  _basic_iter.move_by_glyphs(delta);
  
  if(delta >= 0) {
    for(int i = 0; i < delta; ++i) {
      if(is_utf8_first_byte(s[i]))
        ++_attr_index;
    }
  }
  else {
    for(int i = 0; i > delta; --i) {
      if(is_utf8_first_byte(s[i]))
        --_attr_index;
    }
  }
}

void TextLayoutIterator::move_next_char() {
  if(!has_more_bytes())
    return;
  
  ++_basic_iter;
  while(has_more_bytes() && !is_utf8_first_byte(_buffer[byte_index()]))
    ++_basic_iter;
  
  ++_attr_index;
}

void TextLayoutIterator::move_previous_char() {
  if(byte_index() == 0)
    return;
  
  --_basic_iter;
  while(byte_index() > 0 && !is_utf8_first_byte(_buffer[byte_index()]))
    --_basic_iter;
  
  --_attr_index;
}

void TextLayoutIterator::skip_forward_beyond_text_pos(TextSequence *seq, int pos) {
  int i = byte_index();
  
  _basic_iter.skip_forward_to_glyph_after_text_pos(seq, pos, byte_count());

  for(; i < byte_index(); ++i) {
    if(is_utf8_first_byte(_buffer[i]))
      ++_attr_index;
  }
}

//} ... class TextLayoutIterator
