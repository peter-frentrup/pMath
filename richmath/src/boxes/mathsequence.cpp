#include <boxes/mathsequence.h>

#include <climits>
#include <cmath>

#include <boxes/box-factory.h>
#include <boxes/errorbox.h>
#include <boxes/fractionbox.h>
#include <boxes/gridbox.h>
#include <boxes/numberbox.h>
#include <boxes/section.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/templatebox.h>
#include <boxes/underoverscriptbox.h>

#include <eval/binding.h>
#include <eval/application.h>

#include <graphics/context.h>
#include <graphics/glyph-iterator.h>
#include <graphics/ot-font-reshaper.h>

#include <syntax/scope-colorizer.h>
#include <syntax/spanexpr.h>


#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))


using namespace richmath;

extern pmath_symbol_t richmath_System_Automatic;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_List;

static const float RefErrorIndictorHeight = 1 / 3.0f;

static const float UnderoverscriptOverhangCoverage = 0.75f;
/*
   A value of 1 would        A value of 0 would
   yield:                    yield:
    /   n       \                 n
    | .---.     |             / .---.     \
    |  \        |             |  \        |
    |  /    ... |             |  /    ... |
    | '---'     |             \ '---'     /
    \  i=1      /                i=1
*/

namespace {
  struct ScanData {
    MathSequence *sequence;
    int current_box; // for box_at_index
    BoxOutputFlags flags;
    int start;
    int end;
  };
  
  class BreakPositionWithPenalty {
    public:
      BreakPositionWithPenalty()
        : glyph_position(-1),
          prev_break_index(-1),
          penalty(Infinity)
      {
      }
      
      BreakPositionWithPenalty(int glyph_position, int prev, double pen)
        : glyph_position(glyph_position),
          prev_break_index(prev),
          penalty(pen)
      {}
      
    public:
      int glyph_position;
      int prev_break_index;
      double penalty;
  };
  
  struct GlyphHeights {
    float ascent;
    float descent;
  };
  
  class InlineSpanPainting {
    public:
      explicit InlineSpanPainting(MathSequence *cur);
      ~InlineSpanPainting();
      
      void switch_to_sequence(Context &context, MathSequence *next_seq, DisplayStage stage) { if(current != next_seq) switch_to_sequence_impl(context, next_seq, stage); }
    
    private:
      void switch_to_sequence_impl(Context &context, MathSequence *next_seq, DisplayStage stage);
    
    public:
      static BasicHeterogeneousStack context_stack;
      
    private:
      MathSequence *current;
      int debug_stack_depth;
  };
}

namespace richmath {
  class MathSequence::Impl {
    private:
      MathSequence &self;
      
    public:
      Impl(MathSequence &_self)
        : self(_self)
      {
      }
      
      //{ loading/scanner helpers
    public:
      static pmath_bool_t subsuperscriptbox_at_index(int i, void *_data);
      static pmath_string_t underoverscriptbox_at_index(int i, void *_data);
      static void syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err);
      static pmath_t box_at_index(int i, void *_data);
      static pmath_t add_debug_metadata(
        pmath_t                             token_or_span,
        const struct pmath_text_position_t *start,
        const struct pmath_text_position_t *end,
        void                               *_data);
      
      static pmath_t remove_null_tokens(pmath_t boxes);
      //}
      
      //{ get vertical size
    public:
      void box_size(  InlineSpanPainting &isp, Context &context, const GlyphIterator &pos, float *a, float *d);
      void boxes_size(InlineSpanPainting &isp, Context &context, GlyphIterator start, const GlyphIterator &end, float *a, float *d);
      //}
      
      //{ generate glyphs
    public:
      void generate_glyphs(Context &context);
      MathSequence &outermost_span();
      GlyphIterator glyph_iterator();
      
    private:
      class GlyphGenerator {
        public:
          using g2t_t   = RleLinearPredictorArray<int>;
          using g2seq_t = RleArray<MathSequence*>;
          using g2t_iter_t   = g2t_t::iterator_type;
          using g2seq_iter_t = g2seq_t::iterator_type;
        public:
          explicit GlyphGenerator(MathSequence &owner);
          
          void append_all(Context &context);
          
        private:
          void append_all(Context &context, MathSequence &seq);
          void append_span(Context &context, MathSequence &span_seq, Span span, int *pos, int *box);
          void append_text_glyph_run(Context &context, MathSequence &seq, int pos, int count);
          void append_box_glyphs(    Context &context, MathSequence &seq, int pos, Box *box);
          void append_empty_glyphs(MathSequence &seq, int pos, int count);
          
        private:
          InlineSpanPainting isp;
          MathSequence &owner;
          g2t_iter_t    g2t_iter;
          g2seq_iter_t  g2seq_iter;
      };
      
      MathSequence &parent_outermost_span();
      //}
      
      //{ vertical stretching
    public:
      void stretch_all_vertical(Context &context);
      
    private:
      class VerticalStretcher {
        public:
          explicit VerticalStretcher(Context &context, MathSequence &seq);
          
          void stretch_all(GlyphHeights &core_heights, GlyphHeights &heights);

        private:
          static bool can_stretch_char(uint16_t ch);

          void stretch_outermost_span(GlyphHeights &core_heights, GlyphHeights &heights);
          void stretch_span(                  MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights);
          void stretch_span_start(            MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights);
          void try_stretch_division_span_rest(MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights);
          void stretch_span_rest(             MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights);
          void stretch_nonspan_box(Box *box, GlyphHeights &core_heights, GlyphHeights &heights);
          void size_nonspan_token(MathSequence *span_seq, GlyphHeights &heights);
          
        private:
          Context            &context;
          GlyphIterator       iter;
          InlineSpanPainting  isp;
      };
      //}
      
      //{ OpenType substitutions
    private:
      void substitute_glyphs(
        Context              &context,
        uint32_t              math_script_tag,
        uint32_t              math_language_tag,
        uint32_t              text_script_tag,
        uint32_t              text_language_tag,
        const FontFeatureSet &features);
      
    public:
      void apply_glyph_substitutions(Context &context);
      //}
      
      //{ horizontal kerning/spacing
    private:
      class EnlargeSpace {
        public:
          EnlargeSpace(MathSequence &_self, Context &context)
            : self{_self},
              context{context},
              isp{&_self}
          {
          }
          
          void run();
          
        private:
          void run_text_space_characters();
          
          void group_number_digits(const GlyphIterator &start, const GlyphIterator &end_inclusive);
          bool slant_is_italic(int glyph_slant);
          void italic_correction(const GlyphIterator &token_end);
          void show_tab_character(const GlyphIterator &pos, bool in_string);
          ArrayView<const uint16_t> get_effective_token(const GlyphIterator &start, const GlyphIterator &tok_end);
          void find_box_token(const uint16_t *&op, int &ii, int &ee, Box *box);
          static pmath_token_t get_box_start_token(Box *box);
          
        private:
          MathSequence       &self;
          Context            &context;
          InlineSpanPainting  isp;
      };
    
    public:
      void enlarge_space(Context &context) {
        EnlargeSpace(self, context).run();
      }
      
      //}
      
      //{ horizontal stretching (variable width)
    public:
      bool hstretch_lines( // return whether there was any height change
        float  width,
        float  window_width,
        float *unfilled_width);
      
      //}
      
      //{ line breaking/indentation
    private:
      class IndentLines {
        public:
          explicit IndentLines(MathSequence &seq);
          
          void visit_all();
          
        private:
          void visit_span(      MathSequence *span_seq, Span span, int depth);
          void visit_token(     MathSequence *span_seq, int depth);
          void visit_string(    MathSequence *span_seq, Span span, int depth);
          void visit_block_body(MathSequence *span_seq, Span span, int depth);
          
        public:
          /* indention_array[i]: indention of the next line, if there is a line break
             before the i-th glyph.
           */
          static Array<int> indention_array;
        
        private:
          GlyphIterator iter;
      };
      
      class PenalizeBreaks {
        public:
          explicit PenalizeBreaks(MathSequence &seq);
          
          void visit_all();
          
        private:
          void visit_span(      MathSequence *span_seq, Span span, int depth);
          void visit_token(     MathSequence *span_seq, int depth);
          void visit_string(MathSequence *span_seq, Span span, int depth);
          void visit_block_body(MathSequence *span_seq, Span span, int depth);
         
        public:
          /* penalty_array[i]: A penalty value, which is used to decide whether a line
             break should be placed after the i-th glyph. The higher this value is, 
             the lower is the probability of a line break after glyph i.
           */
          static Array<double> penalty_array;
          
          static const double DepthPenalty;
          static const double WordPenalty;
          static const double BestLineWidth;
          static const double LineWidthFactor;
          
        private:
          GlyphIterator iter;
      };
      
      static Array<BreakPositionWithPenalty>  break_array;
      static Array<int>                       break_result;
      
    private:
      void new_line(int glyph_index, unsigned int indent, bool continuation = false);
      
    public:
      void split_lines(Context &context);
      //}
      
      void calculate_line_heights(Context &context, InlineSpanPainting &isp);
      void calculate_total_extents_from_lines();
      
      void paint(Context &context);
      
      Vector2F total_offest_to_index(int index);
      float glyph_offset_x(int glpyh_index, int line_index);
      
      void selection_path_wrt_outermost_origin(Context *opt_context, Canvas &canvas, int start, int end);
      void selection_path(Context *opt_context, Canvas &canvas, VolatileSelection sel);
      void selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, Context *opt_context, VolatileSelection sel);
      void selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, Context *opt_context, const GlyphIterator &start, const GlyphIterator &end);
      
      class PaintHookHandler {
        public:
          PaintHookHandler(Context &context, Point outermost_origin, PaintHookManager &hooks, bool background);
          
          void run(MathSequence &seq);
        
        private:
          Context          &context;
          Point             outermost_origin;
          PaintHookManager &hooks;
          bool              background;
      };
  };
  
  Array<BreakPositionWithPenalty>  MathSequence::Impl::break_array(0);
  Array<int>                       MathSequence::Impl::break_result(0);
  
}

//{ class MathSequence ...

MathSequence::MathSequence()
  : base()
{
}

float MathSequence::fill_weight() {
  if(boxes.length() == 1 && str.length() == 1)
    return boxes[0]->fill_weight();
  
  return 0.0f;
}

bool MathSequence::expand(const BoxSize &size) {
  if(inline_span())
    return false;
  
  if(glyphs.length() == 1) {
    GlyphIterator iter = Impl(*this).glyph_iterator();
    if(auto box = iter.current_box()) {
      if(box->expand(size)) {
        _extents = box->extents();
        glyphs[0].right  = _extents.width;
        lines[0].ascent  = _extents.ascent;
        lines[0].descent = _extents.descent;
        return true;
      }
    }
  }
  else {
    float uw;
    float w = _extents.width;
    
    bool height_changes = Impl(*this).hstretch_lines(size.width, size.width, &uw);
    //if(height_changes)
    //  Impl(*this).calculate_line_heights( ? context ? );
    
    Impl(*this).calculate_total_extents_from_lines();
    return w != _extents.width;
  }
  
  return false;
}

void MathSequence::resize(Context &context) {
  inline_span(false);
  has_text_shadow_span(false);
  ensure_boxes_valid();
  ensure_spans_valid();
  
  em = context.canvas().get_font_size();
  auto_indent(context.math_spacing);
  
  float old_scww = context.section_content_window_width;
  context.section_content_window_width = HUGE_VAL;
  
  semantic_styles.clear();
  
  Impl(*this).generate_glyphs(context);
  Impl(*this).apply_glyph_substitutions(context);
  
  if(context.show_auto_styles) {
    ScopeColorizer colorizer(*this);
    
    colorizer.comments_colorize();
      
    int pos = 0;
    while(pos < length()) {
      SpanExpr *se = new SpanExpr(pos, spans[pos], this);
      
      if(se->count() == 0 || !is_comment_start_at(buffer_view(se->item_as_text(0)))) {
        colorizer.syntax_colorize_spanexpr(        se);
        colorizer.arglist_errors_colorize_spanexpr(se, em * RefErrorIndictorHeight);
      }
      
      pos = se->end() + 1;
      delete se;
    }

    // TODO: enlarge glyphs where semantic_styles gives SyntaxGlyphStyle with is_missing_after() == true
  }
  
  if(context.math_spacing) {
    if(glyphs.length() == 1 && !dynamic_cast<UnderoverscriptBox *>(parent()))
    {
      pmath_token_t tok = pmath_token_analyse(str.buffer(), 1, nullptr);
      
      if(tok == PMATH_TOK_INTEGRAL || tok == PMATH_TOK_PREFIX) {
        context.math_shaper->vertical_stretch_char(
          context,
          0, 0,
          true,
          str[0],
          &glyphs[0]);
          
        BoxSize size;
        context.math_shaper->vertical_glyph_size(
          context,
          str[0],
          glyphs[0],
          &size.ascent,
          &size.descent);
      }
      else
        Impl(*this).stretch_all_vertical(context);
    }
    else
      Impl(*this).stretch_all_vertical(context);
  }
  Impl(*this).enlarge_space(context);
  
  InlineSpanPainting isp{this};
  
  {
    _extents.width = 0;
    for(GlyphIterator iter{*this}; iter.has_more_glyphs(); iter.move_next_glyph()) {
      GlyphInfo &gi = iter.current_glyph();
      if(iter.current_char() == '\n')
        gi.right = _extents.width;
      else
        gi.right = _extents.width += gi.right;
    }
  }
  
  lines.length(1);
  lines[0].end = glyphs.length();
  lines[0].ascent = lines[0].descent = 0;
  lines[0].indent = 0;
  lines[0].continuation = 0;
  
  context.section_content_window_width = old_scww;
  
  Impl(*this).split_lines(context);
  if(dynamic_cast<Section *>(parent())) {
    Impl(*this).hstretch_lines(
      context.width,
      context.section_content_window_width,
      &context.sequence_unfilled_width);
  }
  
  Impl(*this).calculate_line_heights(context, isp);
  Impl(*this).calculate_total_extents_from_lines();
  
  isp.switch_to_sequence(context, this, DisplayStage::Layout);
  
  if(context.sequence_unfilled_width == -HUGE_VAL)
    context.sequence_unfilled_width = _extents.width;
}

void MathSequence::colorize_scope(SyntaxState &state) {
  ensure_spans_valid();
  
  ScopeColorizer colorizer(*this);
  
  int pos = 0;
  while(pos < length()) {
    SpanExpr *se = new SpanExpr(pos, spans[pos], this);
    
    colorizer.scope_colorize_spanexpr(state, se);
    
    pos = se->end() + 1;
    delete se;
  }
}

void MathSequence::before_paint_inline(Context &context) {
  for(auto box : boxes) {
    if(box->as_inline_span())
      box->before_paint_inline(context);
  }
}

void MathSequence::paint(Context &context) {
  Point p0 = context.canvas().current_pos();
  
  Color default_color = context.canvas().get_color();
  
  before_paint_inline(context);
  
  Impl::PaintHookHandler(context, p0, context.pre_paint_hooks, true).run(*this);
  
  if(has_text_shadow_span() && !context.canvas().show_only_text) {
    RleArray<Expr> text_shadows;
    auto text_shadows_iter = text_shadows.begin();
    
    for(auto &run : glyph_to_inline_sequence.groups) {
      text_shadows_iter.rewind_to(run.first_index);
      Expr val;
      for(Box *box = run.next_value; box && box != this; box = box->parent()) {
        Expr expr;
        if(box->style && context.stylesheet->get(box->style, TextShadow, &expr)) {
          val = PMATH_CPP_MOVE(expr);
          break;
        }
      }
      text_shadows_iter.reset_rest(val);
    }
    
    Array<RectangleF> rects;
    for(int i = 0; i < text_shadows.groups.length(); ++i) {
      if(Expr text_shadow = text_shadows.groups[i].next_value) {
        GlyphIterator run_start(*this);
        run_start.move_to_glyph(text_shadows.groups[i].first_index);
        
        GlyphIterator run_end = run_start;
        if(i + 1 <  text_shadows.groups.length())
          run_end.move_to_glyph(text_shadows.groups[i+1].first_index);
        else
          run_end.move_to_glyph(glyphs.length());
        
        rects.length(0);
        Impl(*this).selection_rectangles(rects, SelectionDisplayFlags::Default, context.canvas().current_pos(), nullptr/*&context*/, run_start, run_end);
        
        for(auto &rect : rects) {
          context.draw_text_shadows(
            [&](Context &ctx) { Impl(*this).paint(ctx); }, 
            rect,
            text_shadow);
        }
      }
    }
  }
  context.canvas().move_to(p0);
  Impl(*this).paint(context);
  
  Impl::PaintHookHandler(context, p0, context.post_paint_hooks, false).run(*this);
  
  context.canvas().set_color(default_color);
}

void MathSequence::selection_path(Canvas &canvas, int start, int end) {
  auto delta = Impl(*this).total_offest_to_index(0);
  
  canvas.rel_move_to(-delta);
  Impl(*this).selection_path_wrt_outermost_origin(nullptr, canvas, start, end);
}

void MathSequence::selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, int start, int end) {
  MathSequence &outer = Impl(*this).outermost_span();
  Vector2F delta = Impl(*this).total_offest_to_index(0);
  Impl(outer).selection_rectangles(rects, flags, p0 - delta, nullptr, {this, start, end});
}

void MathSequence::selection_path(Context &context, int start, int end) {
  auto delta = Impl(*this).total_offest_to_index(0);
  
  context.canvas().rel_move_to(-delta);
  Impl(*this).selection_path_wrt_outermost_origin(&context, context.canvas(), start, end); 
}

void MathSequence::on_text_changed() {
  // glyph_to_inline_sequence might now contain invalid references
  glyph_to_inline_sequence.clear();
  glyph_to_text.clear();
  glyphs.length(0);
  lines.length(0);
  
  MathSequence &outer = Impl(*this).outermost_span();
  outer.text_changed(true);
}

Expr MathSequence::to_pmath_impl(BoxOutputFlags flags) {
  return to_pmath_impl(flags, 0, length());
}

Expr MathSequence::to_pmath_impl(BoxOutputFlags flags, int start, int end) {
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = flags;
  data.start       = start;
  data.end         = end;
  
  struct pmath_boxes_from_spans_ex_t settings;
  memset(&settings, 0, sizeof(settings));
  settings.size           = sizeof(settings);
  settings.data           = &data;
  settings.box_at_index   = Impl::box_at_index;
  settings.add_debug_metadata = Impl::add_debug_metadata;
  
  if(has(flags, BoxOutputFlags::Parseable))
    settings.flags |= PMATH_BFS_PARSEABLE;
    
  settings.flags |= PMATH_BFS_USESTRINGBOX;
  
  ensure_spans_valid();
  
  pmath_t boxes = pmath_boxes_from_spans_ex(spans.array(), str.get(), &settings);
  if(start > 0 || end < length())
    boxes = Impl::remove_null_tokens(boxes);
  
  if(has(flags, BoxOutputFlags::WithDebugMetadata)) {
    boxes = pmath_try_set_debug_metadata(
              boxes, 
              SelectionReference(id(), start, end).to_pmath().release());
  }
  
  return Expr(boxes);
}

Box *MathSequence::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  const int len = length();
  const uint16_t *buf = str.buffer();
  
  if(direction == LogicalDirection::Forward) {
    if(*index >= len) {
      if(auto par = parent()) {
        if(!par->exitable())
          return this;
          
        *index = _index;
        return par->move_logical(LogicalDirection::Forward, true, index);
      }
      return this;
    }
    
    if(*index < 0) {
      *index = 0;
      return this;
    }
    
    if(jumping || buf[*index] != PMATH_CHAR_BOX) {
      if(jumping) {
        while(*index + 1 < len && !spans.is_token_end(*index))
          ++*index;
          
        ++*index;
        while(*index < len && (buf[*index] == ' ' || buf[*index] == '\t'))
          ++*index;
      }
      else {
        if(*index + 1 < len && is_utf16_high(buf[*index]) && is_utf16_low(buf[*index + 1]))
          ++*index;
          
        ++*index;
      }
      
      return this;
    }
    
    ensure_boxes_valid();
    
    int b = 0;
    while(boxes[b]->index() != *index)
      ++b;
    *index = -1;
    return boxes[b]->move_logical(LogicalDirection::Forward, true, index);
  }
  
  if(*index <= 0) {
    if(auto par = parent()) {
      if(!par->exitable())
        return this;
        
      *index = _index + 1;
      return par->move_logical(LogicalDirection::Backward, true, index);
    }
    return this;
  }
  
  if(*index > len) {
    *index = len;
    return this;
  }
  
  if(jumping) {
    do {
      --*index;
    } while(*index > 0 && (buf[*index] == ' ' || buf[*index] == '\t'));
    ++*index;
    
    do {
      --*index;
    } while(*index > 0 && !spans.is_token_end(*index - 1));
    
    return this;
  }
  
  if(buf[*index - 1] != PMATH_CHAR_BOX) {
    --*index;
    
    if(*index > 0 && is_utf16_high(buf[*index - 1]) && is_utf16_low(buf[*index]))
      --*index;
      
    return this;
  }
  
  ensure_boxes_valid();
  
  int b = 0;
  while(boxes[b]->index() != *index - 1)
    ++b;
  *index = boxes[b]->length() + 1;
  return boxes[b]->move_logical(LogicalDirection::Backward, true, index);
}

Box *MathSequence::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  int line, dstline;
  float x = *index_rel_x;
  
  GlyphIterator iter = Impl(*this).glyph_iterator();
  MathSequence     &outer  = *iter.outermost_sequence();
  Array<Line>      &lines  = iter.outermost_sequence()->lines;
  Array<GlyphInfo> &glyphs = iter.outermost_sequence()->glyphs;
    
  if(*index >= 0) {
    iter.skip_forward_to_glyph_after_text_pos(this, *index);
    
    line = 0;
    while(line < lines.length() - 1 && lines[line].end <= iter.glyph_index())
      ++line;
      
    if(iter.glyph_index() > 0) {
      x += glyphs[iter.glyph_index() - 1].right + outer.indention_width(lines[line].indent);
      if(line > 0)
        x -= glyphs[lines[line - 1].end - 1].right;
    }
    dstline = direction == LogicalDirection::Forward ? line + 1 : line - 1;
  }
  else if(direction == LogicalDirection::Forward) {
    line = -1;
    dstline = 0;
  }
  else {
    line = lines.length();
    dstline = line - 1;
  }
  
  if(dstline >= 0 && dstline < lines.length()) {
    ensure_boxes_valid();
    
    float l = outer.indention_width(lines[dstline].indent);
    if(dstline > 0) {
      iter.move_to_glyph(lines[dstline - 1].end);
      l -= glyphs[lines[dstline - 1].end - 1].right;
    }
    else
      iter.move_to_glyph(0);
    
    while(iter.glyph_index() < lines[dstline].end && iter.current_glyph().right + l < x)
      iter.move_next_glyph();
      
    if(iter.glyph_index() < lines[dstline].end && iter.current_char() != PMATH_CHAR_BOX) {
      if( (iter.glyph_index() == 0 && l +  iter.current_glyph().right                                         / 2 <= x) ||
          (iter.glyph_index() >  0 && l + (iter.current_glyph().right + glyphs[iter.glyph_index() - 1].right) / 2 <= x)
      ) {
        iter.move_next_glyph();
      }
      
//      if(is_utf16_high(str[i - 1]))
//        --i;
//      else if(is_utf16_low(str[i]))
//        ++i;
      if(is_utf16_low(iter.current_char()))
        iter.move_next_glyph();
    }
    
    if(iter.glyph_index() > 0 && iter.has_more_glyphs() && iter.glyph_index() == lines[dstline].end) {
      auto prev = iter;
      prev.move_by_glyphs(-1);
      if(direction == LogicalDirection::Backward || prev.current_char() == '\n')
        iter = prev;
    }
    
    *index_rel_x = x - outer.indention_width(lines[dstline].indent);
    if(iter.glyph_index() == lines[dstline].end && dstline < lines.length() - 1) {
      *index_rel_x += outer.indention_width(lines[dstline + 1].indent);
    }
    else if(iter.glyph_index() > 0) {
      *index_rel_x -= glyphs[iter.glyph_index() - 1].right;
      if(dstline > 0)
        *index_rel_x += glyphs[lines[dstline - 1].end - 1].right;
    }
    
    if(auto box = iter.current_box()) {
      if(*index_rel_x > 0) {
        if(!iter.has_more_glyphs()) { // 'glyphs' is not up-to date. Get out of here quickly before things get worse.
          *index = iter.text_index();
          return iter.current_sequence();
        }
        if(x < iter.current_glyph().right + l) {
          *index = -1;
          return box->move_vertical(direction, index_rel_x, index, false);
        }
      }
    }
    
    *index = iter.text_index();
    return iter.current_sequence();
  }
  
  if(auto par = outer.parent()) {
    if(!par->exitable())
      return move_logical(direction, false, index);
    
    *index_rel_x = x;
    *index = outer.index();
    return par->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

void MathSequence::select_nearby_placeholder(int *start, int *end, float *index_rel_x) {
  if(*start != *end)
    return;
  
  if(is_placeholder(*start - 1)) {
    --*start;
    auto iter = Impl(*this).glyph_iterator();
    iter.skip_forward_to_glyph_after_text_pos(this, *start);
    
    if(iter.has_more_glyphs())
      *index_rel_x += iter.current_glyph().right;
    if(iter.glyph_index() > 0)
      iter.all_glyphs()[iter.glyph_index() - 1].right;
  }
  else if(is_placeholder(*start))
    ++*end;
}

VolatileSelection MathSequence::mouse_selection(Point pos, bool *was_inside_start) {
  *was_inside_start = true;
  
  GlyphIterator start = Impl(*this).glyph_iterator();
  Array<Line>      &lines  = start.outermost_sequence()->lines;
  Array<GlyphInfo> &glyphs = start.outermost_sequence()->glyphs;
  
  if(lines.length() == 0) 
    return { this, 0, 0 };
  
  int line = 0;
  while(line < lines.length() - 1 && pos.y > lines[line].descent + 0.1 * em) {
    pos.y -= lines[line].descent + line_spacing() + lines[line + 1].ascent;
    ++line;
  }
  
  if(line > 0)
    start.move_to_glyph(lines[line - 1].end);
    
  pos.x -= indention_width(lines[line].indent);
  if(pos.x < 0) {
    *was_inside_start = false;
    int i = start.index_in_sequence(this, -1);
    if(i >= 0)
      return { this, i, i };
    else
      return { this, 0, length() };
  }
  
  float line_start = 0;
  if(start.glyph_index() > 0)
    line_start += glyphs[start.glyph_index() - 1].right;
    
  ensure_boxes_valid();
  
  while(start.glyph_index() < lines[line].end) {
    if(pos.x <= start.current_glyph().right - line_start) {
      float prev = 0;
      if(start.glyph_index() > 0)
        prev = glyphs[start.glyph_index() - 1].right;
        
      if(start.current_sequence()->is_placeholder(start.text_index())) {
        *was_inside_start = true;
        VolatileSelection sel = { start.current_sequence(), start.text_index(), start.text_index() + 1 };
        sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
        return sel;
      }
      
      if(auto box = start.current_box()) {
        float xoff = start.current_glyph().x_offset;
        if(pos.x > prev - line_start + xoff + box->extents().width) {
          *was_inside_start = false;
          VolatileSelection sel = { start.current_sequence(), start.text_index(), start.text_index() + 1 };
          sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
          return sel;
        }
        
        if(pos.x < prev - line_start + xoff) {
          *was_inside_start = false;
          VolatileSelection sel = { start.current_sequence(), start.text_index() };
          sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
          return sel;
        }
        else {
          auto sel = box->mouse_selection(
                       pos - Vector2F(prev - line_start + xoff, 0),
                       was_inside_start);
          
          box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
          return sel;
        }
      }
      
      if(line_start + pos.x > (prev + start.current_glyph().right) / 2) {
        *was_inside_start = false;
        VolatileSelection sel = { start.current_sequence(), start.text_index() + 1 };
        sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
        return sel;
      }
      else {
        VolatileSelection sel = { start.current_sequence(), start.text_index() };
        sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
        return sel;
      }
    }
    
    start.move_next_glyph();
  }
  
  if(start.glyph_index() > 0) {
    auto prev = start;
    prev.move_by_glyphs(-1);
    if(prev.current_char() == '\n' && (line == 0 || lines[line - 1].end != lines[line].end)) {
      start = prev;
    }
    else if(prev.current_char() == ' ')
      start = prev;
  }
  
  if(start.has_more_glyphs()) {
    VolatileSelection sel = { start.current_sequence(), start.text_index() };
    sel.box->after_inline_span_mouse_selection(this, sel, *was_inside_start);
    return sel;
  }
  else
    return { this, length() };
}

void MathSequence::child_transformation(int index, cairo_matrix_t *matrix) {
  auto delta = Impl(*this).total_offest_to_index(index);
  
  if(inline_span()) {
    int i = this->index();
    Box* box = parent();
    while(box) {
      if(auto seq = dynamic_cast<MathSequence*>(box)) {
        delta-= Impl(*seq).total_offest_to_index(i);
        break;
      }
      i = box->index();
      box = box->parent();
    }
  }
  
  cairo_matrix_translate(matrix, delta.x, delta.y);
}

bool MathSequence::request_repaint_all() {
  return request_repaint_range(0, length());
}

bool MathSequence::request_repaint_range(int start, int end) {
  MathSequence &outer = Impl(*this).outermost_span();
  
  if(text_changed()) 
    return outer.request_repaint(outer.extents().to_rectangle());
  
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
  
  Point p1(Impl(*this).total_offest_to_index(start));
  
  Point p2;
  if(start == end) 
    p2 = p1;
  else 
    p2 = Point(Impl(*this).total_offest_to_index(end));
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  if(l1 == l2)
    return outer.request_repaint({p1.x, p1.y - a1, p2.x - p1.x, a1 + d1});
             
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  bool result = true;
  result = outer.request_repaint({p1.x, p1.y - a1, outer.extents().width - p1.x, a1 + d1}) || result;
  result = outer.request_repaint({0.0f, p1.y + d1, outer.extents().width, p2.y - a2 - p1.y - d1}) || result;
  result = outer.request_repaint({0.0f, p2.y - a2, p2.x, a2 + d2}) || result;
  return result;
}

bool MathSequence::request_repaint(const RectangleF &rect) {
  if(inline_span()) {
    MathSequence &outer = Impl(*this).outermost_span();
    if(&outer != this) {
      Vector2F delta = Impl(*this).total_offest_to_index(0);
      
      return outer.request_repaint(rect + delta);
    }
    else {
      // inconsitent inline_span() flag. Probably this box was just edited.
      inline_span(false);
    }
  }
  
  return base::request_repaint(rect);
}

RectangleF MathSequence::range_rect(int start, int end) {
  if(text_changed())
    return _extents.to_rectangle();
  
  int l1, l2;
  
  l1 = l2 = get_line(start);
  if(start != end)
    l2 = get_line(end, l1);
  
  Point p1(Impl(*this).total_offest_to_index(start));
  
  Point p2;
  if(start == end) 
    p2 = p1;
  else 
    p2 = Point(Impl(*this).total_offest_to_index(end));
  
  float a1, d1;
  get_line_heights(l1, &a1, &d1);
  
  MathSequence &outer = Impl(*this).outermost_span();
  
  if(l1 == l2)
    return {p1.x, p1.y - a1, p2.x - p1.x, a1 + d1};
             
  float a2, d2;
  get_line_heights(l2, &a2, &d2);
  
  return RectangleF{p1.x, p1.y - a1, extents().width - p1.x, a1 + d1}
    .union_hull({0.0f, p1.y + d1, extents().width, p2.y - a2 - p1.y - d1})
    .union_hull({0.0f, p2.y - a2, p2.x, a2 + d2});
}

bool MathSequence::visible_rect(RectangleF &rect, Box *top_most) {
  if(inline_span()) {
    MathSequence &outer = Impl(*this).outermost_span();
    
    if(top_most && outer.is_parent_of(top_most)) {
      cairo_matrix_t matrix;
      cairo_matrix_init_identity(&matrix);
      transformation(top_most, &matrix);
      Canvas::transform_rect_inline(matrix, rect);
      return top_most->visible_rect(rect, top_most);
    }
    
    Vector2F delta = Impl(*this).total_offest_to_index(0);
    
    rect+= delta;
    return outer.visible_rect(rect, top_most);
  }
  
  return base::visible_rect(rect, top_most);
}

int MathSequence::find_string_start(int pos_inside_string, int *next_after_string) {
  ensure_spans_valid();
  
  if(next_after_string)
    *next_after_string = -1;
    
  const uint16_t *buf = str.buffer();
  int i = 0;
  while(i < pos_inside_string) {
    if(buf[i] == '"') {
      int start = i;
      Span span = spans[i];
      
      while(span.next()) {
        span = span.next();
      }
      
      if(span) {
        i = span.end() + 1;
        if(i > pos_inside_string || (i == pos_inside_string && buf[i - 1] != '"')) {
          if(next_after_string)
            *next_after_string = i;
          return start;
        }
      }
      else
        ++i;
    }
    else
      ++i;
  }
  
  return -1;
}

void MathSequence::ensure_spans_valid() {
  if(!text_changed())
    return;
    
  text_changed(false);
  
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = BoxOutputFlags::Default;
  data.start       = 0;
  data.end         = str.length();
  
  pmath_string_t code = str.get_as_string();
  spans = pmath_spans_from_string(
            &code,
            nullptr,
            Impl::subsuperscriptbox_at_index,
            Impl::underoverscriptbox_at_index,
            Impl::syntax_error,
            &data);
}

bool MathSequence::is_word_boundary(int i) {
  if(i <= 0 || i >= length())
    return true;
  
  ensure_spans_valid();
  
  return spans.is_token_end(i - 1);
}

int MathSequence::matching_fence(int pos) {
  int len = str.length();
  if(pos < 0 || pos >= len)
    return -1;
  
  if(Tokenizer::is_bracket(VolatileSelection(this, pos, pos+1).syntax_form())) {
    SpanExpr *span = new SpanExpr(pos, this);
    
    while(span) {
      if(span->start() <= pos && span->end() >= pos && span->length() > 1)
        break;
        
      span = span->expand(true);
    }
    
    if(span) {
      for(int i = 0; i < span->count(); ++i) {
        int tok_pos = span->item_pos(i);
        if(tok_pos != pos) {
          pmath_token_t tok = span->item(i)->as_token();
          
          if(tok == PMATH_TOK_LEFT ||
              tok == PMATH_TOK_LEFTCALL ||
              tok == PMATH_TOK_RIGHT)
          {
            delete span;
            return tok_pos;
          }
        }
      }
      
      delete span;
    }
  }
  
  return -1;
}

class PositionedExpr {
  public:
    PositionedExpr()
      : pos(0)
    {
    }
    
    PositionedExpr(Expr _expr, int _pos)
      : expr(_expr),
        pos(_pos)
    {
    }
    
  public:
    Expr expr;
    int  pos;
};

static void defered_make_box(int pos, pmath_t obj, void *data) {
  Array<PositionedExpr> *boxes = (Array<PositionedExpr> *)data;
  
  boxes->add(PositionedExpr(Expr(obj), pos));
}

class SpanSynchronizer: public Base {
  public:
    SpanSynchronizer(
      BoxInputFlags          _new_load_options,
      BoxAdopter             _owner_adoptor,
      Array<Box *>          &_owner_boxes,
      SpanArray             &_old_spans,
      Array<PositionedExpr> &_new_boxes,
      SpanArray             &_new_spans
    ) : Base(),
      owner_adoptor(   _owner_adoptor),
      owner_boxes(     _owner_boxes),
      old_spans(       _old_spans),
      old_pos(         0),
      old_next_box(    0),
      new_load_options(_new_load_options),
      new_boxes(       _new_boxes),
      new_spans(       _new_spans),
      new_pos(         0),
      new_next_box(    0)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
    }
    
    bool is_in_range() {
      if(old_pos >= old_spans.length())
        return false;
        
      if(old_next_box >= owner_boxes.length())
        return false;
        
      if(new_pos >= new_spans.length())
        return false;
        
      if(new_next_box >= new_boxes.length())
        return false;
        
      return true;
    }
    
    void next() {
      if(is_in_range())
        next(old_spans[old_pos], new_spans[new_pos]);
    }
    
    void finish() {
      if(old_pos == old_spans.length()) {
        RICHMATH_ASSERT(old_next_box == owner_boxes.length());
      }
      
      if(new_pos == new_spans.length()) {
        RICHMATH_ASSERT(new_next_box == new_boxes.length());
      }
      
      int rem = 0;
      while(old_next_box + rem < owner_boxes.length() &&
            owner_boxes[old_next_box + rem]->index() < old_spans.length())
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          owner_boxes[old_next_box + i]->safe_destroy();
          
        owner_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        RICHMATH_ASSERT(new_box.pos < new_spans.length());
        
        Box *box = BoxFactory::create_empty_box(LayoutKind::Math, new_box.expr);
        owner_boxes.insert(old_next_box, 1, &box);
        owner_adoptor.adopt(box, new_box.pos);
        if(!box->try_load_from_object(new_box.expr, new_load_options)) {
          box->safe_destroy();
          box = new ErrorBox(new_box.expr);
          owner_boxes.set(old_next_box, box);
          owner_adoptor.adopt(box, new_box.pos);
        }
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  protected:
    void next(Span old_span, Span new_span) {
      if(!is_in_range()) {
        old_pos = old_spans.length();
        new_pos = new_spans.length();
        return;
      }
      
      if(old_span && new_span) {
        int old_start = old_pos;
        int new_start = new_pos;
        
        next(old_span.next(), new_span.next());
        
        RICHMATH_ASSERT(old_start < old_pos);
        RICHMATH_ASSERT(new_start < new_pos);
        
        while(old_pos <= old_span.end() &&
              new_pos <= new_span.end())
        {
          next(old_spans[old_pos], new_spans[new_pos]);
        }
      }
      
      if(old_span) {
        old_pos = old_span.end() + 1;
      }
      else {
        while(!old_spans.is_token_end(old_pos))
          ++old_pos;
        ++old_pos;
      }
      
      if(new_span) {
        new_pos = new_span.end() + 1;
      }
      else {
        while(!new_spans.is_token_end(new_pos))
          ++new_pos;
        ++new_pos;
      }
      
      while(old_next_box < owner_boxes.length() &&
            new_next_box < new_boxes.length())
      {
        Box *box = owner_boxes[old_next_box];
        
        if(box->index() >= old_pos)
          break;
          
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        if(!box->try_load_from_object(new_box.expr, new_load_options)) {
          box->safe_destroy();
          box = BoxFactory::create_empty_box(LayoutKind::Math, new_box.expr);
          owner_boxes.set(old_next_box, box);
          owner_adoptor.adopt(box, new_box.pos);
          if(!box->try_load_from_object(new_box.expr, new_load_options)) {
            box->safe_destroy();
            box = new ErrorBox(new_box.expr);
            owner_boxes.set(old_next_box, box);
            owner_adoptor.adopt(box, new_box.pos);
          }
        }
        
        ++old_next_box;
        ++new_next_box;
      }
      
      int rem = 0;
      while(old_next_box + rem < owner_boxes.length() &&
            owner_boxes[old_next_box + rem]->index() < old_pos)
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          owner_boxes[old_next_box + i]->safe_destroy();
          
        owner_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        Box *box = BoxFactory::create_empty_box(LayoutKind::Math, new_box.expr);
        owner_boxes.insert(old_next_box, 1, &box);
        owner_adoptor.adopt(box, new_box.pos);
        if(!box->try_load_from_object(new_box.expr, new_load_options)) {
          box->safe_destroy();
          box = new ErrorBox(new_box.expr);
          owner_boxes.set(old_next_box, box);
          owner_adoptor.adopt(box, new_box.pos);
        }
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  public:
    BoxAdopter        owner_adoptor;
    Array<Box *>     &owner_boxes;
    const SpanArray  &old_spans;
    int               old_pos;
    int               old_next_box;
    
    BoxInputFlags                new_load_options;
    const Array<PositionedExpr> &new_boxes;
    const SpanArray             &new_spans;
    int                          new_pos;
    int                          new_next_box;
};

void MathSequence::load_from_object(Expr object, BoxInputFlags options) {
  ensure_boxes_valid();
  
  Array<PositionedExpr> new_boxes;
  pmath_string_t        new_string;
  SpanArray             new_spans;
  
  Expr obj = object;
  
  if(obj[0] == richmath_System_BoxData && obj.expr_length() == 1)
    obj = obj[1];
    
  if(has(options, BoxInputFlags::FormatNumbers))
    obj = NumberBox::prepare_boxes(obj);
    
  if(has(options, BoxInputFlags::AllowTemplateSlots))
    obj = TemplateBoxSlot::prepare_boxes(obj);
  
  text_changed(true);
  new_spans = pmath_spans_from_boxes(
                pmath_ref(obj.get()),
                &new_string,
                defered_make_box,
                &new_boxes);
                
  SpanSynchronizer syncer(options, make_adoptor(), boxes, spans, new_boxes, new_spans);
  
  while(syncer.is_in_range())
    syncer.next();
  syncer.finish();
  
  spans         = new_spans.extract_array();
  str           = String(new_string);
  boxes_invalid(true);
  
  finish_load_from_object(PMATH_CPP_MOVE(object));
}

bool MathSequence::stretch_horizontal(Context &context, float width) {
  if(glyphs.length() != 1 || str[0] == PMATH_CHAR_BOX)
    return false;
    
  if(context.math_shaper->horizontal_stretch_char(
        context,
        width,
        str[0],
        &glyphs[0]))
  {
    _extents.width = glyphs[0].right;
    _extents.ascent  = _extents.descent = -1e9;
    context.math_shaper->vertical_glyph_size(
      context, str[0], glyphs[0], &_extents.ascent, &_extents.descent);
    lines[0].ascent  = _extents.ascent;
    lines[0].descent = _extents.descent;
    return true;
  }
  return false;
}

float MathSequence::first_glyph_width() {
  GlyphIterator iter = Impl(*this).glyph_iterator();
  iter.skip_forward_to_glyph_after_text_pos(this, 0);
  
  if(auto box = iter.current_box())
    return box->first_glyph_width();
  
  if(!iter.has_more_glyphs())
    return 0.0f;
  
  return iter.current_glyph().right;
}

float MathSequence::last_glyph_width() {
  if(length() == 0)
    return 0.0f;
    
  GlyphIterator iter = Impl(*this).glyph_iterator();
  iter.skip_forward_to_glyph_after_text_pos(this, length() - 1);
  
  if(auto box = iter.current_box())
    return box->last_glyph_width();
  
  if(!iter.has_more_glyphs())
    return 0.0f;
  
  if(iter.glyph_index() >= 1)
    return _extents.width - iter.all_glyphs()[iter.glyph_index() - 1].right;
    
  return _extents.width;
}

int MathSequence::get_line(int index, int guide) {
  if(text_changed()) {
    pmath_debug_print("[get_line ill-defined: text_changed()]\n");
    return 0;
  }

  GlyphIterator iter = Impl(*this).glyph_iterator();
  iter.skip_forward_to_glyph_after_text_pos(this, index);
  
  Array<Line> &lines = iter.outermost_sequence()->lines;
  
  if(guide >= lines.length())
    guide = lines.length() - 1;
  if(guide < 0)
    guide = 0;
    
  int line = guide;
  
  if(line < lines.length() && lines[line].end > iter.glyph_index()) {
    while(line > 0) {
      if(lines[line - 1].end <= iter.glyph_index())
        return line;
        
      --line;
    }
    
    return 0;
  }
  
  while(line < lines.length()) {
    if(lines[line].end > iter.glyph_index())
      return line;
      
    ++line;
  }
  
  return lines.length() > 0 ? lines.length() - 1 : 0;
}

void MathSequence::get_line_heights(int line, float *ascent, float *descent) {
  Array<Line> &lines = Impl(*this).outermost_span().lines;
  
  if(length() == 0) {
    *ascent  = 0.75f * em;
    *descent = 0.25f * em;
    return;
  }
  
  if(line < 0 || line >= lines.length()) {
    *ascent = *descent = 0;
    return;
  }
  
  *ascent  = lines[line].ascent;
  *descent = lines[line].descent;
}

float MathSequence::indention_width(int i) {
  if(!auto_indent())
    return 0.0f;
  
  float f = i * em / 2;
  if(f <= _extents.width / 2)
    return f;
    
  return floor(_extents.width / em) * em / 2;
}

//} ... class MathSequence

//{ class MathSequence::Impl ...

pmath_bool_t MathSequence::Impl::subsuperscriptbox_at_index(int i, void *_data) {
  ScanData *data = (ScanData *)_data;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    if(data->sequence->boxes[data->current_box]->index() == i) {
      auto b = dynamic_cast<SubsuperscriptBox*>(data->sequence->boxes[data->current_box]);
      return nullptr != b;
    }
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start) {
    if(data->sequence->boxes[data->current_box]->index() == i)
      return nullptr != dynamic_cast<SubsuperscriptBox *>(data->sequence->boxes[data->current_box]);
    ++data->current_box;
  }
  
  return FALSE;
}

pmath_string_t MathSequence::Impl::underoverscriptbox_at_index(int i, void *_data) {
  ScanData *data = (ScanData *)_data;
  
  Box *box = nullptr;
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    Box *next_box = data->sequence->boxes[data->current_box];
    if(next_box->index() == i) {
      box = next_box;
      break;
    }
    ++data->current_box;
  }

  if(!box) {
    data->current_box = 0;
    while(data->current_box < start) {
      Box *next_box = data->sequence->boxes[data->current_box];
      if(next_box->index() == i) {
        box = next_box;
        break;
      }
      ++data->current_box;
    }
  }

  return VolatileSelection(box, 0).expanded_to_parent().syntax_form().release();
}

void MathSequence::Impl::syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err) {
  ScanData *data = (ScanData *)_data;
  
  if(!data->sequence->get_style(ShowAutoStyles))
    return;
  
  ArrayView<const uint16_t> buf = buffer_view(code);
  if(err) {
    if(pos < data->sequence->length()) {
      data->sequence->semantic_styles.find(pos).reset_range(GlyphStyleSyntaxError, 1);
    }
  }
}

pmath_t MathSequence::Impl::box_at_index(int i, void *_data) {
  ScanData *data = (ScanData *)_data;
  
  BoxOutputFlags flags = data->flags;
  if(has(flags, BoxOutputFlags::Parseable) && data->sequence->is_inside_string(i)) {
    flags -= BoxOutputFlags::Parseable;
  }
  
  if(i < data->start || data->end <= i)
    return PMATH_FROM_TAG(PMATH_TAG_STR0, 0); // PMATH_C_STRING("")
    
  Box *box = nullptr;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    if(data->sequence->boxes[data->current_box]->index() == i) {
      box = data->sequence->boxes[data->current_box];
      break;
    }
    ++data->current_box;
  }
  
  if(!box) {
    data->current_box = 0;
    while(data->current_box < start) {
      if(data->sequence->boxes[data->current_box]->index() == i) {
        box = data->sequence->boxes[data->current_box];
        break;
      }
      ++data->current_box;
    }
  }
  
  if(!box)
    return PMATH_NULL;
  
  return box->to_pmath(flags).release();
}

pmath_t MathSequence::Impl::add_debug_metadata(
  pmath_t                             token_or_span,
  const struct pmath_text_position_t *start,
  const struct pmath_text_position_t *end,
  void                               *_data
) {
  ScanData *data = (ScanData *)_data;
  
  if(data->end <= start->index || end->index <= data->start) {
    pmath_unref(token_or_span);
    return PMATH_FROM_TAG(PMATH_TAG_STR0, 0); // PMATH_C_STRING("")
  }
  
  if(pmath_is_string(token_or_span)) {
    if(data->start > start->index || data->end < end->index) {
      /* does not work with string tokens containing boxes */
    
      if(start->index <= data->start && data->end <= end->index) {
        token_or_span = pmath_string_part(
                 token_or_span,
                 data->start - start->index,
                 data->end - data->start);
      }
      else if(data->start <= start->index && start->index <= data->end) {
        token_or_span = pmath_string_part(
                 token_or_span,
                 0,
                 data->end - start->index);
      }
      else if(data->start <= end->index && end->index <= data->end) {
        token_or_span = pmath_string_part(
                 token_or_span,
                 data->start - start->index,
                 end->index - data->start);
      }
    }
  }
  
  if(!has(data->flags, BoxOutputFlags::WithDebugMetadata))
    return token_or_span;
  
  if(!pmath_is_expr(token_or_span) && !pmath_is_string(token_or_span))
    return token_or_span;
  
  Expr debug_metadata = SelectionReference(data->sequence->id(), start->index, end->index).to_pmath();
                      
  token_or_span = pmath_try_set_debug_metadata(
                    token_or_span,
                    debug_metadata.release());
                    
  return token_or_span;
}

pmath_t MathSequence::Impl::remove_null_tokens(pmath_t boxes) {
  while(true) {
    if(pmath_is_expr_of(boxes, richmath_System_List)) {
      size_t first = 1;
      size_t length = pmath_expr_length(boxes);
      size_t last = length;
      
      for(; last >= first; --last) {
        pmath_t item = pmath_expr_get_item(boxes, last);
        bool is_null_token = pmath_is_str0(item);
        pmath_unref(item);
        if(!is_null_token)
          break;
      }
      for(; first <= last; ++first) {
        pmath_t item = pmath_expr_get_item(boxes, first);
        bool is_null_token = pmath_is_str0(item);
        pmath_unref(item);
        if(!is_null_token)
          break;
      }
      
      if(first > last) {
        pmath_unref(boxes);
        return PMATH_FROM_TAG(PMATH_TAG_STR0, 0);
      }
      
      if(first == last) {
        pmath_t item = pmath_expr_get_item(boxes, first);
        pmath_unref(boxes);
        boxes = item;
        continue;
      }
      
      if(first == 1 && last == length)
        return boxes;
      
      if(first < last) {
        pmath_t items = pmath_expr_get_item_range(boxes, first, last - first + 1);
        pmath_unref(boxes);
        return items;
      }
    }
    return boxes;
  }
}


void MathSequence::Impl::box_size(InlineSpanPainting &isp, Context &context, const GlyphIterator &pos, float *a, float *d) {
  if(!pos.has_more_glyphs())
    return;
  
  if(Box *box = pos.current_box()) {
    box->extents().bigger_y(a, d);
    return;
  }
  
  isp.switch_to_sequence(context, pos.current_sequence(), DisplayStage::Layout);
  if(pos.current_glyph().is_normal_text) {
    context.text_shaper->vertical_glyph_size(
      context,
      pos.current_char(),
      pos.current_glyph(),
      a,
      d);
  }
  else {
    context.math_shaper->vertical_glyph_size(
      context,
      pos.current_char(),
      pos.current_glyph(),
      a,
      d);
  }
}

void MathSequence::Impl::boxes_size(InlineSpanPainting &isp, Context &context, GlyphIterator start, const GlyphIterator &end, float *a, float *d) {
  for(;start.glyph_index() < end.glyph_index(); start.move_next_glyph()) {
    box_size(isp, context, start, a, d);
  }
}

void MathSequence::Impl::generate_glyphs(Context &context) {
  self.glyph_to_text.clear();
  self.glyph_to_inline_sequence.clear();
  self.glyphs.length(0);
  
  GlyphGenerator(self).append_all(context);
}

inline MathSequence &MathSequence::Impl::outermost_span() {
  return self.inline_span() ? parent_outermost_span() : self;
}

inline MathSequence &MathSequence::Impl::parent_outermost_span() {
  for(Box *box = self.parent(); box; box = box->parent()) {
    if(auto seq = dynamic_cast<MathSequence*>(box)) {
      if(!seq->inline_span())
        return *seq;
    }
  }
  
  //pmath_debug_print("[inconstistent inline_span() flag]\n");
  return self;
}

inline GlyphIterator MathSequence::Impl::glyph_iterator() {
  return GlyphIterator{outermost_span()};
}

void MathSequence::Impl::stretch_all_vertical(Context &context) {
  GlyphHeights core_heights{};
  GlyphHeights heights{};
  VerticalStretcher(context, self).stretch_all(core_heights, heights);
}

void MathSequence::Impl::substitute_glyphs(
  Context              &context,
  uint32_t              math_script_tag,
  uint32_t              math_language_tag,
  uint32_t              text_script_tag,
  uint32_t              text_language_tag,
  const FontFeatureSet &features
) {
  if(features.empty())
    return;
  
  //const uint16_t *buf = self.str.buffer();
  cairo_text_extents_t cte;
  cairo_glyph_t        cg;
  cg.x = cg.y = 0;
  
  InlineSpanPainting isp{&self};
  
  GlyphIterator run_start{self};
  while(run_start.has_more_glyphs()) {
    if(run_start.current_glyph().composed || run_start.current_char() == PMATH_CHAR_BOX) {
      run_start.move_next_glyph();
      continue;
    }
    
    GlyphIterator next_run = run_start;
    next_run.move_next_glyph();
    for(; next_run.has_more_glyphs(); next_run.move_next_glyph()) {
      if(next_run.current_glyph().composed || next_run.current_char() == PMATH_CHAR_BOX)
        break;
        
      if(run_start.current_sequence() != next_run.current_sequence())
        break;
      if(run_start.current_glyph().fontinfo != next_run.current_glyph().fontinfo)
        break;
      if(run_start.current_glyph().slant != next_run.current_glyph().slant)
        break;
      if(run_start.current_glyph().is_normal_text != next_run.current_glyph().is_normal_text)
        break;
    }
    
    uint32_t script_tag, language_tag;
    SharedPtr<TextShaper> shaper;
    
    isp.switch_to_sequence(context, run_start.current_sequence(), DisplayStage::Layout);
    
    if(run_start.current_glyph().is_normal_text) {
      shaper = context.text_shaper;
      script_tag   = text_script_tag;
      language_tag = text_language_tag;
    }
    else {
      shaper = context.math_shaper;
      script_tag   = math_script_tag;
      language_tag = math_language_tag;
    }
    
    FontFace face = shaper->font(run_start.current_glyph().fontinfo);
    FontInfo info(face);
    
    if(const auto gsub = info.get_gsub_table()) {
      static Array<OTFontReshaper::IndexAndValue> lookups;
      lookups.length(0);
      
      OTFontReshaper::get_lookups(
        gsub,
        script_tag,
        language_tag,
        features,
        &lookups);
        
      if(lookups.length() > 0) {
        context.canvas().set_font_face(face);
        
        static OTFontReshaper reshaper;
        
        reshaper.glyphs.length(    next_run.glyph_index() - run_start.glyph_index());
        reshaper.glyph_info.length(next_run.glyph_index() - run_start.glyph_index());
        
        reshaper.glyphs.length(0);
        reshaper.glyph_info.length(0);
        
        for(int i = run_start.glyph_index(); i < next_run.glyph_index(); ++i) {
          if(self.glyphs[i].index == IgnoreGlyph)
            continue;
            
          reshaper.glyphs.add(self.glyphs[i].index);
          reshaper.glyph_info.add(i); // TODO: fill with glyph_to_text[i]
        }
        
        reshaper.apply_lookups(gsub, lookups);
        
        RICHMATH_ASSERT(reshaper.glyphs.length() == reshaper.glyph_info.length());
        
        int len = reshaper.glyphs.length();
        int i = 0;
        while(i < len) {
          int i2 = i + 1;
          
          int pos = reshaper.glyph_info[i];
          
          if(i2 < len && reshaper.glyph_info[i2] == pos) {
            // no room for "one to many" substitution
            
            ++i2;
            while(i2 < len && reshaper.glyph_info[i2] == pos)
              ++i2;
              
            i = i2;
            continue;
          }
          
          int next;
          if(i2 < len)
            next = reshaper.glyph_info[i2];
          else
            next = next_run.glyph_index();
            
          RICHMATH_ASSERT(pos < next);
          
          if(self.glyphs[pos].index != reshaper.glyphs[i]) {
            cg.index = self.glyphs[pos].index = reshaper.glyphs[i];
            
            context.canvas().glyph_extents(&cg, 1, &cte);
            cte.x_advance /= (next - pos);
            self.glyphs[pos].right = cte.x_advance;
            
            for(int j = pos + 1; j < next; ++j) {
              self.glyphs[j].right = cte.x_advance;
              self.glyphs[j].index = IgnoreGlyph;
            }
          }
          
          i = i2;
        }
      }
    }
    run_start = next_run;
  }
  
  isp.switch_to_sequence(context, &self, DisplayStage::Layout);
}

void MathSequence::Impl::apply_glyph_substitutions(Context &context) {
  if(context.fontfeatures.empty())
    return;
    
  int old_ssty_feature_value = context.fontfeatures.feature_value(FontFeatureSet::TAG_ssty);
  if(old_ssty_feature_value < 0) {
    if(context.script_level >= 2)
      context.fontfeatures.set_feature(FontFeatureSet::TAG_ssty, context.script_level - 1);
    else
      context.fontfeatures.set_feature(FontFeatureSet::TAG_ssty, 0);
  }
    
  /* TODO: infer script ("math") and language ("dflt") from style/context.
   */
  
  substitute_glyphs(
    context,
    OTFontReshaper::SCRIPT_math,
    OTFontReshaper::LANG_dflt,
    OTFontReshaper::SCRIPT_latn, //OTFontReshaper::SCRIPT_DFLT
    OTFontReshaper::LANG_dflt,
    context.fontfeatures);
    
  substitute_glyphs(
    context,
    OTFontReshaper::SCRIPT_latn,
    OTFontReshaper::LANG_dflt,
    OTFontReshaper::SCRIPT_latn, //OTFontReshaper::SCRIPT_DFLT
    OTFontReshaper::LANG_dflt,
    context.fontfeatures);
    
  context.fontfeatures.set_feature(FontFeatureSet::TAG_ssty, old_ssty_feature_value);
}

bool MathSequence::Impl::hstretch_lines( // return whether there was any height change
  float width,
  float window_width,
  float *unfilled_width
) {
  bool has_any_height_change = false;
  *unfilled_width = -HUGE_VAL;
  
  if(width == HUGE_VAL) {
    if(window_width == HUGE_VAL)
      return has_any_height_change;
      
    width = window_width;
  }
  
  float delta_x = 0.0f;
  GlyphIterator start{self};
  for(int line = 0; line < self.lines.length(); ++line) {
    float total_fill_weight = 0.0f;
    float white = 0.0f;
    
    auto next = start;
    for(; next.glyph_index() < self.lines[line].end; next.move_next_glyph()) {
      if(auto filler = next.current_box()) {
        float weight = filler->fill_weight();
        if(weight > 0) {
          total_fill_weight += weight;
          white += filler->extents().width;
        }
      }
      
      next.current_glyph().right += delta_x;
    }
    
    float line_width = self.indention_width(line);
    if(start.glyph_index() > 0)
      line_width -= self.glyphs[start.glyph_index() - 1].right;
    if(self.lines[line].end > 0)
      line_width += self.glyphs[self.lines[line].end - 1].right;
      
    if(total_fill_weight > 0) {
      if(width - line_width > 0) {
        float dx = 0;
        
        white += width - line_width;
        
        for(auto pos = start; pos.glyph_index() < self.lines[line].end; pos.move_next_glyph()) {
          if(auto filler = pos.current_box()) {
            float weight = filler->fill_weight();
            if(weight > 0) {
              BoxSize size = filler->extents();
              dx -= size.width;
              
              size.width = white * weight / total_fill_weight;
              filler->expand(size);
              dx += filler->extents().width;
              
              auto fa = filler->extents().ascent;
              auto fd = filler->extents().descent;
              has_any_height_change = has_any_height_change || size.ascent != fa || size.descent != fd;
              
              if(self.lines[line].ascent  < fa) self.lines[line].ascent = fa;
              if(self.lines[line].descent < fd) self.lines[line].descent = fd;
            }
          }
          
          pos.current_glyph().right += dx;
        }
        
        delta_x += dx;
      }
    }
    
    line_width += self.indention_width(self.lines[line].indent);
    if(*unfilled_width < line_width)
      *unfilled_width = line_width;
      
    start = next;
  }
  return has_any_height_change;
}


void MathSequence::Impl::new_line(int glyph_index, unsigned int indent, bool continuation) {
  int len = self.lines.length();
  if( self.lines[len - 1].end < glyph_index ||
      glyph_index == 0 ||
      (len >= 2 && self.lines[len - 2].end >= glyph_index))
  {
    return;
  }
  
  self.lines.length(len + 1);
  self.lines[len].end = self.lines[len - 1].end;
  self.lines[len - 1].end = glyph_index;
  self.lines[len - 1].continuation = continuation;
  
  self.lines[len].ascent = self.lines[len].descent = 0;
  self.lines[len].indent = indent;
  self.lines[len].continuation = 0;
  return;
}


void MathSequence::Impl::split_lines(Context &context) {
  if(self.glyphs.length() == 0)
    return;
  
  IndentLines::indention_array.length(self.glyphs.length() + 1);
  IndentLines::indention_array.zeromem();
  IndentLines(self).visit_all();
  IndentLines::indention_array[self.glyphs.length()] = IndentLines::indention_array[self.glyphs.length() - 1];
  
  PenalizeBreaks::penalty_array.length(self.glyphs.length());
  PenalizeBreaks::penalty_array.zeromem();
  PenalizeBreaks(self).visit_all();
  
  //if(buf[self.glyphs.length() - 1] != '\n')
  PenalizeBreaks::penalty_array[self.glyphs.length() - 1] = HUGE_VAL;
    
  self._extents.width = context.width;
  for(GlyphIterator start_of_paragraph{self}; start_of_paragraph.has_more_glyphs();) {
    auto end_of_paragraph = start_of_paragraph;
    
    bool end_with_nl = false;
    while(end_of_paragraph.has_more_glyphs()) {
      if(end_of_paragraph.current_char() == '\n') {
        end_with_nl = true;
        end_of_paragraph.move_next_glyph();
        break;
      }
      end_of_paragraph.move_next_glyph();
    }
    
    break_array.length(0);
    break_array.add(BreakPositionWithPenalty(start_of_paragraph.glyph_index() - 1, 0, 0.0));
    
    for(auto pos = start_of_paragraph; pos.glyph_index() < end_of_paragraph.glyph_index(); pos.move_next_glyph()) {
      float xend = pos.current_glyph().right;
      
      int current = break_array.length();
      break_array.add(BreakPositionWithPenalty(pos.glyph_index(), -1, Infinity));
      //break_array.add(BreakPositionWithPenalty(pos, -1, PenalizeBreaks::penalty_array[pos.glyph_index()]));
      
      for(int i = current - 1; i >= 0; --i) {
        float xstart    = 0;
        float indention = 0;
        double penalty  = break_array[i].penalty;
        if(break_array[i].glyph_position >= 0) {
          int gp = break_array[i].glyph_position;
          xstart    = self.glyphs[gp].right;
          indention = self.indention_width(IndentLines::indention_array[gp + 1]);
          penalty  += PenalizeBreaks::penalty_array[gp];
        }
        
        if(xend - xstart + indention > context.width && i + 1 < current)
          break;
          
        double best = context.width * PenalizeBreaks::BestLineWidth;
        if(pos.glyph_index() + 1 < end_of_paragraph.glyph_index() || best < xend - xstart + indention) {
          double factor = 0.0;
          if(context.width > 0)
            factor = PenalizeBreaks::LineWidthFactor / context.width;
          double rel_amplitude = ((xend - xstart + indention) - best) * factor;
          penalty += rel_amplitude * rel_amplitude;
        }
        
        if(penalty < break_array[current].penalty) {
          break_array[current].penalty = penalty;
          break_array[current].prev_break_index = i;
        }
      }
    }
    
    int mini = break_array.length() - 1;
// TODO: What is this code meant for:
//    for(int i = mini - 1; i >= 0 && break_array[i].glyph_position + 1 == end_of_paragraph.glyph_index(); --i) {
//      if(break_array[i].penalty < break_array[mini].penalty)
//        mini = i;
//    }
    
    if(mini >= 0 && !end_with_nl)
      mini = break_array[mini].prev_break_index;
      
    break_result.length(0);
    for(int i = mini; i > 0; i = break_array[i].prev_break_index)
      break_result.add(i);
      
    for(int i = break_result.length() - 1; i >= 0; --i) {
      int j = break_result[i];
      GlyphIterator pos{self};
      pos.move_to_glyph(break_array[j].glyph_position);
      
      bool continuation = !pos.is_at_token_end();
      while(pos.glyph_index() < end_of_paragraph.glyph_index()) {
        pos.move_next_glyph();
        if(pos.current_char() != ' ')
          break;
      }
       
      if(pos.glyph_index() <= end_of_paragraph.glyph_index()) {
        new_line(
          pos.glyph_index(),
          IndentLines::indention_array[pos.glyph_index()],
          continuation);
      }
    }
    
    start_of_paragraph = end_of_paragraph;
  }
  
  // Move FillBoxes to the beginning of the next line,
  // so aaaaaaaaaaaaaa.........bbbbb will become
  //    aaaaaaaaaaaaaa
  //    ............bbbbb
  // when the window is resized.
  GlyphIterator prev(self);
  for(int line = 0; line < self.lines.length() - 1; ++line) {
    if(self.lines[line].end == 0)
      continue;
    
    prev.move_to_glyph(self.lines[line].end - 1);
    if(Box *filler = prev.current_box()) {
      if(filler->fill_weight() > 0) {
        auto next = prev;
        next.move_next_glyph();
        if(Box *next_filler = next.current_box()) {
          if(next_filler->fill_weight() > 0)
            continue;
        }
        
        float w = self.glyphs[self.lines[line + 1].end - 1].right - self.glyphs[self.lines[line].end - 1].right;
        
        if(filler->extents().width + w + self.indention_width(self.lines[line + 1].indent) <= context.width) {
          self.lines[line].end--;
        }
      }
    }
  }
}

void MathSequence::Impl::calculate_line_heights(Context &context, InlineSpanPainting &isp) {
  int line = 0;
  GlyphIterator iter{self};
  if(self.lines.length() > 1) {
    self.lines[0].ascent  = 0.75f * self.em;
    self.lines[0].descent = 0.25f * self.em;
  }
  while(iter.has_more_glyphs()) {
    if(iter.glyph_index() == self.lines[line].end) {
      ++line;
      self.lines[line].ascent  = 0.75f * self.em;
      self.lines[line].descent = 0.25f * self.em;
    }
    
    isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
    
    if(auto box = iter.current_box()) {
      box->extents().bigger_y(&self.lines[line].ascent, &self.lines[line].descent);
    }
    else if(iter.current_glyph().is_normal_text) {
      context.text_shaper->vertical_glyph_size(
        context,
        iter.current_char(),
        iter.current_glyph(),
        &self.lines[line].ascent,
        &self.lines[line].descent);
    }
    else {
      context.math_shaper->vertical_glyph_size(
        context,
        iter.current_char(),
        iter.current_glyph(),
        &self.lines[line].ascent,
        &self.lines[line].descent);
    }
    
    iter.move_next_glyph();
  }
  
  if(line + 1 < self.lines.length()) {
    ++line;
    self.lines[line].ascent = 0.75f * self.em;
    self.lines[line].descent = 0.25f * self.em;
  }
}

void MathSequence::Impl::calculate_total_extents_from_lines() {
  int line = 0;
  int pos = 0;
  float x = 0.0f;
  self._extents.width = 0.0f;
  self._extents.ascent = 0.0f;
  self._extents.descent = 0.0f;
  while(pos < self.glyphs.length()) {
    if(pos == self.lines[line].end) {
      if(pos > 0) {
        double indent = self.indention_width(self.lines[line].indent);
        
        if(self._extents.width < self.glyphs[pos - 1].right - x + indent)
          self._extents.width  = self.glyphs[pos - 1].right - x + indent;
        
        x = self.glyphs[pos - 1].right;
      }
      
      self._extents.descent += self.lines[line].ascent + self.lines[line].descent + self.line_spacing();
      ++line;
    }
    
    ++pos;
  }
  
  if(pos > 0) {
    double indent = self.indention_width(self.lines[line].indent);
    
    if(self._extents.width < self.glyphs[pos - 1].right - x + indent)
      self._extents.width  = self.glyphs[pos - 1].right - x + indent;
  }
  
  if(self._extents.width < 0.75 && self.lines.length() > 1) {
    self._extents.width = 0.75;
  }
  
  if(line + 1 < self.lines.length()) {
    self._extents.descent += self.lines[line].ascent + self.lines[line].descent;
    ++line;
  }
  self._extents.ascent = self.lines[0].ascent;
  self._extents.descent += self.lines[line].ascent + self.lines[line].descent - self.lines[0].ascent;
  
  //// round ascent/descent to next multiple of 0.75pt (= 1px by default):
  //self._extents.ascent  = ceilf(self._extents.ascent  / 0.75f ) * 0.75f;
  //self._extents.descent = ceilf(self._extents.descent / 0.75f ) * 0.75f;
}

//} ... class MathSequence::Impl

//{ class MathSequence::Impl::GlyphGenerator ...

MathSequence::Impl::GlyphGenerator::GlyphGenerator(MathSequence &owner)
  : isp{&owner},
    owner{owner},
    g2t_iter{owner.glyph_to_text.begin()},
    g2seq_iter{owner.glyph_to_inline_sequence.begin()}
{
}

inline void MathSequence::Impl::GlyphGenerator::append_all(Context &context) {
  isp.switch_to_sequence(context, &owner, DisplayStage::Layout);
  append_all(context, owner);
  isp.switch_to_sequence(context, &owner, DisplayStage::Layout);
}

void MathSequence::Impl::GlyphGenerator::append_all(Context &context, MathSequence &seq) {
  int box = 0;
  int pos = 0;
  const int len = seq.length();
  while(pos < len)
    append_span(context, seq, seq.spans[pos], &pos, &box);
}

void MathSequence::Impl::GlyphGenerator::append_span(Context &context, MathSequence &span_seq, Span span, int *pos, int *box) {
  if(!span) {
    if(span_seq.str[*pos] == PMATH_CHAR_BOX) {
      append_box_glyphs(context, span_seq, *pos, span_seq.boxes[*box]);
      ++*box;
      ++*pos;
      return;
    }
    
    const int len = span_seq.length();
    int next  = *pos;
    while(next < len && !span_seq.spans.is_token_end(next))
      ++next;
      
    if(next < len)
      ++next;
      
    append_text_glyph_run(context, span_seq, *pos, next - *pos);
    
    *pos = next;
    return;
  }
  
  if(!span.next() && span_seq.str[*pos] == '"') {
    const uint16_t *buf = span_seq.str.buffer();
    int end = span.end();
    if(!context.show_string_characters) {
      ++*pos;
      if(buf[end] == '"')
        --end;
    }
    else {
      append_text_glyph_run(context, span_seq, *pos, 1);
      
      ++*pos;
        
      if(*pos <= end && buf[end] == '"') {
        --end;
      }
    }
    
    bool old_math_styling = context.math_spacing;
    context.math_spacing = false;
    
    while(*pos <= end) {
      if(buf[*pos] == PMATH_CHAR_BOX) {
        append_box_glyphs(context, span_seq, *pos, span_seq.boxes[*box]);
        ++*box;
        ++*pos;
      }
      else {
        int next = *pos;
        while(next <= end && !span_seq.spans.is_token_end(next))
          ++next;
        ++next;
        
        if(!context.show_string_characters && buf[*pos] == '\\') {
          if(*pos + 1 < next && (buf[*pos+1] == '\"' || buf[*pos+1] == '\\'))
            ++*pos;
        }
        
        append_text_glyph_run(context, span_seq, *pos, next - *pos);
        
        *pos = next;
      }
    }
    
    context.math_spacing = old_math_styling;
    
    if(context.show_string_characters && *pos == span.end()) { // trailing "
      append_text_glyph_run(context, span_seq, *pos, 1);
    }
    
    *pos = span.end() + 1;
  }
  else {
    append_span(context, span_seq, span.next(), pos, box);
    while(*pos <= span.end())
      append_span(context, span_seq, span_seq.spans[*pos], pos, box);
  }
}

void MathSequence::Impl::GlyphGenerator::append_box_glyphs(Context &context, MathSequence &seq, int pos, Box *box) {
  if(auto sub = box->as_inline_span()) {
    box->resize_inline(context); // TODO: get rid of this call
    if(!owner.has_text_shadow_span()) {
      Expr expr;
      if(box->style && context.stylesheet->get(box->style, TextShadow, &expr)) {
        owner.has_text_shadow_span(true);
      }
    }
    isp.switch_to_sequence(context, sub, DisplayStage::Layout);
    sub->em = context.canvas().get_font_size();
    sub->inline_span(true);
    sub->ensure_boxes_valid();
    sub->ensure_spans_valid();
    append_all(context, *sub);
    isp.switch_to_sequence(context, &seq, DisplayStage::Layout);
    return;
  }
  
  box->resize(context);
  
  append_empty_glyphs(seq, pos, 1);
  GlyphInfo &gi = owner.glyphs[owner.glyphs.length() - 1];
  
  gi.right = box->extents().width;
  gi.composed = 1;
}

void MathSequence::Impl::GlyphGenerator::append_text_glyph_run(Context &context, MathSequence &seq, int pos, int count) {
  const uint16_t *buf = seq.str.buffer() + pos;
  
  int glyph_start = owner.glyphs.length();
  if(context.math_spacing) {
    append_empty_glyphs(seq, pos, count);
    
    GlyphInfo *glyph_items = owner.glyphs.items() + glyph_start;
    
    switch(count) {
      case 2:
        switch(buf[0]) {
          case '|':
            if(buf[1] == '>') { // |>
              static const uint16_t liga[2] = { '|', 0x203A }; // U+203A right single guillemet
              context.math_shaper->decode_token(context, 2, liga, glyph_items);
              return;
            }
            break;
            
          case '[':
            if(buf[1] == '[') {
              static const uint16_t liga[2] = {' ', 0x27E6 }; // U+27E6 [[
              context.math_shaper->decode_token(context, 2, liga, glyph_items);
              glyph_items[0].right = glyph_items[1].right /= 2;
              glyph_items[1].x_offset = -glyph_items[0].right;
              return;
            }
            break;
            
          case ']':
            if(buf[1] == ']') {
              static const uint16_t liga[2] = {' ', 0x27E7 }; // U+27E7 ]]
              context.math_shaper->decode_token(context, 2, liga, glyph_items);
              glyph_items[0].right = glyph_items[1].right /= 2;
              glyph_items[1].x_offset = -glyph_items[0].right;
              return;
            }
            break;
        }
        break;
    }
    
    context.math_shaper->decode_token(
      context,
      count,
      buf,
      glyph_items);
  }
  else {
    append_empty_glyphs(seq, pos, count);
    context.text_shaper->decode_token(
      context,
      count,
      buf,
      owner.glyphs.items() + glyph_start);
      
    for(int i = glyph_start; i < owner.glyphs.length(); ++i) {
      if(owner.glyphs[i].index) {
        owner.glyphs[i].is_normal_text = 1;
      }
      else if(is_utf16_low(buf[i - glyph_start])) {
        owner.glyphs[i].is_normal_text = 1;
      }
      else {
        context.math_shaper->decode_token(
          context,
          1,
          buf + (i - glyph_start),
          owner.glyphs.items() + i);
      }
    }
  }
}

void MathSequence::Impl::GlyphGenerator::append_empty_glyphs(MathSequence &seq, int pos, int count) {
  ARRAY_ASSERT(count >= 0);
  
  int old_len = owner.glyphs.length();
  
  owner.glyphs.length(old_len + count);
  memset(owner.glyphs.items() + old_len, 0, sizeof(owner.glyphs[0]) * count);
  
  g2t_iter.rewind_to(old_len);
  g2t_iter.reset_rest(pos);
  
  g2seq_iter.rewind_to(old_len);
  g2seq_iter.reset_rest(&seq == &owner ? nullptr : &seq);
}

//} ... class MathSequence::Impl::GlyphGenerator

//{ class MathSequence::Impl::VerticalStretcher ...

MathSequence::Impl::VerticalStretcher::VerticalStretcher(Context &context, MathSequence &seq)
  : context{context},
    iter{seq},
    isp{&seq}
{
}

void MathSequence::Impl::VerticalStretcher::stretch_all(GlyphHeights &core_heights, GlyphHeights &heights) {
  while(iter.has_more_glyphs())
    stretch_outermost_span(core_heights, heights);
  
  isp.switch_to_sequence(context, iter.outermost_sequence(), DisplayStage::Layout);
}

void MathSequence::Impl::VerticalStretcher::stretch_outermost_span(GlyphHeights &core_heights, GlyphHeights &heights) {
  ARRAY_ASSERT(iter.has_more_glyphs());

  // For single tokens wrappend in TagBox(...) etc., move up the TagBox/... to support constructs like TagBox(")",None,SyntaxForm->"(")
  MathSequence *outermost = iter.outermost_sequence();
  MathSequence *cur_seq   = iter.current_sequence();
  if(cur_seq != outermost && iter.text_index() == 0) {
    VolatileSelection sel(cur_seq, 0);
    sel.expand_to_parent();
    while(sel) {
      if(auto outer = dynamic_cast<MathSequence*>(sel.box)) {
        if(outer == outermost || outer->length() > 1) { // outer contains more than sel
          stretch_span(outer, outer->span_array()[sel.start], core_heights, heights);
          return;
        }
      }
      if(sel.box == outermost)
        break;
      sel.expand_to_parent();
    }
  }

  stretch_span(iter.current_sequence(), iter.text_span_array()[iter.text_index()], core_heights, heights);
}

void MathSequence::Impl::VerticalStretcher::stretch_span(MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights) {
  if(span) {
    stretch_span_start(span_seq, span, core_heights, heights);
    try_stretch_division_span_rest(span_seq, span, core_heights, heights);
    stretch_span_rest(span_seq, span, core_heights, heights);
    return;
  }
  
  if(auto box = iter.current_box()) {
    stretch_nonspan_box(box, core_heights, heights);
    return;
  }
  
  if(iter.is_operand_start() && iter.text_buffer_length() > 1) {
    uint16_t ch = iter.current_char();
    if(pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)) { 
      // Bigop that does not start a span. This happens when no further text comes afterwards. 
      GlyphInfo &gi = iter.current_glyph();
      isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
      
      context.math_shaper->vertical_stretch_char(
        context, 0, 0,
        true,
        ch, &gi);
        
      GlyphHeights size;
      context.math_shaper->vertical_glyph_size(
        context, ch, gi,
        &size.ascent,
        &size.descent);
        
      if(core_heights.ascent  < size.ascent)  core_heights.ascent  = size.ascent;
      if(core_heights.descent < size.descent) core_heights.descent = size.descent;
      if(     heights.ascent  < size.ascent)       heights.ascent  = size.ascent;
      if(     heights.descent < size.descent)      heights.descent = size.descent;
      
      iter.move_next_token();
      return;
    }
  }
  
  size_nonspan_token(span_seq, core_heights);
  
  if(heights.ascent  < core_heights.ascent)  heights.ascent  = core_heights.ascent;
  if(heights.descent < core_heights.descent) heights.descent = core_heights.descent;
}

void MathSequence::Impl::VerticalStretcher::stretch_span_start(MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights) {
  ARRAY_ASSERT(span);
  
  if(auto next = span.next()) {
    stretch_span(span_seq, next, core_heights, heights);
    return;
  }
  
  MathSequence *cur_seq = iter.current_sequence();
  if(cur_seq != span_seq && span_seq->is_parent_of(cur_seq)) {
    if(iter.text_index() == 0) {
      VolatileSelection sel(cur_seq, 0);
      sel.expand_to_parent();
      while(sel && sel.box != span_seq) {
        if(auto outer = dynamic_cast<MathSequence*>(sel.box)) {
          if(outer->length() > 1) { // outer contains more than sel
            stretch_span(outer, outer->span_array()[sel.start], core_heights, heights);
            return;
          }
        }
        sel.expand_to_parent();
      }
    }
  }

  uint16_t ch = iter.current_char();
  
  int span_end = span.end();
  
  if(ch == '"') {
    iter.skip_forward_to_glyph_after_current_text_pos(span_end + 1);
    return;
  }
  
  // TODO: we should not need to look at the SyntaxForm in VerticalStretcher and instead stretch any character that is stretchable
  String start_syntax_form = iter.find_syntax_form();
  if(auto box = iter.current_box()) {
    auto underover = dynamic_cast<UnderoverscriptBox*>(box);
    if(underover && underover->base()->length() == 1 && underover->base()->glyph_array().length() == 1)
      ch = underover->base()->str[0];
  }
  
  auto start_iter = iter;
  
  if(Tokenizer::is_left_bracket(start_syntax_form)) { // usually ==pmath_char_is_left(ch)
    GlyphHeights inner_core_heights {};
    GlyphHeights inner_heights {};
    
    iter.move_next_token();
    while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end && !pmath_char_is_right(iter.current_char())) {
      stretch_outermost_span(inner_core_heights, inner_heights);
    }
    
    float overhang_a = (inner_heights.ascent - inner_core_heights.ascent) * UnderoverscriptOverhangCoverage;
    float overhang_d = (inner_heights.descent - inner_core_heights.descent) * UnderoverscriptOverhangCoverage;
    
    float new_ca = inner_heights.ascent + overhang_a;
    float new_cd = inner_heights.descent + overhang_d;
    
    bool full_stretch = true;
    if(iter.index_in_sequence(span_seq, span_end + 1) <= span_end) {
      uint16_t end_ch          = iter.current_char();
      String   end_syntax_form = iter.find_syntax_form();
      if(Tokenizer::is_right_bracket(end_syntax_form)) { // usually ==pmath_char_is_right(end_ch)
        if(ch == '{' && end_ch == '}')
          full_stretch = false;
        //if(start_syntax_form.equals("{") && end_syntax_form.equals("}"))
        //  full_stretch = false;

        isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
        context.math_shaper->vertical_stretch_char(
          context, new_ca, new_cd, full_stretch, end_ch, &iter.current_glyph());
          
        iter.move_next_token();
      }
    }
    
    // caution: ch might come from an UnderOverscriptBox
    isp.switch_to_sequence(context, start_iter.current_sequence(), DisplayStage::Layout);
    context.math_shaper->vertical_stretch_char(
      context, new_ca, new_cd, full_stretch, ch, &start_iter.current_glyph());
      
    if(heights.ascent  < inner_heights.ascent)  heights.ascent  = inner_heights.ascent;
    if(heights.descent < inner_heights.descent) heights.descent = inner_heights.descent;
      
    if(core_heights.ascent  < new_ca) core_heights.ascent  = new_ca;
    if(core_heights.descent < new_cd) core_heights.descent = new_cd;
  }
  else if(pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)) {
    GlyphHeights inner_core_heights {};
    
    UnderoverscriptBox *underover = nullptr;
    SubsuperscriptBox *subsuper = nullptr;
    if(auto box = iter.current_box()) {
      underover = dynamic_cast<UnderoverscriptBox*>(box);
      ARRAY_ASSERT(underover != nullptr); // otherwise ch would be a CHAR_BOX, see above.
      iter.move_next_token();
    }
    else {
      iter.move_next_token();
      if(auto box = iter.current_box()) { // Note: checking iter.has_more_glyphs() not neccessary
        subsuper = dynamic_cast<SubsuperscriptBox *>(box);
        if(subsuper)
          iter.move_next_token();
      }
    }
    
    while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end) {
      stretch_outermost_span(inner_core_heights, heights);
    }
    
    isp.switch_to_sequence(context, start_iter.current_sequence(), DisplayStage::Layout);
    if(underover) {
      ARRAY_ASSERT(underover->base()->glyph_array().length() == 1);
      
      GlyphInfo &gi = underover->base()->glyph_array()[0];
      
      context.math_shaper->vertical_stretch_char(
        context,
        inner_core_heights.ascent,
        inner_core_heights.descent,
        true, 
        ch, &gi);
        
      context.math_shaper->vertical_glyph_size(
        context, ch, gi,
        &underover->base()->_extents.ascent,
        &underover->base()->_extents.descent);
        
      underover->base()->_extents.width = gi.right;
      
      underover->after_items_resize(context);
      
      start_iter.current_glyph().right = underover->extents().width;
      
      underover->base()->extents().bigger_y(&core_heights.ascent, &core_heights.descent);
      underover->extents().bigger_y(             &heights.ascent,      &heights.descent);
    }
    else {
      GlyphInfo &gi = start_iter.current_glyph();
      
      context.math_shaper->vertical_stretch_char(
        context,
        inner_core_heights.ascent,
        inner_core_heights.descent,
        true,
        ch, &gi);
        
      BoxSize size;
      context.math_shaper->vertical_glyph_size(
        context, ch, gi,
        &size.ascent,
        &size.descent);
        
      size.bigger_y(&core_heights.ascent, &core_heights.descent);
      size.bigger_y(     &heights.ascent,      &heights.descent);
      
      if(subsuper) {
        subsuper->stretch(context, size);
        subsuper->extents().bigger_y(&heights.ascent, &heights.descent);
        
        subsuper->adjust_x(context, ch, gi);
      }
    }
    
    if(core_heights.ascent  < inner_core_heights.ascent)  core_heights.ascent  = inner_core_heights.ascent;
    if(core_heights.descent < inner_core_heights.descent) core_heights.descent = inner_core_heights.descent;
  }
  else
    stretch_span(span_seq, span.next(), core_heights, heights);
}

void MathSequence::Impl::VerticalStretcher::try_stretch_division_span_rest(MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights) {
  if(!iter.has_more_glyphs())
    return;
  
  uint16_t ch = iter.current_char();
  if(ch != '/')
    return;
  
  if(!iter.is_at_token_end()) 
    return;
  
  auto division_iter = iter;
  
  int span_end = span.end();
  iter.move_next_token();
  while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end)
    stretch_outermost_span(core_heights, heights);
  
  GlyphInfo &gi = division_iter.current_glyph();
  isp.switch_to_sequence(context, division_iter.current_sequence(), DisplayStage::Layout);
  context.math_shaper->vertical_stretch_char(
    context,
    core_heights.ascent  - 0.1 * span_seq->em,
    core_heights.descent - 0.1 * span_seq->em,
    true,
    ch, &gi);
    
  BoxSize size;
  context.math_shaper->vertical_glyph_size(
    context,
    ch, gi,
    &size.ascent,
    &size.descent);
    
  size.bigger_y(&core_heights.ascent, &core_heights.descent);
  size.bigger_y(     &heights.ascent,      &heights.descent);
}

void MathSequence::Impl::VerticalStretcher::stretch_span_rest(MathSequence *span_seq, Span span, GlyphHeights &core_heights, GlyphHeights &heights) {
  int span_end = span.end();
  while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end) {
    if(iter.is_left_bracket()) {
      // NOTE: old code checked if there was a span starting here instead of looking for operand start
      if(!iter.is_operand_start())
        break; // opening parenthesis in function call "f(..." or "x.f(..."
    }
    stretch_outermost_span(core_heights, heights);
  }
  
  if(iter.index_in_sequence(span_seq, span_end + 1) < span_end) {
    auto call_paren_start = iter;
    
    GlyphHeights inner_core_heights {};
    GlyphHeights inner_heights {};
    
    iter.move_next_token();
    while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end && !pmath_char_is_right(iter.current_char()))
      stretch_outermost_span(inner_core_heights, inner_heights);
    
    float overhang_a = (inner_heights.ascent - inner_core_heights.ascent) * UnderoverscriptOverhangCoverage;
    float overhang_d = (inner_heights.descent - inner_core_heights.descent) * UnderoverscriptOverhangCoverage;
    
    float new_ca = inner_heights.ascent  + overhang_a;
    float new_cd = inner_heights.descent + overhang_d;
    
    if(iter.index_in_sequence(span_seq, span_end + 1) <= span_end) { // closing parenthesis
      isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
      context.math_shaper->vertical_stretch_char(
        context, new_ca, new_cd, false, iter.current_char(), &iter.current_glyph());
      
      iter.move_next_token();
    }
    
    isp.switch_to_sequence(context, call_paren_start.current_sequence(), DisplayStage::Layout);
    context.math_shaper->vertical_stretch_char(
      context, new_ca, new_cd, false, call_paren_start.current_char(), &call_paren_start.current_glyph());
      
    if(heights.ascent  < inner_heights.ascent)  heights.ascent  = inner_heights.ascent;
    if(heights.descent < inner_heights.descent) heights.descent = inner_heights.descent;
      
    if(core_heights.ascent  < new_ca) core_heights.ascent  = new_ca;
    if(core_heights.descent < new_cd) core_heights.descent = new_cd;
      
    while(iter.index_in_sequence(span_seq, span_end + 1) <= span_end)
      stretch_outermost_span(core_heights, heights);
  }
}

void MathSequence::Impl::VerticalStretcher::stretch_nonspan_box(Box *box, GlyphHeights &core_heights, GlyphHeights &heights) {
  ARRAY_ASSERT(box == iter.current_box());

  auto subsup = dynamic_cast<SubsuperscriptBox*>(box);
  
  if(subsup && iter.glyph_index() > 0) {
    GlyphIterator prev = iter;
    prev.move_by_glyphs(-1);
    
    isp.switch_to_sequence(context, prev.current_sequence(), DisplayStage::Layout);
    
    if(auto prev_box = prev.current_box()) {
      subsup->stretch(context, prev_box->extents());
    }
    else {
      BoxSize size;
      
      context.math_shaper->vertical_glyph_size(
        context, prev.current_char(), prev.current_glyph(),
        &size.ascent, &size.descent);
        
      subsup->stretch(context, size);
      subsup->adjust_x(context, prev.current_char(), prev.current_glyph());
    }
    
    subsup->extents().bigger_y(&heights.ascent,      &heights.descent);
    subsup->extents().bigger_y(&core_heights.ascent, &core_heights.descent);
  }
  else if(auto underover = dynamic_cast<UnderoverscriptBox *>(box)) {
    uint16_t ch = 0;
    
    if(underover->base()->length() == 1 && underover->base()->glyph_array().length() == 1)
      ch = underover->base()->text()[0];
    
    isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
    if(iter.is_operand_start() && (pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch))) { // bigop that starts no span
      GlyphInfo &gi = underover->base()->glyph_array()[0];
      
      context.math_shaper->vertical_stretch_char(
        context, 0, 0,
        true,
        ch, &gi);
        
      context.math_shaper->vertical_glyph_size(
        context, ch, gi,
        &underover->base()->_extents.ascent,
        &underover->base()->_extents.descent);
        
      underover->base()->_extents.width = gi.right;
      
      underover->after_items_resize(context);
      
      iter.current_glyph().right = underover->extents().width;
    }
    
    underover->base()->extents().bigger_y(&core_heights.ascent, &core_heights.descent);
  }
  else
    box->extents().bigger_y(&core_heights.ascent, &core_heights.descent);
  
  box->extents().bigger_y(&heights.ascent, &heights.descent);
  iter.move_next_token();
}

void MathSequence::Impl::VerticalStretcher::size_nonspan_token(MathSequence *span_seq, GlyphHeights &heights) {
  int next_token = iter.index_in_sequence(span_seq, 0) + iter.find_next_token() - iter.text_index();
  
  while(iter.index_in_sequence(span_seq, next_token) < next_token) {
    isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
    context.math_shaper->vertical_glyph_size(
      context, iter.current_char(), iter.current_glyph(), &heights.ascent, &heights.descent);
    iter.move_next_glyph();
  };
}

//} ... class MathSequence::Impl::VerticalStretcher

//{ class MathSequence::Impl::EnlargeSpace ...

void MathSequence::Impl::EnlargeSpace::run_text_space_characters() {
  for(GlyphIterator iter{self}; iter.has_more_glyphs(); iter.move_next_glyph()) {
    if(iter.current_char() == '\t') {
      isp.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Layout);
      iter.current_glyph().index = 0;
      iter.current_glyph().right = 4 * context.canvas().get_font_size();
    }
  }
}

void MathSequence::Impl::EnlargeSpace::run() {
  run_text_space_characters();
  
  if(context.script_level >= 2 || !context.math_spacing)
    return;
  
  bool in_alias = false;
  bool last_was_factor = false;
  //bool last_was_number = false;
  bool last_was_space  = false;
  bool last_was_left   = false;
  
  for(GlyphIterator iter_next{self}; iter_next.has_more_glyphs();) {
    GlyphIterator iter_start = iter_next;
    iter_next.move_token_end();
    
    GlyphIterator iter_tok_end = iter_next;
    
    italic_correction(iter_next);
    
    iter_next.move_next_glyph();
    while(iter_next.has_more_glyphs() && dynamic_cast<SubsuperscriptBox*>(iter_next.current_box())) {
      iter_next.move_next_glyph();
    }
    
    isp.switch_to_sequence(context, iter_start.current_sequence(), DisplayStage::Layout);
    
    if(iter_start.current_char() == '\t') {
      show_tab_character(iter_start, iter_start.current_glyph().is_normal_text);
      continue;
    }
    
    if(iter_start.current_char() == PMATH_CHAR_ALIASDELIMITER) {
      in_alias = !in_alias;
      last_was_factor = false;
      continue;
    }
    
    if(iter_start.current_glyph().is_normal_text) {
      last_was_factor = false;
      continue;
    }
    
    if(iter_start.current_glyph().is_normal_text || in_alias)
      continue;
      
    if( iter_start.current_char() == PMATH_CHAR_INVISIBLECALL || 
        iter_start.current_char() == PMATH_CHAR_INVISIBLETIMES || 
        iter_start.current_char() == PMATH_CHAR_INVISIBLECOMMA ||
        iter_start.current_char() == PMATH_CHAR_INVISIBLEPLUS)
    {
      continue;
    }
    
    ArrayView<const uint16_t> tok_text = get_effective_token(iter_start, iter_tok_end);
    
    int prec;
    pmath_token_t tok = pmath_token_analyse(tok_text.items(), tok_text.length(), &prec);
    float space_left  = 0.0;
    float space_right = 0.0;
    
    bool lwf = false; // new last_was_factor
    bool lwl = false; // new last_was_left
    switch(tok) {
      case PMATH_TOK_PLUSPLUS: {
          if(iter_start.is_operand_start()) {
            prec = PMATH_PREC_CALL;
            goto PREFIX;
          }
          
          if(iter_next.is_operand_start())
            goto INFIX;
          
          prec = PMATH_PREC_CALL;
        }
        goto POSTFIX;
        
      case PMATH_TOK_NARY_OR_PREFIX: {
          if(iter_start.is_operand_start()) {
            prec = pmath_token_prefix_precedence(tok_text.items(), tok_text.length(), prec);
            goto PREFIX;
          }
        }
        goto INFIX;
        
      case PMATH_TOK_CALL:
      case PMATH_TOK_CALLPIPE:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_NARY:
      case PMATH_TOK_QUESTION: {
        INFIX:
          switch(prec) {
            case PMATH_PREC_SEQ:
            case PMATH_PREC_EVAL:
              space_right = self.em * 6 / 18;
              break;
              
            case PMATH_PREC_ASS:
            case PMATH_PREC_MODY:
              space_left  = self.em * 4 / 18;
              space_right = self.em * 8 / 18;
              break;
              
            case PMATH_PREC_LAZY:
            case PMATH_PREC_REPL:
            case PMATH_PREC_RULE:
            case PMATH_PREC_MAP:
            case PMATH_PREC_STR:
            case PMATH_PREC_COND:
            case PMATH_PREC_ARROW:
            case PMATH_PREC_REL:
              space_left = space_right = self.em * 5 / 18; // total: 10/18 em
              break;
              
            case PMATH_PREC_ALT:
            case PMATH_PREC_OR:
            case PMATH_PREC_XOR:
            case PMATH_PREC_AND:
            case PMATH_PREC_UNION:
            case PMATH_PREC_ISECT:
            case PMATH_PREC_RANGE:
            case PMATH_PREC_ADD:
            case PMATH_PREC_PLUMI:
              space_left = space_right = self.em * 4 / 18; // total: 8/18 em
              break;
              
            case PMATH_PREC_CIRCADD:
            case PMATH_PREC_CIRCMUL:
            case PMATH_PREC_MUL:
            case PMATH_PREC_DIV:
            case PMATH_PREC_MIDDOT:
            case PMATH_PREC_MUL2:
              space_left = space_right = self.em * 3 / 18; // total: 6/18 em
              break;
              
            case PMATH_PREC_CROSS:
            case PMATH_PREC_POW:
            case PMATH_PREC_APL:
            case PMATH_PREC_TEST:
              space_left = space_right = self.em * 2 / 18; // total: 4/18 em
              break;
              
            case PMATH_PREC_REPEAT:
            case PMATH_PREC_INC:
            case PMATH_PREC_CALL:
            case PMATH_PREC_DIFF:
            case PMATH_PREC_PRIM:
              break;
          }
        }
        break;
        
      case PMATH_TOK_POSTFIX_OR_PREFIX:
        if(!iter_start.is_operand_start())
          goto POSTFIX;
          
        prec = pmath_token_prefix_precedence(tok_text.items(), tok_text.length(), prec);
        goto PREFIX;
        
      case PMATH_TOK_PREFIX: {
        PREFIX:
          switch(prec) {
            case PMATH_PREC_REL: // not
              space_right = self.em * 4 / 18;
              break;
              
            case PMATH_PREC_ADD:
              space_right = self.em * 1 / 18;
              break;
              
            case PMATH_PREC_DIV:
              if(tok_text[0] == PMATH_CHAR_INTEGRAL_D) {
                space_left = self.em * 3 / 18;
              }
              break;
              
            default: break;
          }
        }
        break;
        
      case PMATH_TOK_POSTFIX: {
        POSTFIX:
          switch(prec) {
            case PMATH_PREC_FAC:
            case PMATH_PREC_FUNC:
              space_left = self.em * 2 / 18;
              break;
              
            default: break;
          }
        }
        break;
        
      case PMATH_TOK_COLON:
      case PMATH_TOK_ASSIGNTAG:
        space_left = space_right = self.em * 4 / 18;
        break;
        
      case PMATH_TOK_NEWLINE:
        space_left = space_right = 0.0;
        break;
        
      case PMATH_TOK_SPACE: {
          // implicit multiplication:
          if( iter_start.current_char() == ' ' &&
              iter_next.has_more_glyphs() &&
              last_was_factor && context.multiplication_sign)
          {
            uint16_t next_char = iter_next.current_char();
            pmath_token_t tok2 = pmath_token_analyse(&next_char, 1, nullptr);
            
            if(Box *next_box = iter_next.current_box())
              tok2 = get_box_start_token(next_box);
            
            if(tok2 == PMATH_TOK_DIGIT || tok2 == PMATH_TOK_LEFTCALL) {
              context.math_shaper->decode_token(
                context,
                1,
                &context.multiplication_sign,
                &iter_start.current_glyph());
                
              space_left = space_right = self.em * 3 / 18;
              
              //if(context.show_auto_styles)
              iter_start.semantic_style_iter().reset_range(GlyphStyleImplicit, 1); // would invalidate later iters ...
            }
          }
          else {
            last_was_space = true;
            continue;
          }
        } break;
        
      case PMATH_TOK_DIGIT:
        group_number_digits(iter_start, iter_tok_end);
      /* fall through */
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
        lwf = true;
      /* fall through */
      case PMATH_TOK_SLOT:
        if(last_was_factor) {
          space_left = self.em * 3 / 18;
        }
        break;
        
      case PMATH_TOK_LEFT:
        lwl = true;
        if(last_was_factor) {
          space_left = self.em * 3 / 18;
        }
        break;
        
      case PMATH_TOK_RIGHT:
        if(last_was_left) {
          space_left = self.em * 3 / 18;
        }
        lwf = true;
        break;
        
      case PMATH_TOK_PRETEXT:
        if(iter_start.glyph_index() + 1 == iter_tok_end.glyph_index() && iter_start.current_char() == '<') {
          iter_tok_end.current_glyph().x_offset -= self.em * 4 / 18;
          iter_tok_end.current_glyph().right -=    self.em * 2 / 18;
        }
        break;
        
      case PMATH_TOK_LEFTCALL:
        lwl = true;
        break;
        
      case PMATH_TOK_NONE:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_INTEGRAL:
      case PMATH_TOK_COMMENTEND:
        break;
    }
    
    //last_was_number = tok == PMATH_TOK_DIGIT;
    last_was_factor = lwf;
    last_was_left   = lwl;
    
    if(last_was_space) {
      last_was_space = false;
      space_left     = 0;
    }
    
    if(iter_start.glyph_index() > 0 || iter_next.has_more_glyphs()) {
      if(iter_start.glyph_index() > 0) {
        self.glyphs[iter_start.glyph_index() - 1].right += space_left / 2;
        space_left-= space_left / 2;
      }
      iter_start.current_glyph().x_offset += space_left;
      iter_start.current_glyph().right +=    space_left;
      if(iter_next.has_more_glyphs()) {
        iter_next.current_glyph().x_offset += space_right / 2;
        iter_next.current_glyph().right +=    space_right / 2;
        space_right-= space_right / 2;
      }
      
      if(iter_next.glyph_index() > 0)
        iter_next.all_glyphs()[iter_next.glyph_index() - 1].right += space_right;
    }
  }
  
  isp.switch_to_sequence(context, &self, DisplayStage::Layout);
}

void MathSequence::Impl::EnlargeSpace::group_number_digits(const GlyphIterator &start, const GlyphIterator &end_inclusive) {
  static const int min_int_digits  = 5;
  static const int min_frac_digits = 7;//INT_MAX;
  static const int int_group_size  = 3;
  static const int frac_group_size = 5;
  
  float half_space_width = self.em / 10; // total: em/5 = thin space
  
  const uint16_t *buf = start.text_buffer_raw();
  
  int end = end_inclusive.text_index();
  
  RICHMATH_ASSERT(buf == end_inclusive.text_buffer_raw());
  RICHMATH_ASSERT(start.text_index() <= end);
  RICHMATH_ASSERT(end < start.text_buffer_length());
  
  if(end_inclusive.glyph_index() - start.glyph_index() != end - start.text_index()) {
    // unsupported: number without 1-to-1 mapping between chars and glyphs
    return;
  }
  
  int decimal_point = end + 1;
  
  for(int i = start.text_index(); i <= end; ++i) {
    if(buf[i] < '0' || buf[i] > '9') {
      if(buf[i] == '^') // explicitly specified base
        return;
        
      if(buf[i] == '.' && i < decimal_point) {
        decimal_point = i;
        continue;
      }
      
      if(buf[i] == '`') { // precision control
        if(i < decimal_point)
          decimal_point = i;
          
        end = i - 1;
        break;
      }
    }
  }
  
  if( int_group_size                     >  0              &&
      decimal_point - start.text_index() >= min_int_digits &&
      decimal_point - start.text_index() >  int_group_size)
  {
    for(int i = decimal_point - int_group_size; i > start.text_index(); i -= int_group_size) {
      int glyph_index = start.glyph_index() + i - start.text_index();
      
      self.glyphs[glyph_index - 1].right += half_space_width;
      
      self.glyphs[glyph_index].x_offset  += half_space_width;
      self.glyphs[glyph_index].right     += half_space_width;
    }
  }
  
  if( frac_group_size     >  0               &&
      end - decimal_point >= min_frac_digits &&
      end - decimal_point >  frac_group_size)
  {
    int significant_end = decimal_point;
    while(significant_end < end && buf[significant_end] != '[')
      ++significant_end;
  
    for(int i = decimal_point + frac_group_size; i < significant_end; i += frac_group_size) {
      int glyph_index = start.glyph_index() + i - start.text_index();
      
      self.glyphs[glyph_index].right        += half_space_width;
      
      self.glyphs[glyph_index + 1].x_offset += half_space_width;
      self.glyphs[glyph_index + 1].right    += half_space_width;
    }
  }
}

bool MathSequence::Impl::EnlargeSpace::slant_is_italic(int glyph_slant) {
  switch(glyph_slant) {
    case FontSlantPlain:
      return false;
    case FontSlantItalic:
      return true;
  }
  return context.math_shaper->get_style().italic;
}

void MathSequence::Impl::EnlargeSpace::italic_correction(const GlyphIterator &token_end) {
  if(token_end.current_char() == PMATH_CHAR_BOX)
    return;
  
  if(!slant_is_italic(token_end.current_glyph().slant)) {
    if(!pmath_char_is_integral(token_end.current_char()))
      return;
  }
  
  GlyphIterator next_glyph = token_end;
  next_glyph.move_next_glyph();
  
  if( !next_glyph.has_more_glyphs() ||
      !slant_is_italic(next_glyph.current_glyph().slant) ||
      next_glyph.current_char() == PMATH_CHAR_BOX ||
      pmath_char_is_integral(token_end.current_char()))
  {
    float ital_corr = context.math_shaper->italic_correction(
                        context,
                        token_end.current_char(),
                        token_end.current_glyph());
                        
    ital_corr *= self.em;
    if(next_glyph.has_more_glyphs()) {
      next_glyph.current_glyph().x_offset += ital_corr;
      next_glyph.current_glyph().right += ital_corr;
    }
    else
      token_end.current_glyph().right += ital_corr;
  }
}

void MathSequence::Impl::EnlargeSpace::show_tab_character(const GlyphIterator &pos, bool in_string) {
  static uint16_t arrow = 0x21e2;//0x27F6;
  float width = 4 * context.canvas().get_font_size();
  
  if(context.show_auto_styles && !in_string) {
    context.math_shaper->decode_token(
      context,
      1,
      &arrow,
      &pos.current_glyph());
      
    pos.current_glyph().x_offset = (width - pos.current_glyph().right) / 2;
    pos.semantic_style_iter().reset_range(GlyphStyleImplicit, 1);
  }
  else {
    pos.current_glyph().index = 0;
  }
  
  pos.current_glyph().right = width;
}

ArrayView<const uint16_t> MathSequence::Impl::EnlargeSpace::get_effective_token(const GlyphIterator &start, const GlyphIterator &tok_end) {
////Fixme: This assumption can be wrong:
//  RICHMATH_ASSERT(start.text_buffer_raw() == tok_end.text_buffer_raw());
//  RICHMATH_ASSERT(start.text_index() <= tok_end.text_index());
//  RICHMATH_ASSERT(tok_end.text_index() < start.text_buffer_length());
  
  Box *box = start.current_box();
  while(box) {
    if(AbstractStyleBox *asb = dynamic_cast<AbstractStyleBox *>(box)) {
      box = asb->content();
      continue;
    }
    
    if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
      if(seq->length() == 1 && seq->count() == 1) {
        box = seq->item(0);
        continue;
      }
      
      int e = 0;
      while(e < seq->length() && !seq->span_array().is_token_end(e)) {
        ++e;
      }
      
      if(e + 1 == seq->length())
        return ArrayView<const uint16_t>(seq->length(), seq->text().buffer());
      
      break;
    }
    
    if(UnderoverscriptBox *uob = dynamic_cast<UnderoverscriptBox *>(box)) {
      box = uob->base();
      continue;
    }
    
    break;
  }
  
  // See above FIXME
  //return ArrayView<const uint16_t>(tok_end.text_index() + 1 - start.text_index(), start.text_at_glyph());
  return start.find_token();
}          

pmath_token_t MathSequence::Impl::EnlargeSpace::get_box_start_token(Box *box) {
  while(box) {
    if(AbstractStyleBox *asb = dynamic_cast<AbstractStyleBox *>(box)) {
      box = asb->content();
      continue;
    }
    
    if(UnderoverscriptBox *uob = dynamic_cast<UnderoverscriptBox *>(box)) {
      box = uob->base();
      continue;
    }
    
    if(NumberBox *nb = dynamic_cast<NumberBox *>(box)) {
      box = nb->content();
      continue;
    }
    
    if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
      if(seq->length() == 0)
        break;
        
      const uint16_t *buf = seq->text().buffer();
      if(buf[0] == PMATH_CHAR_BOX) {
        box = seq->item(0);
        continue;
      }
      
      return pmath_token_analyse(buf, 1, nullptr);
    }
    
    if(dynamic_cast<FractionBox *>(box))
      return PMATH_TOK_DIGIT;
      
    break;
  }
  
  return PMATH_TOK_NAME2;
}

//} ... class MathSequence::Impl::EnlargeSpace

void MathSequence::Impl::paint(Context &context) {
  Point p0 = context.canvas().current_pos();
  
  Color default_color = context.canvas().get_color();
  SharedPtr<MathShaper> default_math_shaper = context.math_shaper;
  
  context.syntax->glyph_style_colors[GlyphStyleNone] = default_color;
  
  float y = p0.y;
  if(self.lines.length() > 0)
    y -= self.lines[0].ascent;
  
  double clip_x1, clip_y1, clip_x2, clip_y2;
  context.canvas().clip_extents(&clip_x1, &clip_y1, &clip_x2,  &clip_y2);
  
  int line = 0;
  // skip invisible lines:
  while(line < self.lines.length()) {
    float h = self.lines[line].ascent + self.lines[line].descent;
    if(y + h >= clip_y1)
      break;
      
    y += h + self.line_spacing();
    ++line;
  }
  
  if(line < self.lines.length()) {
    float glyph_left = 0;
    InlineSpanPainting inline_span_painting{&self};
    GlyphIterator iter{self};
    
    if(line > 0)
      iter.move_to_glyph(self.lines[line - 1].end);
      
    if(iter.glyph_index() > 0)
      glyph_left = self.glyphs[iter.glyph_index() - 1].right;
      
    bool have_style = false;
    bool have_slant = false;
    for(; line < self.lines.length() && y < clip_y2; ++line) {
      float x_extra = p0.x + self.indention_width(self.lines[line].indent);
      
      if(iter.glyph_index() > 0)
        x_extra -= self.glyphs[iter.glyph_index() - 1].right;
      
      y += self.lines[line].ascent;
      
      for(; iter.glyph_index() < self.lines[line].end; iter.move_next_glyph()) {
        inline_span_painting.switch_to_sequence(context, iter.current_sequence(), DisplayStage::Paint);
        
        if(iter.current_char() == '\n') {
          glyph_left = iter.current_glyph().right;
          continue;
        }
        
        if(have_style || iter.semantic_style()) {
          Color color = context.syntax->glyph_style_colors[iter.semantic_style().kind() & 0x0F];
          
          context.canvas().set_color(color);
          have_style = color != default_color;
        }
        
        if(have_slant || iter.current_glyph().slant) {
          if(iter.current_glyph().slant == FontSlantItalic) {
            context.math_shaper = default_math_shaper->set_style(
                                    default_math_shaper->get_style() + Italic);
            have_slant = true;
          }
          else if(iter.current_glyph().slant == FontSlantPlain) {
            context.math_shaper = default_math_shaper->set_style(
                                    default_math_shaper->get_style() - Italic);
            have_slant = true;
          }
          else {
            context.math_shaper = default_math_shaper;
            have_slant = false;
          }
        }
        
        if(auto box = iter.current_box()) {
          context.canvas().move_to(glyph_left + x_extra + iter.current_glyph().x_offset, y);
          
          box->paint(context);
          
          context.syntax->glyph_style_colors[GlyphStyleNone] = default_color;
        }
        else if(iter.current_glyph().index || iter.current_glyph().composed || iter.current_glyph().horizontal_stretch) {
          if(iter.current_glyph().is_normal_text) {
            context.text_shaper->show_glyph(
              context,
              Point{glyph_left + x_extra, y},
              iter.current_char(),
              iter.current_glyph());
          }
          else {
            context.math_shaper->show_glyph(
              context,
              Point{glyph_left + x_extra, y},
              iter.current_char(),
              iter.current_glyph());
          }
        }
        
        if(iter.semantic_style().is_missing_after()) {
          float d = self.em * RefErrorIndictorHeight * 2 / 3.0f;
          float dd = d / 4;
          
          context.canvas().move_to(iter.current_glyph().right + x_extra, y + self.em / 8);
          if(iter.glyph_index() + 1 < self.glyphs.length())
            context.canvas().rel_move_to(self.glyphs[iter.glyph_index() + 1].x_offset / 2, 0);
            
          context.canvas().rel_line_to(-d, d);
          context.canvas().rel_line_to(dd, dd);
          context.canvas().rel_line_to(d - dd, dd - d);
          context.canvas().rel_line_to(d - dd, d - dd);
          context.canvas().rel_line_to(dd, -dd);
          context.canvas().rel_line_to(-d, -d);
          
          context.canvas().close_path();
          context.canvas().set_color(
            context.syntax->glyph_style_colors[GlyphStyleExcessOrMissingArg]);
          context.canvas().fill();
          
          have_style = true;
        }
        
        glyph_left = iter.current_glyph().right;
      }
      
      if(self.lines[line].continuation) {
        GlyphInfo gi;
        memset(&gi, 0, sizeof(GlyphInfo));
        uint16_t cont = CHAR_LINE_CONTINUATION;
        context.math_shaper->decode_token(
          context,
          1,
          &cont,
          &gi);
          
        context.math_shaper->show_glyph(
          context,
          Point{glyph_left + x_extra, y},
          cont,
          gi);
      }
      
      y += self.lines[line].descent + self.line_spacing();
    }
    
    inline_span_painting.switch_to_sequence(context, &self, DisplayStage::Paint);
  }
  
  if(!context.canvas().show_only_text) {
    InlineSpanPainting inline_span_painting{&self};
    
    context.for_each_selection_inside(&self, [&](const VolatileSelection &sel) {
      if(MathSequence *seq = dynamic_cast<MathSequence*>(sel.box)) {
        if(&Impl(*seq).outermost_span() == &self) {
          inline_span_painting.switch_to_sequence(context, seq, DisplayStage::Paint);
          context.canvas().move_to(p0);
          
          selection_path(&context, context.canvas(), sel);
            
          context.draw_selection_path();
        }
      }
    });
    
    inline_span_painting.switch_to_sequence(context, &self, DisplayStage::Paint);
  }
  
  context.math_shaper = default_math_shaper;
}

Vector2F MathSequence::Impl::total_offest_to_index(int index) {
  GlyphIterator iter = glyph_iterator();
  iter.skip_forward_to_glyph_after_text_pos(&self, index);
  
  MathSequence     &outer  = outermost_span();
  Array<Line>      &lines  = iter.outermost_sequence()->lines;
  Array<GlyphInfo> &glyphs = iter.outermost_sequence()->glyphs;
  
  if(lines.length() == 0)
    return {0.0f, 0.0f};
    
  float x = 0;
  float y = 0;
  
  int l = 0;
  while(l + 1 < lines.length() && lines[l].end <= iter.glyph_index()) {
    y += lines[l].descent + outer.line_spacing() + lines[l + 1].ascent;
    ++l;
  }
  
  x += outer.indention_width(lines[l].indent);
  
  if(iter.has_more_glyphs()) {
    if(iter.current_box())
      x += iter.current_glyph().x_offset;
  }
  
  if(iter.glyph_index() > 0) {
    x += glyphs[iter.glyph_index() - 1].right;
    
    if(l > 0 && lines[l - 1].end > 0) {
      x -= glyphs[lines[l - 1].end - 1].right;
    }
  }
  
  return {x, y};
}

float MathSequence::Impl::glyph_offset_x(int glpyh_index, int line_index) {
  float dx = 0;
  if(glpyh_index > 0)
    dx += self.glyphs[glpyh_index - 1].right;
    
  if(line_index > 0)
    dx -= self.glyphs[self.lines[line_index - 1].end - 1].right;
    
  dx += self.indention_width(self.lines[line_index].indent);
  return dx;
}

void MathSequence::Impl::selection_path_wrt_outermost_origin(Context *opt_context, Canvas &canvas, int start, int end) {
  Impl(outermost_span()).selection_path(opt_context, canvas, {&self, start, end});
}

void MathSequence::Impl::selection_path(Context *opt_context, Canvas &canvas, VolatileSelection sel) {
  Array<RectangleF> rects;
  selection_rectangles(rects, SelectionDisplayFlags::Default, canvas.current_pos(), opt_context, sel);
  
  if(sel.length() == 0 && rects.length() == 1) {
    canvas.move_to(canvas.align_point(rects[0].top_left(),    true));
    canvas.line_to(canvas.align_point(rects[0].bottom_left(), true));
    return;
  }
  
  for(auto &rect : rects) {
    rect.pixel_align(canvas, false, 0);
  }
  
  canvas.add_stacked_rectangles(rects);
}

void MathSequence::Impl::selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, Context *opt_context, VolatileSelection sel) {
  MathSequence *sel_seq = dynamic_cast<MathSequence*>(sel.box);
  if(!sel_seq)
    return;
  
  if(&Impl(*sel_seq).outermost_span() != &self)
    return;
  
  if(self.lines.length() == 0) // resize() not yet called
    return;
  
  GlyphIterator iter_start(self);
  iter_start.skip_forward_to_glyph_after_text_pos(sel_seq, sel.start);
  
  GlyphIterator iter_end = iter_start;
  iter_end.skip_forward_to_glyph_after_text_pos(sel_seq, sel.end);
  
  selection_rectangles(rects, flags, p0, opt_context, iter_start, iter_end);
}

void MathSequence::Impl::selection_rectangles(Array<RectangleF> &rects, SelectionDisplayFlags flags, Point p0, Context *opt_context, const GlyphIterator &start, const GlyphIterator &end) {
  bool tight_widths    = has(flags, SelectionDisplayFlags::TightWidths);
  bool big_middle_blob = has(flags, SelectionDisplayFlags::BigCenterBlob);
  
  RICHMATH_ASSERT(start.outermost_sequence() == &self);
  RICHMATH_ASSERT(end.outermost_sequence()   == &self);
  
  p0.y -= self.lines[0].ascent;
  
  Point p1 = p0;
  float line_spacing = self.line_spacing();
  
  int startline = 0;
  while(startline < self.lines.length() && start.glyph_index() >= self.lines[startline].end) {
    p1.y += self.lines[startline].ascent + self.lines[startline].descent + line_spacing;
    ++startline;
  }
  
  if(startline == self.lines.length()) {
    --startline;
    p1.y -= self.lines[startline].ascent + self.lines[startline].descent + line_spacing;
  }
  
  p1.x = p0.x + glyph_offset_x(start.glyph_index(), startline);
  Point p2 = p1;
  if(end.glyph_index() <= self.lines[startline].end) { // single line: return on tight rectangle
    p2.x = p0.x + glyph_offset_x(end.glyph_index(), startline);
    
    float a = 0.5 * self.em;
    float d = 0;
    
    if(opt_context) {
      InlineSpanPainting isp{&self};
      if(start.current_location() == end.current_location()) {
        if(start.glyph_index() > 0) {
          GlyphIterator iter_before_start(self);
          iter_before_start.move_to_glyph(start.glyph_index() - 1);
          box_size(isp, *opt_context, iter_before_start, &a, &d);
        }
        box_size(isp, *opt_context, start,        &a, &d);
      }
      else {
        boxes_size(isp, *opt_context, start, end, &a, &d);
      }
      
      isp.switch_to_sequence(*opt_context, &self, DisplayStage::Layout);
    }
    else {
      a = self.lines[startline].ascent;
      d = self.lines[startline].descent;
    }
    
    p1.y += self.lines[startline].ascent;
    p2.y = p1.y + d + 1;
    p1.y -= a + 1;
    
    rects.add({p1, p2});
    return;
  }
  
  p1.y -= line_spacing/2;
  p2.y -= line_spacing/2;
  p2.y += self.lines[startline].ascent + self.lines[startline].descent + line_spacing;
  
  p2.x = p0.x + (tight_widths ? glyph_offset_x(self.lines[startline].end, startline) : self._extents.width);
  rects.add({p1, p2});
  
  p1.y = p2.y;
  if(big_middle_blob || !tight_widths) {
    p1.x = p0.x;
    p2.x = p0.x + self._extents.width;
  }
  
  int endline = startline + 1;
  if(big_middle_blob) {
    while(endline < self.lines.length() && end.glyph_index() > self.lines[endline].end) {
      p2.y += self.lines[endline].ascent + self.lines[endline].descent + line_spacing;
      ++endline;
    }
    
    if(startline + 1 < endline) {
      rects.add({p1, p2});
    }
    p1.y = p2.y;
  }
  else {
    while(endline < self.lines.length() && end.glyph_index() > self.lines[endline].end) {
      p2.y += self.lines[endline].ascent + self.lines[endline].descent + line_spacing;
      
      if(tight_widths) {
        p1.x = p0.x + self.indention_width(self.lines[endline].indent);
        p2.x = p0.x + glyph_offset_x(self.lines[endline].end, endline);
      }
      rects.add({p1, p2});
      
      p1.y = p2.y;
      ++endline;
    }
  }
  
  if(endline < self.lines.length()) {
    p2.y += line_spacing/2;
    p2.y += self.lines[endline].ascent + self.lines[endline].descent;
    
    if(tight_widths) {
      p1.x = p0.x + self.indention_width(self.lines[endline].indent);
    }
    p2.x = p0.x + glyph_offset_x(end.glyph_index(), endline);
    
    rects.add({p1, p2});
  }
}
      
//{ class MathSequence::Impl::PaintHookHandler ...

MathSequence::Impl::PaintHookHandler::PaintHookHandler(Context &context, Point outermost_origin, PaintHookManager &hooks, bool background)
: context{context},
  outermost_origin{outermost_origin},
  hooks{hooks},
  background{background}
{
}

void MathSequence::Impl::PaintHookHandler::run(MathSequence &seq) {
  if(hooks.contains(&seq)) {
    context.canvas().move_to(outermost_origin + Impl(seq).total_offest_to_index(0));
    hooks.run(&seq, context);
  }
  
  for(auto box : seq.boxes) {
    if(auto inner = box->as_inline_span()) {
      if(background) {
        if(Color bg = box->get_own_style(Background)) {
          context.canvas().save();
  
          Impl(*inner).selection_path_wrt_outermost_origin(nullptr, context.canvas(), 0, inner->length());
          
          Color old_c = context.canvas().get_color();
          context.canvas().set_color(bg);
          
          context.canvas().fill();
          
          context.canvas().set_color(old_c);
          context.canvas().restore();
        }
      }
      run(*inner);
    }
  }
}

//} ... class MathSequence::Impl::PaintHookHandler

//{ class MathSequence::Impl::IndentLines ...
  
Array<int> MathSequence::Impl::IndentLines::indention_array(0);

MathSequence::Impl::IndentLines::IndentLines(MathSequence &span_seq) 
  : iter{span_seq}
{
}

void MathSequence::Impl::IndentLines::visit_all() {
  auto seq = iter.outermost_sequence();
  while(iter.has_more_glyphs()) {
    visit_span(seq, seq->span_array()[iter.index_in_sequence(seq, -1)], 0);
  }
}

void MathSequence::Impl::IndentLines::visit_span(MathSequence *span_seq, Span span, int depth) {
  if(!span) {
    visit_token(span_seq, depth);
    return;
  }
  
  if(iter.current_char() == '"' && iter.current_sequence() == span_seq && !span.next()) {
    visit_string(span_seq, span, depth);
    return;
  }
  
  void (IndentLines::*visitor)(MathSequence*, Span, int) = &IndentLines::visit_span;
  {
    int pos = iter.index_in_sequence(span_seq, -1);
    if(pos >= 0) {
      SpanExpr span_expr(pos, span, span_seq);
      if(BlockSpan::maybe_block(&span_expr))
        visitor = &IndentLines::visit_block_body;
    }
  }
  
  int start = iter.glyph_index();
  int inner_depth = depth + 1;
  if(iter.current_char() == '\n' && !span.next())
    inner_depth = depth;
  
  (this->*visitor)(span_seq, span.next(), inner_depth);
  
  int span_end = span.end();
  bool prev_simple = false;
  bool ends_with_newline = false;
  bool inner_newline = false;
  while(iter.has_more_glyphs()) {
    int pos = iter.index_in_sequence(span_seq, span_end + 1);
    if(pos > span_end)
      break;
    
    Span sub = span_seq->span_array()[pos];
    uint16_t ch = iter.current_char();
    if((ch == ';' || ch == '\n') && !sub && !prev_simple)
      inner_newline = true;
    
    auto tok_buf = iter.find_token();
    if(tok_buf.length() == 2 && tok_buf[0] == '|' && tok_buf[1] == '>') {
      for(int i = start; i < iter.glyph_index(); ++i)
        indention_array[i]--;
    }
      
    ends_with_newline = ch == '\n' && !sub;
    prev_simple = !sub;
    (this->*visitor)(span_seq, sub, inner_depth);
  }
  
  if(ends_with_newline) {
    // Have span = {{foo...}, "\n"}. Treat it like {foo...}
    for(int i = start; i < iter.glyph_index(); ++i)
      indention_array[i] = MAX(0, indention_array[i] - 1);
    return;
  }
  
  indention_array[start] = depth;
  if(inner_newline) {
    for(int i = start + 1; i < iter.glyph_index(); ++i)
      indention_array[i] = MAX(0, indention_array[i] - 1);
  }
}

void MathSequence::Impl::IndentLines::visit_token(MathSequence *span_seq, int depth) {
  if(iter.current_sequence() != span_seq) {
    auto inner = iter.current_sequence();
    while(inner) {
      auto outer = inner->find_parent<MathSequence>(false);
      if(outer == span_seq)
        break;
      
      inner = outer;
    }
    
    int inner_length = inner->length();
    
    ARRAY_ASSERT(inner);
    ARRAY_ASSERT(inner != span_seq);
    ARRAY_ASSERT(inner_length > 0);
    
    //++depth;
    while(iter.has_more_glyphs()) {
      int i = iter.index_in_sequence(inner, inner_length);
      if(i >= inner_length)
        break;
      
      visit_span(inner, inner->span_array()[i], depth);
    }
  }
  else {
    int next_token = iter.index_in_sequence(span_seq, 0) + iter.find_next_token() - iter.text_index();
    
    indention_array[iter.glyph_index()] = depth;
    iter.move_next_glyph();
    
    ++depth;
    while(iter.index_in_sequence(span_seq, next_token) < next_token) {
      indention_array[iter.glyph_index()] = depth;
      iter.move_next_glyph();
    }
  }
}

void MathSequence::Impl::IndentLines::visit_string(MathSequence *span_seq, Span span, int depth) {
  ARRAY_ASSERT(iter.current_char() == '"');
  ARRAY_ASSERT(span);
  ARRAY_ASSERT(!span.next());
  ARRAY_ASSERT(iter.current_sequence() == span_seq);
  
  int start = iter.glyph_index();
  iter.skip_forward_to_glyph_after_text_pos(span_seq, iter.text_index() + 1);
  
  indention_array[start] = depth;
  for(int i = start + 1; i < iter.glyph_index(); ++i)
    indention_array[i] = depth + 1;
  
  int span_end = span.end();
  while(iter.has_more_glyphs()) {
    int pos = iter.index_in_sequence(span_seq, span_end + 1);
    if(pos > span_end)
      break;
    
    if(iter.text_index() > 0 && iter.text_buffer_raw()[iter.text_index() - 1] == '\n') {
      indention_array[iter.glyph_index()] = depth;
      iter.move_next_glyph();
    }
    else {
      visit_span(span_seq, span_seq->span_array()[pos], depth + 1);
    }
  }
}

void MathSequence::Impl::IndentLines::visit_block_body(MathSequence *span_seq, Span span, int depth) {
  uint16_t start_char = iter.current_char();
  
  visit_span(span_seq, span, depth);
  if(!span)
    return;
  
  uint16_t last_char = 0;
  if(iter.text_index() > 0)
    last_char = iter.text_buffer_raw()[iter.text_index() - 1];
  
  if(start_char == '{' && last_char == '}' && !span.next()) {
    /* Unindent closing brace of a block.
         Block {
           body
         }
       instead of
         Block {
           body
           }
     */
    indention_array[iter.glyph_index() - 1] = MAX(0, depth - 1);
  }
  else if(last_char == ')') {
    /* Unindent closing parenthesis of a block header.
         If(
           cond
         ) {
           body
         }
       instead of
         If(
           cond
           ) {
           body
         }
     */
    indention_array[iter.glyph_index() - 1] = MAX(0, depth - 1);
  }
}

//} ... MathSequence::Impl::IndentLines

//{ class MathSequence::Impl::PenalizeBreaks ...

Array<double> MathSequence::Impl::PenalizeBreaks::penalty_array(0);

const double MathSequence::Impl::PenalizeBreaks::DepthPenalty = 1.0;
const double MathSequence::Impl::PenalizeBreaks::WordPenalty = 100.0;//2.0;
const double MathSequence::Impl::PenalizeBreaks::BestLineWidth = 0.95;
const double MathSequence::Impl::PenalizeBreaks::LineWidthFactor = 2.0;

MathSequence::Impl::PenalizeBreaks::PenalizeBreaks(MathSequence &span_seq) 
  : iter{span_seq}
{
}

void MathSequence::Impl::PenalizeBreaks::visit_all() {
  auto seq = iter.outermost_sequence();
  while(iter.has_more_glyphs()) {
    int i = iter.index_in_sequence(seq, -1);
    if(i < 0)
      break;
    
    visit_span(seq, seq->span_array()[i], 0);
  }
}

void MathSequence::Impl::PenalizeBreaks::visit_span(MathSequence *span_seq, Span span, int depth) {
  if(!span) {
    visit_token(span_seq, depth);
    return;
  }
  
  if(iter.current_char() == '"' && iter.current_sequence() == span_seq && !span.next()) {
    visit_string(span_seq, span, depth);
    return;
  }
  
  int func_depth = depth;
  ++depth;
  
  void (PenalizeBreaks::*visitor)(MathSequence*, Span, int) = &PenalizeBreaks::visit_span;
  {
    int pos = iter.index_in_sequence(span_seq, -1);
    if(pos >= 0) {
      SpanExpr span_expr(pos, span, span_seq);
      if(BlockSpan::maybe_block(&span_expr))
        visitor = &PenalizeBreaks::visit_block_body;
    }
  }
  
  int start = iter.glyph_index();
  if(iter.is_left_bracket()) {
    penalty_array[start] += WordPenalty + DepthPenalty;
  }
  
  (this->*visitor)(span_seq, span.next(), depth);
  
  int span_end = span.end();
  float inc_penalty = 0.0;
  float dec_penalty = 0.0;
  while(iter.has_more_glyphs()) {
    int index_in_seq = iter.index_in_sequence(span_seq, span_end + 1);
    if(index_in_seq > span_end)
      break;
    
    auto token = iter.find_syntax_form();
    switch(iter.current_char()) { // TODO: consider only the syntax_form (=token) ?
      case ';': dec_penalty = DepthPenalty; break;
      
      case PMATH_CHAR_ASSIGN:
      case PMATH_CHAR_ASSIGNDELAYED:
      case PMATH_CHAR_RULE:
      case PMATH_CHAR_RULEDELAYED:   inc_penalty = DepthPenalty; break;
      
      case ':': {
          auto tok_buf = iter.find_token();
          if( (tok_buf.length() == 3 && tok_buf[1] == ':' && tok_buf[2] == '=') ||
              (tok_buf.length() == 2 && (tok_buf[1] == '>' || tok_buf[1] == '=')))
          {
            inc_penalty = DepthPenalty;
          }
        } break;
        
      case '-': {
          auto tok_buf = iter.find_token();
          if(tok_buf.length() == 2 && (tok_buf[1] == '>' || tok_buf[1] == '='))
            inc_penalty = DepthPenalty;
        } break;
        
      case '+': {
          auto tok_buf = iter.find_token();
          if(tok_buf.length() == 2 && tok_buf[1] == '=')
            inc_penalty = DepthPenalty;
        } break;
        
      default:
        if(Tokenizer::is_left_bracket(token)) {
          if(iter.is_operand_start())
            penalty_array[iter.glyph_index()] += WordPenalty;
          else
            penalty_array[iter.glyph_index() - 1] += WordPenalty;
            
          depth = func_depth;
        }
        else if(Tokenizer::is_right_bracket(token)) {
          penalty_array[iter.glyph_index() - 1] += WordPenalty;
        }
    }
    
    (this->*visitor)(span_seq, span_seq->span_array()[index_in_seq], depth);
  }
  
  inc_penalty -= dec_penalty;
  if(inc_penalty != 0) {
    for(int i = start; i < iter.glyph_index(); ++i)
      penalty_array[i] += inc_penalty;
  }
}

void MathSequence::Impl::PenalizeBreaks::visit_string(MathSequence *span_seq, Span span, int depth) {
  ARRAY_ASSERT(iter.current_char() == '"');
  ARRAY_ASSERT(span);
  ARRAY_ASSERT(!span.next());
  ARRAY_ASSERT(iter.current_sequence() == span_seq);
  
  // Indenting by 2 seems to be the outcome of the old code:
  depth += 2;
  
  penalty_array[iter.glyph_index()] = Infinity;
  
  bool last_was_special = false;
  int span_end = span.end();
  
  while(iter.has_more_glyphs()) {
    int pos = iter.index_in_sequence(span_seq, span_end + 1);
    if(pos >= span_end)
      break;
    
    uint16_t ch = iter.current_char();
    pmath_token_t tok = pmath_token_analyse(&ch, 1, nullptr);
    
    penalty_array[iter.glyph_index()] += depth * DepthPenalty + WordPenalty;
    
    switch(tok) {
      case PMATH_TOK_SPACE:
        penalty_array[iter.glyph_index()] -= WordPenalty;
        last_was_special = false;
        break;
        
      case PMATH_TOK_STRING:
        last_was_special = false;
        break;
        
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
      case PMATH_TOK_DIGIT:
        if(last_was_special)
          penalty_array[iter.glyph_index() - 1] -= WordPenalty;
        last_was_special = false;
        break;
        
      default:
        last_was_special = true;
        break;
    }
    
    iter.move_next_glyph();
  }
}
  
void MathSequence::Impl::PenalizeBreaks::visit_token(MathSequence *span_seq, int depth) {
  if(iter.current_sequence() != span_seq) {
    auto inner = iter.current_sequence();
    while(inner) {
      auto outer = inner->find_parent<MathSequence>(false);
      if(outer == span_seq)
        break;
      
      inner = outer;
    }
    
    int inner_length = inner->length();
    
    ARRAY_ASSERT(inner);
    ARRAY_ASSERT(inner != span_seq);
    ARRAY_ASSERT(inner_length > 0);
    
    ++depth;
    while(iter.has_more_glyphs()) {
      int i = iter.index_in_sequence(inner, inner_length);
      if(i >= inner_length)
        break;
      visit_span(inner, inner->span_array()[i], depth);
    }
    return;
  }
  
  if(iter.glyph_index() > 0) {
    uint16_t ch = iter.current_char();
    if( ch == ',' ||
        ch == ';' ||
        ch == ':' ||
        ch == PMATH_CHAR_ASSIGN ||
        ch == PMATH_CHAR_ASSIGNDELAYED)
    {
      penalty_array[iter.glyph_index() - 1] += DepthPenalty;
      //--depth;
    }
    
    if( ch == 0xA0   /* \[NonBreakingSpace] */ ||
        ch == 0x2011 /* non breaking hyphen */ ||
        ch == 0x2060 /* \[NonBreak] */)
    {
      penalty_array[iter.glyph_index() - 1] = Infinity;
      penalty_array[iter.glyph_index()]     = Infinity;
      iter.move_next_glyph();
      return;
    }
    
    if(auto box = iter.current_box()) {
      if(dynamic_cast<SubsuperscriptBox *>(box)) {
        penalty_array[iter.glyph_index() - 1] = Infinity;
        iter.move_next_glyph();
        return;
      }
      
      if(dynamic_cast<GridBox *>(box)) {
        // Do not break between \[Piecewise] and GridBox or around GridBox when .
        // FIXME: Check only works when \[Piecewise] and GridBox are in the same MathSequence.
        // FIXME: Check should be done when visiting \[Piecewise], not here, when visiting the GridBox.
        if(iter.text_index() > 0 && iter.text_span_array().is_operand_start(iter.text_index() - 1)) {
          uint16_t prev_char = iter.text_buffer_raw()[iter.text_index() - 1];
          if(pmath_char_is_left(prev_char)) {
            penalty_array[iter.glyph_index() - 1] = Infinity;
          }
        }
        
        iter.move_next_glyph();
        if(iter.has_more_glyphs() && pmath_char_is_right(iter.current_char())) {
          penalty_array[iter.glyph_index() - 1] = Infinity;
        }
        
        return;
      }
    }
  }
  
  if(!iter.is_operand_start())
    ++depth;
    
  if(iter.current_char() == ' ') {
    penalty_array[iter.glyph_index()] += depth * DepthPenalty;
    iter.move_next_glyph();
    return;
  }
  
  int token_end = iter.index_in_sequence(span_seq, 0) + iter.find_token_end() - iter.text_index();
  
  while(iter.index_in_sequence(span_seq, token_end) < token_end) {
    penalty_array[iter.glyph_index()] += depth * DepthPenalty + WordPenalty;
    iter.move_next_glyph();
  }
  
  if(iter.has_more_glyphs()) {
    penalty_array[iter.glyph_index()] += depth * DepthPenalty;
    iter.move_next_glyph();
  }
}

void MathSequence::Impl::PenalizeBreaks::visit_block_body(MathSequence *span_seq, Span span, int depth) {
  if(!span) {
    visit_token(span_seq, depth);
    return;
  }
  
  visit_span(span_seq, span, depth);
  
  if(iter.text_index() >= 2 && iter.glyph_index() >= 2) {
    uint16_t prev_char = iter.text_buffer_raw()[iter.text_index() - 1];
    if(prev_char == ')') {
      penalty_array[iter.glyph_index() - 2] = MAX(0, penalty_array[iter.glyph_index() - 1] - 2 * DepthPenalty);
    }
  }
}

//} ... class MathSequence::Impl::PenalizeBreaks

//{ class InlineSpanPainting ...

BasicHeterogeneousStack InlineSpanPainting::context_stack;
  
InlineSpanPainting::InlineSpanPainting(MathSequence *cur)
  : current{cur},
    debug_stack_depth(context_stack.contents.length())
{
}

InlineSpanPainting::~InlineSpanPainting() {
  ARRAY_ASSERT(context_stack.contents.length() == debug_stack_depth);
}

void InlineSpanPainting::switch_to_sequence_impl(Context &context, MathSequence *next_seq, DisplayStage stage) {
  Box *common_parent = Box::common_parent(current, next_seq);
  
  for(Box *b = current; b != common_parent; b = b->parent()) {
    if(auto seq = b->as_inline_span()) {
      b->end_paint_inline_span(context, context_stack, stage);
    }
  }
  
  int num_anchestors = 0;
  for(Box *b = next_seq; b != common_parent; b = b->parent())
    ++num_anchestors;
  
  static Array<Box*> ancestors;
  
  ancestors.length(num_anchestors);
  int i = num_anchestors;
  for(Box *b = next_seq; b != common_parent; b = b->parent()) {
    ancestors.set(--i, b);
  }
  
  for(Box *b : ancestors) {
    if(auto seq = b->as_inline_span()) {
      b->begin_paint_inline_span(context, context_stack, stage);
    }
  }
  
  current = next_seq;
}

//} ... class InlineSpanPainting
