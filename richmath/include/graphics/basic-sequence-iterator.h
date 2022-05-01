#ifndef RICHMATH__BASIC_SEQUENCE_ITERATOR_H__INCLUDED
#define RICHMATH__BASIC_SEQUENCE_ITERATOR_H__INCLUDED

#include <boxes/box.h>
#include <util/selections.h>
#include <util/rle-array.h>

namespace richmath {
  template<typename Seq>
  class BasicSequenceIterator {
    public:
      using TextPositionIter = RleArrayIterator<const RleLinearPredictorArray<int>>;
      using InlineSeqIter    = RleArrayIterator<const RleArray<Seq*>>;
    public:
      explicit BasicSequenceIterator(Seq &seq, TextPositionIter text_pos_iter, InlineSeqIter inline_seq_iter)
        : _owning_seq{&seq}, _current_buf{nullptr}, _text_pos_iter{text_pos_iter}, _inline_seq_iter{inline_seq_iter}, _next_box_index{0}, _current_char{0} 
      {move_by_glyphs(0);}
      
      int glyph_index() const { return _text_pos_iter.index(); }
      int text_index() const {  return _text_pos_iter.get(); }
      
      Seq *outermost_sequence() const { return _owning_seq; }
      
      VolatileLocation current_location() const { return VolatileLocation(current_sequence(), text_index()); }
      Seq *current_sequence() const { auto seq = _inline_seq_iter.get(); return seq ? seq : _owning_seq; }
      
      const uint16_t            *text_buffer_raw() const {    return _current_buf; }
      int                        text_buffer_length() const { return current_sequence()->length(); }
      ArrayView<const uint16_t>  text_view() const {          return ArrayView<const uint16_t>(text_buffer_length(), text_buffer_raw()); }
      
      int index_in_sequence(Seq *other, int fallback, int glyph_count);
      
      uint16_t   current_char() const { return _current_char; }
      Box       *current_box() const;
      
      bool find_next_run(int &next_run_index);
      
      BasicSequenceIterator &operator++() { move_by_glyphs(1);  return *this; }
      BasicSequenceIterator &operator--() { move_by_glyphs(-1); return *this; }
      
      void rewind_to(int glyph_index);
      void move_by_glyphs(int delta);
      
      void skip_forward_to_glyph_after_text_pos(Seq *seq, int pos, int glyph_count);
      void skip_forward_to_glyph_after_current_text_pos(int pos, int glyph_count);
      
    private:
      Seq              *_owning_seq;
      const uint16_t   *_current_buf;
      TextPositionIter  _text_pos_iter;
      InlineSeqIter     _inline_seq_iter;
      mutable int       _next_box_index;
      uint16_t          _current_char;
  };
  
  template <typename Seq>
  bool BasicSequenceIterator<Seq>::find_next_run(int &next_run_index) {
    int min = INT_MAX;
    int i;
    bool found = false;
    
    if(_text_pos_iter.find_next_run(i)) {   found = true; if(i < min) min = i; }
    if(_inline_seq_iter.find_next_run(i)) { found = true; if(i < min) min = i; }
    
    if(found) next_run_index = min;
    
    return found;
  }
  
  template <typename Seq>
  void BasicSequenceIterator<Seq>::rewind_to(int glyph_index) {
    ARRAY_ASSERT(glyph_index >= 0);
    
    _text_pos_iter.rewind_to(  glyph_index);
    _inline_seq_iter.rewind_to(glyph_index);
    
    move_by_glyphs(0);
  }
  
  template <typename Seq>
  void BasicSequenceIterator<Seq>::move_by_glyphs(int delta) {
    ARRAY_ASSERT(delta >= 0 || glyph_index() + delta >= 0);
    
    //if(delta > _owning_seq->glyph_array().length() - glyph_index())
    //  delta = _owning_seq->glyph_array().length() - glyph_index();
    
    auto oldseq = current_sequence();
    
    _text_pos_iter+=   delta;
    _inline_seq_iter+= delta;
    
    auto newseq = _inline_seq_iter.get();
    if(!newseq)
      newseq = _owning_seq;
    
    if(newseq != oldseq || !_current_buf) {
      newseq->ensure_boxes_valid();
      _current_buf = newseq->text().buffer();
    }
    
    int ti = text_index();
    _current_char = (ti < text_buffer_length()) ? _current_buf[ti] : 0;
  }
  
  template <typename Seq>
  void BasicSequenceIterator<Seq>::skip_forward_to_glyph_after_text_pos(Seq *seq, int pos, int glyph_count) {
    ARRAY_ASSERT(0 <= pos);
    ARRAY_ASSERT(pos <= seq->length());
    ARRAY_ASSERT(!seq->DEBUG_boxes_invalid());
    ARRAY_ASSERT(!current_sequence()->DEBUG_boxes_invalid());
    
    // In the scematics below we use the following notation:
    //   [ and ]   begin and end of a sequence
    //   ooo       glyphs in the `_owning_seq`
    //   ccc       glyphs in the `current_sequence()` (before the move)
    //   ^         current position (`_text_index` inside `_current_seq`)
    //   sss       glyphs in the requested `seq`
    //   |         is requested position (`pos` inside `seq`)
    
    while(glyph_index() < glyph_count) {
      int pos_in_cur = (seq == current_sequence()) ? pos : seq->index_in_ancestor(current_sequence(), -1);
      if(pos_in_cur >= 0) { // [ccc[sss]ccc]
        if(text_index() >= pos_in_cur) // [ccc[ss|s]ccc^ccc]  or  [ccc[sss|]^ccc]
          return;
        
        // [cc^c[ss|s]ccc]
        skip_forward_to_glyph_after_current_text_pos(pos_in_cur, glyph_count);
        // [ccc[^ss|s]ccc]
        // or
        // [ccc[|]^ccc]
      }
      else {
        int pos_in_seq = (seq == current_sequence()) ? pos : current_sequence()->index_in_ancestor(seq, -1);
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
        
        int old_glyph_index = glyph_index();
        
        // [ooo[cc^c]ooo[ss|s]ooo]   or   [sss[c^cc]s|ss]
        skip_forward_to_glyph_after_current_text_pos(text_buffer_length(), glyph_count);
        // [ooo[ccc]^ooo[ss|s]ooo]   or   [sss[ccc]^s|ss]

        if(glyph_index() == old_glyph_index) {
          // should not happen, probably text_changed()
          break;
        }
      }
    }
  }

  template <typename Seq>
  void BasicSequenceIterator<Seq>::skip_forward_to_glyph_after_current_text_pos(int pos, int glyph_count) {
    Seq *seq = current_sequence();
    
    ARRAY_ASSERT(0 <= pos);
    ARRAY_ASSERT(pos <= seq->length());
    ARRAY_ASSERT(!seq->DEBUG_boxes_invalid());
    
    for(;;) {
      int next_run = glyph_count;
      int i;
      if(find_next_run(i) && i < next_run) next_run = i;
      
      int run_length = next_run - glyph_index();
      ARRAY_ASSERT(run_length >= 0);
      if(run_length == 0)
        break;
      
      int delta = pos - index_in_sequence(seq, pos, glyph_count);
      if(delta < 0) {
        pmath_debug_print("[text pos %d skipped]\n", pos);
        break;
      }
      
      if(delta == 0)
        break;
      
      _current_buf = nullptr; // to make move_by_glyphs(0) re-calculate buffer
      if(delta < run_length) {
        _text_pos_iter.increment(  delta);
        _inline_seq_iter.increment(delta);
        break;
      }
      _text_pos_iter.increment(  run_length);
      _inline_seq_iter.increment(run_length);
    }
    
    move_by_glyphs(0);
  }
  
  template <typename Seq>
  int BasicSequenceIterator<Seq>::index_in_sequence(Seq *other, int fallback, int glyph_count) {
    if(glyph_index() >= glyph_count)
      return fallback;
    
    auto sub = current_sequence();
    if(other == sub)
      return text_index();
    
    return sub->index_in_ancestor(other, fallback);
  }
  
  template <typename Seq>
  Box *BasicSequenceIterator<Seq>::current_box() const {
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
}

#endif // RICHMATH__BASIC_SEQUENCE_ITERATOR_H__INCLUDED
