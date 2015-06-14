#ifndef __BOXES__SEQUENCE_H__
#define __BOXES__SEQUENCE_H__

#include <boxes/box.h>
#include <util/array.h>
#include <util/syntax-state.h>


namespace richmath {
  class FontFeatureSet;
  class SpanExpr;
  
  class Line {
    public:
      int end; // after last character of the line
      float ascent;
      float descent;
      unsigned indent: 31;
      unsigned continuation: 1;
  };
  
  /* This is a box containing math.
     For normal text, use class TextSequence.
   */
  class MathSequence: public AbstractSequence {
    friend class MathSequenceImpl;
    public:
      MathSequence();
      virtual ~MathSequence();
      
      virtual AbstractSequence *create_similar() { return new MathSequence(); }
      
      virtual Box *item(int i);
      virtual int count() {      return boxes.length(); }
      virtual int length() {     return str.length(); }
      
      virtual String raw_substring(int start, int length);
      virtual uint32_t char_at(int pos); // return 0 on Out-Of-Range
      
      virtual bool expand(const BoxSize &size);
      virtual void resize(Context *context);
      virtual void colorize_scope(SyntaxState *state);
      virtual void paint(Context *context);
      
      virtual void selection_path(Canvas *canvas, int start, int end);
      void selection_path(Context *opt_context, Canvas *canvas, int start, int end);
      
      virtual Expr to_pmath_symbol() { return Expr(); }
      virtual Expr to_pmath(int flags);
      virtual Expr to_pmath(int flags, int start, int end);
      
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
      
      int find_string_start(int pos_inside_string, int *next_afer_string = 0); // returns -1 on failure
      bool is_inside_string(int pos) { return find_string_start(pos) >= 0; }
      
      virtual void ensure_boxes_valid();
      void ensure_spans_valid();
      
      bool is_placeholder();
      virtual bool is_placeholder(int i);
      
      int matching_fence(int pos); // -1 on error
      
    public:
      int insert(int pos, uint16_t chr);                  // unsafe, allows PMATH_BOX_CHAR
      int insert(int pos, const uint16_t *ucs2, int len); // unsafe, allows PMATH_BOX_CHAR
      int insert(int pos, const char *latin1, int len);   // unsafe, allows PMATH_BOX_CHAR
      virtual int insert(int pos, const String &s);       // unsafe, allows PMATH_BOX_CHAR
      virtual int insert(int pos, Box *box);
      virtual int insert(int pos, AbstractSequence *seq, int start, int end) {
        return AbstractSequence::insert(pos, seq, start, end);
      }
      
      virtual void remove(int start, int end);
      virtual Box *remove(int *index);
      
      virtual Box *extract_box(int boxindex);
      
      virtual void load_from_object(Expr object, int options); // BoxOptionXXX
      
      const String           &text() {        return str;    }
      const SpanArray        &span_array() {  return spans;  }
      const Array<Line>      &line_array() {  return lines;  }
      const Array<GlyphInfo> &glyph_array() { return glyphs; }
      
      bool stretch_horizontal(Context *context, float width);
      
      virtual int get_line(int index, int guide = 0); // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent);
      
      int get_box(int index, int guide = 0);
      float indention_width(int i);
      float font_size() { return em; }
      float line_spacing() { return 0.3f * em; }
      
    private:
      Array<Box *>     boxes;
      String           str;
      SpanArray        spans;
      Array<GlyphInfo> glyphs;
      Array<Line>      lines;
      
      bool boxes_invalid;
      bool spans_invalid;
  };
  
  
}

#endif // __BOXES__SEQUENCE_H__
