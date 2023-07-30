#ifndef RICHMATH__BOXES__SEQUENCE_H__INCLUDED
#define RICHMATH__BOXES__SEQUENCE_H__INCLUDED

#include <boxes/abstractsequence.h>
#include <util/rle-array.h>
#include <graphics/syntax-styles.h>
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
    public:
      explicit MathSequence();
      
      virtual LayoutKind kind() override { return LayoutKind::Math; }
      static AbstractSequence *create() { return new MathSequence(); }
      virtual AbstractSequence *create_similar() override { return create(); }
      
      virtual float fill_weight() override;
      virtual bool expand(Context &context, const BoxSize &size) override;
      virtual void resize(Context &context) override;
      virtual void colorize_scope(SyntaxState &state) override;
      virtual void before_paint_inline(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual void selection_path(Canvas &canvas,   int start, int end) override;
      void         selection_path(Context &context, int start, int end);
      virtual void selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, int start, int end) override;

      virtual Expr to_pmath_symbol() override { return Expr(); }
      
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
        
      virtual bool request_repaint_all() override;
      virtual bool request_repaint_range(int start, int end) override;
      virtual bool request_repaint(const RectangleF &rect) override;
      virtual RectangleF range_rect(int start, int end) override;
      virtual bool visible_rect(RectangleF &rect, Box *top_most) override;
      
      int find_string_start(int pos_inside_string, int *next_after_string = nullptr); // returns -1 on failure
      bool is_inside_string(int pos) { return find_string_start(pos) >= 0; }
      
      void ensure_spans_valid();
      virtual bool is_word_boundary(int i) override;
      
      int matching_fence(int pos); // -1 on error
      
      virtual void load_from_object(Expr object, BoxInputFlags options) override;
      
      const SpanArray                     &span_array() {  return spans;  }
      const Array<Line>                   &line_array() {  return lines;  }
      const Array<GlyphInfo>              &glyph_array() { return glyphs; }
      SyntaxGlyphStylesArray              &semantic_styles_array() { return semantic_styles; }
      RleArrayIterator<const RleLinearPredictorArray<int>> glyph_to_text_iter() {            return glyph_to_text.cbegin(); }
      RleArrayIterator<const RleArray<MathSequence*>>      glyph_to_inline_sequence_iter() { return glyph_to_inline_sequence.cbegin(); }
   
      bool stretch_horizontal(Context &context, float width);
      virtual float first_glyph_width() override;
      virtual float last_glyph_width() override;
      
      virtual int get_line(int index, int guide = 0) override; // 0, 1, ...
      
      virtual void get_line_heights(int line, float *ascent, float *descent) override;
      
      float indention_width(int i);
      float font_size() { return em; }
      float line_spacing() { return 0.3f * em; }
      
      bool inline_span() { return get_flag(InlineSpanBit); }
    
    private:
      enum {
        AutoIndentBit = base::NumFlagsBits,
        InlineSpanBit,
        HasTextShadowSpan,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool auto_indent() {                return get_flag(AutoIndentBit); }
      void auto_indent(bool value) {          change_flag(AutoIndentBit, value); }
      void inline_span(bool value) {          change_flag(InlineSpanBit, value); }
      bool has_text_shadow_span() {       return get_flag(HasTextShadowSpan);}
      void has_text_shadow_span(bool value) { change_flag(HasTextShadowSpan, value); }
      
      virtual void on_text_changed() override;
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual Expr to_pmath_impl(BoxOutputFlags flags, int start, int end) override;
      
    private:
      Array<GlyphInfo> glyphs;
      Array<Line>      lines;
      SyntaxGlyphStylesArray       semantic_styles; // uses text index, not glyph index
      RleLinearPredictorArray<int> glyph_to_text;
      RleArray<MathSequence*>      glyph_to_inline_sequence;
      SpanArray        spans;
  };
}

#endif // RICHMATH__BOXES__SEQUENCE_H__INCLUDED
