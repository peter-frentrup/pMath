#ifndef RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
#define RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED


#include <boxes/box.h>

namespace richmath {
  class MathSequence;
  class TextSequence;
  
  enum class LayoutKind {
    Math,
    Text
  };
  
  class AbstractSequence: public Box {
      using base = Box;
    private: // No other subclasses possible:
      friend class MathSequence;
      friend class TextSequence;
      virtual ~AbstractSequence();
    public:
      explicit AbstractSequence();
      
      virtual AbstractSequence *create_similar() = 0;
      
      virtual LayoutKind kind() = 0;
      
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      virtual void load_from_object(Expr object, BoxInputFlags options) = 0;
      
      virtual Box *item(int i) final override;
      virtual int count() final override {  return boxes.length(); }
      virtual int length() final override { return str.length(); }
      
      String raw_substring(int start, int length);
      uint32_t char_at(int pos); // return 0 on Out-Of-Range
      
      void ensure_boxes_valid();
      
      virtual VolatileSelection normalize_selection(int start, int end) final override;
      
      bool is_placeholder();
      bool is_placeholder(int i);
      virtual bool is_word_boundary(int i) = 0;
      
      int insert(int pos, uint32_t chr);
      int insert(int pos, const uint16_t *ucs2, int len);
      int insert(int pos, const char *latin1, int len);
      int insert(int pos, const String &s);
      int insert(int pos, Box *box);
      int insert(int pos, AbstractSequence *seq, int start, int end);
      
      void remove(int start, int end);
      virtual Box *remove(int *index) final override;
      
      Box *extract_box(int boxindex);
      int get_box(int index, int guide = 0);
      
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      BoxSize &var_extents() { return _extents; }
      
      virtual int get_line(int index, int guide = 0) = 0; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) = 0;
      
      virtual bool request_repaint_range(int start, int end) override;
      virtual RectangleF range_rect(int start, int end) override;
      
      float get_em() { return em; }
      const String &text() { return str; }
      
#ifndef NDEBUG
      bool DEBUG_boxes_invalid() { return boxes_invalid(); }
#endif
    protected:
      enum {
        BoxesInvalidBit = base::NumFlagsBits,
        TextChangedBit, // TODO: text_changed must be propagated to outer sequence
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool boxes_invalid() {       return get_flag(BoxesInvalidBit); }
      void boxes_invalid(bool value) { change_flag(BoxesInvalidBit, value); }
      bool text_changed() {        return get_flag(TextChangedBit); }
      void text_changed(bool value);
      
      virtual void on_text_changed() {}
      
    protected:
      String                 str;
      Array<Box *>           boxes;
      ObservableValue<float> em;
  };
  
  inline void AbstractSequence::text_changed(bool value) { 
    if(value) {
      if(!get_flag(TextChangedBit)) {
        set_flag(TextChangedBit); 
        on_text_changed(); 
      }
    }
    else {
      clear_flag(TextChangedBit);
    } 
  }
}

#endif // RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
