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
      
      virtual Box *item(int i) { return boxes[i]; }
      virtual int count() {      return boxes.length(); }
      virtual int length() {     return text.length(); }
      
      virtual String raw_substring(int start, int length);
      virtual uint32_t char_at(int pos) { return text.char_at(pos); }
      virtual bool is_placeholder(int i);
      
      const TextBuffer &text_buffer() { return text; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual void selection_path(Canvas *canvas, int start, int end);
      
      virtual Expr to_pmath_symbol() { return Expr(); }
      virtual Expr to_pmath(int flags);
      virtual Expr to_pmath(int flags, int start, int end);
      virtual void load_from_object(Expr object, int options); // BoxOptionXXX
      
      virtual void ensure_boxes_valid();
      void ensure_text_valid();
      
      int insert(int pos, const char *utf8, int len);
      int insert(int pos, TextSequence *txt, int start, int end);
      virtual int insert(int pos, const String &s); // unsafe: allows PMATH_BOX_CHAR
      virtual int insert(int pos, Box *box);
      virtual int insert(int pos, AbstractSequence *seq, int start, int end);
      
      virtual void remove(int start, int end);
      virtual Box *remove(int *index);
      
      virtual Box *extract_box(int boxindex);
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index);
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child);
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix);
        
      virtual Box *normalize_selection(int *start, int *end);
      
      PangoLayoutIter *get_iter();
      PangoLayout     *get_layout() { return _layout; }
      virtual int get_line(int index, int guide = 0); // 0, 1, 2, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent);
      
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
