#ifndef RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
#define RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED


#include <boxes/box.h>

namespace richmath {
  class AbstractSequence: public Box {
    protected:
      virtual ~AbstractSequence();
    public:
      AbstractSequence();
      
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
}

#endif // RICHMATH__BOXES__ABSTRACTSEQUENCE_H__INCLUDED
