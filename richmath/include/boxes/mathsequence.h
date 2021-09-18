#ifndef RICHMATH__BOXES__SEQUENCE_H__INCLUDED
#define RICHMATH__BOXES__SEQUENCE_H__INCLUDED

#include <boxes/box.h>
#include <util/rle-array.h>
#include <syntax/syntax-state.h>


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
  class MathSequence final : public AbstractSequence {
      using base = AbstractSequence;
      class Impl;
    protected:
      virtual ~MathSequence();
    public:
      MathSequence();
      
      virtual AbstractSequence *create_similar() override { return new MathSequence(); }
      
      virtual Box *item(int i) override;
      virtual int count() override {      return boxes.length(); }
      virtual int length() override {     return str.length(); }
      
      virtual String raw_substring(int start, int length) override;
      virtual uint32_t char_at(int pos) override; // return 0 on Out-Of-Range
      
      virtual float fill_weight() override;
      virtual bool expand(const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void colorize_scope(SyntaxState &state) override;
      virtual void paint(Context &context) override;
      
      virtual void selection_path(Canvas &canvas,   int start, int end) override;
      void         selection_path(Context &context, int start, int end);
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      virtual Expr to_pmath(BoxOutputFlags flags, int start, int end) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
      void select_nearby_placeholder(int *start, int *end, float *index_rel_x);
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
      virtual VolatileSelection normalize_selection(int start, int end) override;
      
      int find_string_start(int pos_inside_string, int *next_after_string = 0); // returns -1 on failure
      bool is_inside_string(int pos) { return find_string_start(pos) >= 0; }
      
      virtual void ensure_boxes_valid() override;
      void ensure_spans_valid();
      
      bool is_placeholder();
      virtual bool is_placeholder(int i) override;
      
      int matching_fence(int pos); // -1 on error
      
    public:
      virtual int insert(int pos, uint32_t chr) override;
      int insert(int pos, const uint16_t *ucs2, int len);
      int insert(int pos, const char *latin1, int len);
      virtual int insert(int pos, const String &s) override;
      virtual int insert(int pos, Box *box) override;
      virtual int insert(int pos, AbstractSequence *seq, int start, int end) override {
        return AbstractSequence::insert(pos, seq, start, end);
      }
      
      virtual void remove(int start, int end) override;
      virtual Box *remove(int *index) override;
      
      virtual Box *extract_box(int boxindex) override;
      
      virtual void load_from_object(Expr object, BoxInputFlags options) override;
      
      const String                        &text() {        return str;    }
      const SpanArray                     &span_array() {  return spans;  }
      const Array<Line>                   &line_array() {  return lines;  }
      const Array<GlyphInfo>              &glyph_array() { return glyphs; }
      RleArray<SyntaxGlyphStyle>          &semantic_styles_array() { return semantic_styles; }
      RleArrayIterator<const RleLinearPredictorArray<int>> glyph_to_text_iter() { return glyph_to_text.cbegin(); }
   
      bool stretch_horizontal(Context &context, float width);
      
      virtual int get_line(int index, int guide = 0) override; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) override;
      
      int get_box(int index, int guide = 0);
      float indention_width(int i);
      float font_size() { return em; }
      float line_spacing() { return 0.3f * em; }
    
    private:
      enum {
        BoxesInvalidBit = base::NumFlagsBits,
        SpansInvalidBit,
        AutoIndentBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool boxes_invalid() {       return get_flag(BoxesInvalidBit); }
      void boxes_invalid(bool value) { change_flag(BoxesInvalidBit, value); }
      bool spans_invalid() {       return get_flag(SpansInvalidBit); }
      void spans_invalid(bool value) { change_flag(SpansInvalidBit, value); }
      bool auto_indent() {         return get_flag(AutoIndentBit); }
      void auto_indent(bool value) {   change_flag(AutoIndentBit, value); }
      
    private:
      String           str;
      Array<Box *>     boxes;
      Array<GlyphInfo> glyphs;
      Array<Line>      lines;
      RleArray<SyntaxGlyphStyle>   semantic_styles; // uses text index, not glyph index
      RleLinearPredictorArray<int> glyph_to_text;
      SpanArray        spans;
  };
  
  
}

#endif // RICHMATH__BOXES__SEQUENCE_H__INCLUDED
