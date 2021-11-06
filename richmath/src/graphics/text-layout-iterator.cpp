#include <graphics/text-layout-iterator.h>

#include <boxes/textsequence.h>


using namespace richmath;

static bool is_utf8_first_byte(char c) {
  return ((unsigned char)c & 0xC0) != 0x80;
}

//{ class TextLayoutIterator ...

TextLayoutIterator::TextLayoutIterator(TextSequence &seq)
  : _basic_iter{seq, seq.buffer_to_text_iter(), seq.buffer_to_inline_sequence_iter()}
{
  _attrs = pango_layout_get_log_attrs_readonly(seq.get_layout(), &_num_attrs);
}

int TextLayoutIterator::find_current_line_x(bool trailing, int *pango_x_coord) {
  PangoLayout *layout = outermost_sequence()->get_layout();
  
  int line;
  pango_layout_index_to_line_x(layout, attribute_index(), trailing, &line, pango_x_coord);
  return line;
}

void TextLayoutIterator::rewind_to(int attr_index) {
  ARRAY_ASSERT(0 <= attr_index && attr_index <= attr_count());
  
  _basic_iter.rewind_to(attr_index);
}

void TextLayoutIterator::move_by(int delta) {
  ARRAY_ASSERT(delta >= 0 || attribute_index() + delta >= 0);
  
  if(delta > attr_count() - attribute_index())
    delta = attr_count() - attribute_index();
  
  _basic_iter.move_by_glyphs(delta);
}

void TextLayoutIterator::move_next_char() {
  if(!has_more_attributes())
    return;
  
  const char *str = pango_layout_get_text(outermost_sequence()->get_layout());
  
  ++_basic_iter;
  while(has_more_attributes() && !is_utf8_first_byte(str[attribute_index()]))
    ++_basic_iter;
}

//} ... class TextLayoutIterator
