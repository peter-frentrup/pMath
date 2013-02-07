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
    public:
      MathSequence();
      virtual ~MathSequence();
      
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
        int              *index);
        
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
      
      bool is_inside_string(int pos);
      
      virtual void ensure_boxes_valid();
      void ensure_spans_valid();
      
      bool is_placeholder();
      virtual bool is_placeholder(int i);
      
      int matching_fence(int pos); // -1 on error
      
    protected:
      static pmath_bool_t subsuperscriptbox_at_index(int i, void *_data);
      static pmath_string_t underoverscriptbox_at_index(int i, void *_data);
      static void syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err);
      static pmath_t box_at_index(int i, void *_data);
      static pmath_t add_debug_info(pmath_t token_or_span, int start, int end, void *_data);
      
      void boxes_size(Context *context, int start, int end, float *a, float *d);
      void box_size(  Context *context, int pos, int box, float *a, float *d);
      void caret_size(Context *context, int pos, int box, float *a, float *d);
      
      void resize_span(
        Context *context,
        Span     span,
        int     *pos,
        int     *box);
      
      void stretch_span(
        Context *context,
        Span     span,
        int     *pos,
        int     *box,
        float   *core_ascent,
        float   *core_descent,
        float   *ascent,
        float   *descent);
      
      void apply_glyph_substitutions(Context *context);
        
      void substitute_glyphs(
        Context              *context,
        int                   start,
        int                   end,
        uint32_t              math_script_tag,
        uint32_t              math_language_tag,
        uint32_t              text_script_tag,
        uint32_t              text_language_tag,
        const FontFeatureSet &features);
        
      void group_number_digits(Context *context, int start, int end);
      void enlarge_space(Context *context);
      
      void split_lines(Context *context);
      
      void hstretch_lines(
        float width,
        float window_width,
        float *unfilled_width);
        
      int fill_penalty_array(
        Span  span,
        int   depth,
        int   pos,
        int  *box);
        
      int fill_indention_array(
        Span span,
        int  depth,
        int  pos);
        
      void new_line(int pos, unsigned int indent, bool continuation = false);
      
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
      Array<Box*>      boxes;
      String           str;
      SpanArray        spans;
      Array<GlyphInfo> glyphs;
      Array<Line>      lines;
      
      bool boxes_invalid;
      bool spans_invalid;
  };
  
  
}

#endif // __BOXES__SEQUENCE_H__
