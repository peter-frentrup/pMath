#ifndef __BOXES__TEXTBOX_H__
#define __BOXES__TEXTBOX_H__

#include <pango/pangocairo.h>

#include <boxes/box.h>


namespace richmath {

  /* A character buffer (Utf-8) with pmath_mem_xxx memory handling. */
  class TextBuffer: public Base {
    public:
      TextBuffer(char *buf, int len);
      ~TextBuffer();
      
      int capacity() const {       return _capacity; }
      int length()   const {       return _length; }
      const char *buffer() const { return _buffer; }
      char       *buffer() {       return _buffer; }
      
      uint32_t char_at(int pos);
      
      // return number of bytes inserted at pos
      int insert(int pos, const char *ins, int inslen);
      int insert(int pos, const String &s);
      void remove(int pos, int len);
      
      bool is_box_at(int i);
      
    private:
      int _capacity;
      int _length;
      char *_buffer;
  };
  
  /* This is a box containing text (no math) and other boxes.
     It uses Pango for text layout. For math, use class MathSequence.
   */
  class TextSequence: public AbstractSequence {
    public:
      TextSequence();
      virtual ~TextSequence();
      
      virtual AbstractSequence *create_similar() override { return new TextSequence(); }
      
      virtual Box *item(int i) override { return boxes[i]; }
      virtual int count() override {      return boxes.length(); }
      virtual int length() override {     return text.length(); }
      
      virtual String raw_substring(int start, int length) override;
      virtual uint32_t char_at(int pos) override { return text.char_at(pos); }
      virtual bool is_placeholder(int i) override;
      
      const TextBuffer &text_buffer() { return text; }
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual void selection_path(Canvas *canvas, int start, int end) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxFlags flags) override;
      virtual Expr to_pmath(BoxFlags flags, int start, int end) override;
      virtual void load_from_object(Expr object, int options) override; // BoxOptionXXX
      
      virtual void ensure_boxes_valid() override;
      void ensure_text_valid();
      
      int insert(int pos, const char *utf8, int len);
      int insert(int pos, TextSequence *txt, int start, int end);
      virtual int insert(int pos, const String &s) override; // unsafe: allows PMATH_BOX_CHAR
      virtual int insert(int pos, Box *box) override;
      virtual int insert(int pos, AbstractSequence *seq, int start, int end) override;
      
      virtual void remove(int start, int end) override;
      virtual Box *remove(int *index) override;
      
      virtual Box *extract_box(int boxindex) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual Box *normalize_selection(int *start, int *end) override;
      
      PangoLayoutIter *get_iter();
      PangoLayout     *get_layout() { return _layout; }
      virtual int get_line(int index, int guide = 0) override; // 0, 1, 2, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) override;
      
      void line_extents(PangoLayoutIter *iter, int line, float *x, float *y, BoxSize *size);
      void line_extents(int line, float *x, float *y, BoxSize *size);
      
    private:
      Array<Box*>  boxes;
      TextBuffer   text;
      Array<int>   line_y_corrections;
      
      PangoLayout *_layout;
      
      bool boxes_invalid;
      bool text_invalid;
  };
}

#endif // __BOXES__TEXTBOX_H__
