#ifndef RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
#define RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED


#include <boxes/box.h>

namespace richmath {
  class AbstractSequence: public Box {
    protected:
      virtual ~AbstractSequence();
    public:
      explicit AbstractSequence();
      
      virtual AbstractSequence *create_similar() = 0;
      
      virtual String raw_substring(int start, int length) = 0;
      virtual uint32_t char_at(int pos) = 0; // return 0 on Out-Of-Range
      virtual bool is_placeholder(int i) = 0;
      
      virtual void ensure_boxes_valid() = 0;
      
      virtual int insert(int pos, uint32_t chr) { return insert(pos, String::FromChar(chr)); }
      virtual int insert(int pos, AbstractSequence *seq, int start, int end);
      
      virtual int insert(int pos, const String &s) = 0;
      virtual int insert(int pos, Box *box) = 0;
      virtual void remove(int start, int end) = 0;
      
      virtual Box *extract_box(int boxindex) = 0;
      virtual bool try_load_from_object(Expr object, BoxInputFlags options) override;
      virtual void load_from_object(Expr object, BoxInputFlags options) = 0;
      
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      
      BoxSize &var_extents() { return _extents; }
      
      float get_em() { return em; }
      virtual int get_line(int index, int guide = 0) = 0; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) = 0;
      
      virtual bool request_repaint_range(int start, int end) override;
      
    protected:
      ObservableValue<float> em;
  };
  
  // TODO: merge with AbstractSequence as soon as TextSequence switches to UTF-16 via BasicSequence (issue #122)
  class BasicSequence: public AbstractSequence {
      using base = AbstractSequence;
    protected:
      virtual ~BasicSequence();
    public:
      explicit BasicSequence();
      
      virtual Box *item(int i) final override;
      virtual int count() final override {  return boxes.length(); }
      virtual int length() final override { return str.length(); }
      
      virtual String raw_substring(int start, int length) final override;
      virtual uint32_t char_at(int pos) final override; // return 0 on Out-Of-Range
      
      virtual void ensure_boxes_valid() final override;
      
      bool is_placeholder();
      virtual bool is_placeholder(int i) final override;
      
      virtual int insert(int pos, uint32_t chr) final override;
      int insert(int pos, const uint16_t *ucs2, int len);
      int insert(int pos, const char *latin1, int len);
      virtual int insert(int pos, const String &s) final override;
      virtual int insert(int pos, Box *box) final override;
      virtual int insert(int pos, AbstractSequence *seq, int start, int end) final override {
        return base::insert(pos, seq, start, end);
      }
      
      virtual void remove(int start, int end) final override;
      virtual Box *remove(int *index) final override;
      
      virtual Box *extract_box(int boxindex) final override;
      int get_box(int index, int guide = 0);
      
      const String &text() { return str; }
      
    protected:
      enum {
        BoxesInvalidBit = base::NumFlagsBits,
        SpansInvalidBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool boxes_invalid() {       return get_flag(BoxesInvalidBit); }
      void boxes_invalid(bool value) { change_flag(BoxesInvalidBit, value); }
      bool spans_invalid() {       return get_flag(SpansInvalidBit); }
      void spans_invalid(bool value) { change_flag(SpansInvalidBit, value); }
      
    protected:
      String       str;
      Array<Box *> boxes;
  };
}

#endif // RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
