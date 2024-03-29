#ifndef RICHMATH__BOXES__TEXTBOX_H__INCLUDED
#define RICHMATH__BOXES__TEXTBOX_H__INCLUDED

#include <pango/pangocairo.h>

#include <boxes/abstractsequence.h>
#include <graphics/text-layout-iterator.h>


namespace richmath {
  class TextSequence;
  
  /* This is a box containing text (no math) and other boxes.
     It uses Pango for text layout. For math, use class MathSequence.
   */
  class TextSequence final : public AbstractSequence {
      using base = AbstractSequence;
      class Impl;
    protected:
      virtual ~TextSequence();
      
    public:
      TextSequence();
      
      virtual LayoutKind kind() override { return LayoutKind::Text; }
      static AbstractSequence *create() { return new TextSequence(); }
      virtual AbstractSequence *create_similar() override { return create(); }
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual void selection_path(Canvas &canvas, int start, int end) override;
      virtual void selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, int start, int end) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual void load_from_object(Expr obj, BoxInputFlags options) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(int index, cairo_matrix_t *matrix) override;
      
      virtual bool request_repaint(const RectangleF &rect) override;
      //virtual bool request_repaint_range(int start, int end) override;
      virtual bool visible_rect(RectangleF &rect, Box *top_most) override;
      
      virtual int get_line(int index, int guide = 0) override; // 0, 1, 2, ...
      virtual void get_line_heights(int line, float *ascent, float *descent) override; // only valid if !inline_span()
      
      TextSequence &outermost_sequence();
      TextLayoutIterator outermost_layout_iter();
    
      bool inline_span() { return get_flag(InlineSpanBit); }
      
      void ensure_layout_valid();
      
      virtual bool is_word_boundary(int i) override;
      
      PangoLayout *get_layout() { return _layout; } // only valid if !inline_span()
      int buffer_size() { return _buffer_size; }
      RleArrayIterator<const RleLinearPredictorArray<int>> buffer_to_text_iter() {            return buffer_to_text.cbegin(); }
      RleArrayIterator<const RleArray<TextSequence*>>      buffer_to_inline_sequence_iter() { return buffer_to_inline_sequence.cbegin(); }
      
    private:
      enum {
        InlineSpanBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      void inline_span(bool value) { change_flag(InlineSpanBit, value); }
      
      virtual void on_text_changed() override;
      
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual Expr to_pmath_impl(BoxOutputFlags flags, int start, int end) override;
      
    private:
      Array<int>                   line_y_corrections;
      RleLinearPredictorArray<int> buffer_to_text;
      RleArray<TextSequence*>      buffer_to_inline_sequence;
      PangoLayout                 *_layout;
      int                          _buffer_size;
  };
}

#endif // RICHMATH__BOXES__TEXTBOX_H__INCLUDED
