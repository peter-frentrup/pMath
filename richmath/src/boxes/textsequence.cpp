#include <boxes/textsequence.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <cstdlib>
#include <algorithm>

#include <boxes/box-factory.h>
#include <boxes/templatebox.h>

#include <graphics/context.h>
#include <graphics/rectangle.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
}}

extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_TextData;

static const uint16_t ObjectReplacementChar = 0xFFFC;
static const char *Utf8ObjectReplacementChar = "\xEF\xBF\xBC";
static const int   Utf8ObjectReplacementCharLen = 3;

namespace {
  class PangoContextUtil {
    public:
      static PangoContext *create_context() {
        PangoCairoFontMap *fontmap = (PangoCairoFontMap *)pango_cairo_font_map_get_default();
          
        return pango_cairo_font_map_create_context(fontmap);
      }
      
      static PangoLayout *create_layout() {
        PangoContext *context = create_context();
        PangoLayout *layout = pango_layout_new(context);
        g_object_unref(context);
        return layout;
      }
      
      static void append_fonts(String &name, TextShaper *ts) {
        if(!ts)
          return;
        
        int num_fonts = ts->num_fonts();
        for(int i = 0; i < num_fonts; ++i) {
          String fn = ts->font_name(i);
          
          if(fn.length() > 0) {
            if(name.length() > 0)
              name += ",";
            name += fn;
          }
        }
      }
      
      static void update(PangoContext *pango, Context &ctx) {
        pango_cairo_update_context(ctx.canvas().cairo(), pango);
        
        pango_cairo_context_set_shape_renderer(
          pango,
          box_shape_renderer,
          &ctx,
          0);
          
        PangoFontDescription *desc = pango_font_description_new();
        FontStyle style = ctx.text_shaper->get_style();
        
        String name;
        if(auto fts = dynamic_cast<FallbackTextShaper*>(ctx.text_shaper.ptr())) {
          auto all = fts->all_shapers();
          
#ifdef RICHMATH_USE_FT_FONT
          // FontConfig will do font fallback. Do not add our own (ctx.math_shaper at end).
          if(all.length() > 0 && all[all.length() - 1].ptr() == ctx.math_shaper.ptr()) {
            for(int i = 0; i < all.length() - 1; ++i) {
              append_fonts(name, all[i].ptr());
            }
          }
#endif
        }
        
        if(name.length() == 0)
          append_fonts(name, ctx.text_shaper.ptr());
        
        char *utf8_name = pmath_string_to_utf8(name.get_as_string(), nullptr);
        if(utf8_name)
          pango_font_description_set_family_static(desc, utf8_name);
        
        pango_font_description_set_absolute_size(desc, ctx.canvas().get_font_size() * PANGO_SCALE);
        pango_font_description_set_style(        desc, style.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
        pango_font_description_set_weight(       desc, style.bold   ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
        
        pango_context_set_font_description(pango, desc);
        
        pango_font_description_free(desc);
        pmath_mem_free(utf8_name);
      };
      
      static void box_shape_renderer(cairo_t *cr, PangoAttrShape *shape, gboolean do_path, void *data) {
        Context &ctx = *(Context *)data;
        Box     *box = (Box *)shape->data;
        
        RICHMATH_ASSERT(box != nullptr);
        RICHMATH_ASSERT(ctx.canvas().cairo() == cr);
        
        ctx.canvas().rel_move_to(pango_units_to_double(shape->ink_rect.x), 0);
        box->paint(ctx);
      }
  };

  class PangoLayoutIterPtr {
    public:
      explicit PangoLayoutIterPtr(PangoLayoutIter *ptr) : _ptr{ptr} {}
      PangoLayoutIterPtr(const PangoLayoutIterPtr &that) : _ptr{that._ptr ? pango_layout_iter_copy(that._ptr) : nullptr} {}
      PangoLayoutIterPtr(PangoLayoutIterPtr &&that) : _ptr{that._ptr} { that._ptr = nullptr; }
      ~PangoLayoutIterPtr() { if(_ptr) pango_layout_iter_free(_ptr); }
      PangoLayoutIterPtr &operator=(PangoLayoutIterPtr that) { swap(*this, that); return *this; }
      
      friend void swap(PangoLayoutIterPtr &left, PangoLayoutIterPtr &right) {
        using std::swap;
        swap(left._ptr, right._ptr);
      }
      
      PangoLayoutIter *get() { return _ptr; }
      const PangoLayoutIter *get() const { return _ptr; }
  
    private:
      PangoLayoutIter *_ptr;
  };
}

namespace richmath {
  class TextSequence::Impl {
      TextSequence &self;
    public:
      Impl(TextSequence &self) : self(self) {}
      
      Expr to_pmath(BoxOutputFlags flags, int start, int end);
      
      void append_object(int &next_box, Expr object, BoxInputFlags options);
      
    private:
      Expr add_debug_metadata(Expr expr, BoxOutputFlags flags, int start, int end);
      
    public:
      PangoLayoutIter *new_pango_iter();
      
      void resize_boxes(Context &context);
      void update_layout();
      
      TextSequence       &outermost_span();
      TextLayoutIterator  text_iterator() { return TextLayoutIterator(outermost_span()); }
      
    private:
      TextSequence &parent_outermost_span();
      
    private:
      class Utf8Writer {
        public:
          using b2t_t        = RleLinearPredictorArray<int>;
          using b2seq_t      = RleArray<TextSequence*>;
          using b2t_iter_t   = b2t_t::iterator_type;
          using b2seq_iter_t = b2seq_t::iterator_type;
        public:
          explicit Utf8Writer(TextSequence &owner, Array<char> &buffer);
          
          void append_all();
        
        private:
          void append_all(TextSequence &seq);
          void append_box_glyphs(TextSequence &seq, int pos, Box *box);
          int append_single_char_bytes(TextSequence &seq, int pos, int count);
        
        private:
          TextSequence  &owner;
          Array<char>   &buffer;
          b2t_iter_t     b2t_iter;
          b2seq_iter_t   b2seq_iter;
      };
      
      class AttributeWriter {
        public:
          explicit AttributeWriter(TextSequence &owner, PangoAttrList *attr_list);
          
          void apply_all();
        
        private:
          void visit_inline_sequence(TextSequence *seq);
          void visit_box(TextSequence *seq, Box *box);
          
        private:
          TextLayoutIterator  iter;
          PangoAttrList      *attr_list;
      };
    
    public:
      Vector2F total_offest_to_index(int index);
      
      void line_extents(PangoLayoutIter *iter, int line, float *x, float *y, BoxSize *size);
      void line_extents(int line, float *x, float *y, BoxSize *size);
  };
}

//{ class TextSequence ...

TextSequence::TextSequence()
  : base(),
    _layout(PangoContextUtil::create_layout())
{
  pango_layout_set_spacing(_layout, pango_units_from_double(1.5));
  pango_layout_set_wrap(_layout, PANGO_WRAP_WORD_CHAR);
}

TextSequence::~TextSequence() {
  g_object_unref(_layout);
}

void TextSequence::resize(Context &context) {
  inline_span(false);
  em = context.canvas().get_font_size();
  
  PangoTabArray *tabs = pango_tab_array_new(1, FALSE);
  pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, pango_units_from_double(72 / 2.54)); // 1cm
  pango_layout_set_tabs(_layout, tabs);
  pango_tab_array_free(tabs);
  
  Impl(*this).resize_boxes(context);
  Impl(*this).update_layout();
  
  PangoContext *pango = pango_layout_get_context(_layout);
  PangoContextUtil::update(pango, context);
  pango_layout_context_changed(_layout);
  
  if(context.width < Infinity) {
    int w = pango_units_from_double(context.width);
    if(w < 0)
      w = 0;
    pango_layout_set_width(_layout, w);
  }
  else
    pango_layout_set_width(_layout, -1);
    
  /* We cannot use pango_layout_get_extents() for the height because Pango uses
     the logical rect instead of the ink rect for line height calculation.
     That is too big for math fonts like Cambria Math or Asana Math, which we
     also want to support as fallback fonts, so we must adjust every single line
     on our own.
   */
  line_y_corrections.length(pango_layout_get_line_count(_layout));
  int corr = 0;
  int spacing = pango_layout_get_spacing(_layout);
  int min_ascent  = pango_units_from_double(em * 0.75);
  int min_descent = pango_units_from_double(em * 0.25);
  
  PangoLayoutIter *iter = pango_layout_get_iter(_layout);
  int i = 0;
  do {
    PangoRectangle ink, logical;
    pango_layout_iter_get_line_extents(iter, &ink, &logical);
    
    int base = pango_layout_iter_get_baseline(iter);
    
    if(base - ink.y < min_ascent) {
      ink.height += min_ascent - (base - ink.y);
      ink.y = base - min_ascent;
    }
    
    if((ink.y + ink.height) - base < min_descent) {
      ink.height += min_descent - ((ink.y + ink.height) - base);
    }
    
    line_y_corrections[i] = corr + ink.y - logical.y;
    corr += logical.height - ink.height - spacing;
    
    ++i;
  } while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  PangoRectangle rect;
  pango_layout_get_extents(_layout, nullptr, &rect);
  
  // FIXME: use rect.x and rect.y
  _extents.width   = pango_units_to_double(rect.width);
  _extents.ascent  = pango_units_to_double(pango_layout_get_baseline(_layout) - line_y_corrections[0]);
  _extents.descent = pango_units_to_double(rect.height - corr - spacing) - _extents.ascent;
  
  context.sequence_unfilled_width = _extents.width;
}

void TextSequence::paint(Context &context) {
  float x0, y0;
  context.canvas().current_pos(&x0, &y0);
  
  AutoCallPaintHooks auto_hooks(this, context);
  PangoContext *pango = pango_layout_get_context(_layout);
  PangoContextUtil::update(pango, context);
  
  y0 -= _extents.ascent;
  double clip_x1, clip_y1, clip_x2, clip_y2;
  context.canvas().clip_extents(&clip_x1, &clip_y1, &clip_x2,  &clip_y2);
    
  int cl_y1 = pango_units_from_double(clip_y1 - y0);
  int cl_y2 = pango_units_from_double(clip_y2 - y0);
  
  int line = 0;
  PangoLayoutIter *iter = pango_layout_get_iter(_layout);
  do {
    if(line >= line_y_corrections.length()) {
      /* It might happen that pango_layout_iter_next_line() gives more lines than during resize().
         This could happen if the current font/text_shaper was disposed since the last resize()
         and its set_style() thus does not yield the same fonts any more.

         For example, this scenario happened while the current math shaper was "Matematica 10 Sans" 
         (a ConfigShaper) and FrontEnd`AddConfigShaper("...path\to\mathematica10_sans.pmath") was called:
         The call to FrontEnd`AddConfigShaper disposed the old ConfigShaperDB, but some of the shapers 
         where still in use by the document. 
      */
      break;
    }

    PangoRectangle rect;
    pango_layout_iter_get_line_extents(iter, &rect, 0);
    
    rect.y -= line_y_corrections[line];
    
    if(rect.y >= cl_y2)
      break;
      
    if(rect.y + rect.height >= cl_y1) {
      int base = pango_layout_iter_get_baseline(iter) - line_y_corrections[line];
      
      context.canvas().move_to(
        x0,
        y0 + pango_units_to_double(base));
        
      pango_cairo_show_layout_line(
        context.canvas().cairo(),
        pango_layout_iter_get_line_readonly(iter));
    }
    
    ++line;
  } while(pango_layout_iter_next_line(iter));
  pango_layout_iter_free(iter);
  
  if(!context.canvas().show_only_text) {
    context.for_each_selection_inside(this, [&](const VolatileSelection &sel) {
      if(TextSequence *seq = dynamic_cast<TextSequence*>(sel.box)) {
        if(&Impl(*seq).outermost_span() == this) {
          context.canvas().move_to(x0, y0 + _extents.ascent);
          
          seq->selection_path(
            context.canvas(),
            sel.start,
            sel.end);
            
          context.draw_selection_path();
        }
      }
    });
  }
}

void TextSequence::selection_path(Canvas &canvas, int start_text_index, int end_text_index) {
  if(start_text_index > length())
    start_text_index = length();
  if(end_text_index > length())
    end_text_index = length();
  
  TextLayoutIterator iter_start = Impl(*this).text_iterator();
  TextSequence &outermost = *iter_start.outermost_sequence();
  iter_start.skip_forward_beyond_text_pos(this, start_text_index);
  
  TextLayoutIterator iter_end = iter_start;
  iter_end.skip_forward_beyond_text_pos(this, end_text_index);
  
  float x0, y0;
  canvas.current_pos(&x0, &y0);
  
  if(start_text_index == end_text_index) {
    int x_pos;
    int line = iter_start.find_current_line_x(false, &x_pos);
    
    BoxSize size;
    float x, y;
    Impl(*this).line_extents(line, &x, &y, &size);
    
    float x1 = x0 + x + pango_units_to_double(x_pos);
    float y1 = y0 + y - size.ascent - 0.75;
    
    float x2 = x1;
    float y2 = y0 + y + size.descent + 0.75;
    
    canvas.align_point(&x1, &y1, true);
    canvas.align_point(&x2, &y2, true);
    
    canvas.move_to(x1, y1);
    canvas.line_to(x2, y2);
  }
  else {
    float last_bottom = 0;
    float spacing = pango_units_to_double(pango_layout_get_spacing(outermost._layout));
    
    int line_index = 0;
    PangoLayoutIter *pango_iter = pango_layout_get_iter(outermost._layout);
    
    int start_byte_index = iter_start.byte_index();
    int end_byte_index   = iter_end.byte_index();
    // adjust start_byte_index...
    PangoLayoutLine *prev = nullptr;
    do {
      PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);
      
      if(start_byte_index < pango_line->start_index) {
        if(prev && prev->start_index + prev->length < start_byte_index)
          start_byte_index = prev->start_index + prev->length;
          
        break;
      }
      
      prev = pango_line;
    } while(pango_layout_iter_next_line(pango_iter));
    pango_layout_iter_free(pango_iter);
    prev = nullptr;
    
    pango_iter = pango_layout_get_iter(_layout);
    do {
      PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);
      
      if(end_byte_index <= pango_line->start_index)
        break;
        
      if(start_byte_index <= pango_line->start_index + pango_line->length) {
        int *xranges;
        int num_xranges;
        float x, y;
        BoxSize size;
        
        Impl(outermost).line_extents(pango_iter, line_index, &x, &y, &size);
        if(end_byte_index > pango_line->start_index + pango_line->length)
          size.descent += spacing;
          
        if(start_byte_index < pango_line->start_index) {
          size.ascent +=  spacing;
          
          RectangleF rect(x0 + x, last_bottom, _extents.width - x, y0 + y - size.ascent - last_bottom);
          rect.normalize();
          rect.pixel_align(  canvas, false, 0);
          rect.add_rect_path(canvas, false);
        }
        
        last_bottom = y0 + y + size.descent;
        
        pango_layout_line_get_x_ranges(pango_line, start_byte_index, end_byte_index, &xranges, &num_xranges);
        
        for(int i = 0; i < num_xranges; ++i) {
          RectangleF rect(
            Point(x0 + pango_units_to_double(xranges[2 * i]),
                  y0 + y - size.ascent),
            Point(x0 + pango_units_to_double(xranges[2 * i + 1]),
                  last_bottom));
                  
          rect.pixel_align(  canvas, false, 0);
          rect.add_rect_path(canvas, false);
        }
        
        g_free(xranges);
      }
      
      ++line_index;
    } while(pango_layout_iter_next_line(pango_iter));
    
    pango_layout_iter_free(pango_iter);
  }
}

void TextSequence::on_text_changed() {
  // buffer_to_inline_sequence might now contain invalid references
  buffer_to_inline_sequence.clear();
  buffer_to_text.clear();
  _buffer_size = 0;
  if(_layout) {
    pango_layout_set_text(_layout, "", 0);
  }
  
  TextSequence &outer = Impl(*this).outermost_span();
  outer.text_changed(true);
}

Expr TextSequence::to_pmath_impl(BoxOutputFlags flags) {
  return to_pmath_impl(flags, 0, length());
}

Expr TextSequence::to_pmath_impl(BoxOutputFlags flags, int start, int end) {
  return Impl(*this).to_pmath(flags, start, end);
}

void TextSequence::load_from_object(Expr obj, BoxInputFlags options) {
  str = strings::EmptyString;
  
  boxes_invalid(true);
  text_changed(true);
  
  if(obj[0] == richmath_System_TextData && obj.expr_length() == 1)
    obj = obj[1];
    
  if(has(options, BoxInputFlags::AllowTemplateSlots))
    obj = TemplateBoxSlot::prepare_boxes(obj);
    
  if(obj.is_string())
    obj = expand_string_boxes(String(std::move(obj)));
    
  int next_box = 0;
  
  if(obj[0] == richmath_System_List) {
    for(size_t i = 1; i <= obj.expr_length(); ++i) 
      Impl(*this).append_object(next_box, obj[i], options);
  }
  else 
    Impl(*this).append_object(next_box, obj, options);
  
  for(int i = next_box; i < boxes.length();++i)
    boxes[i]->safe_destroy();
    
  boxes.length(next_box);
  
  finish_load_from_object(std::move(obj));
}

Box *TextSequence::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  ensure_layout_valid();
  
  if(direction == LogicalDirection::Forward) {
    if(*index >= length()) {
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
  }
  else {
    if(*index <= 0) {
      if(auto par = parent()) {
        if(!par->exitable())
          return this;
        
        *index = _index + 1;
        return par->move_logical(LogicalDirection::Backward, true, index);
      }
      return this;
    }
    
    if(*index > length()) {
      *index = length();
      return this;
    }
  }

  auto iter = Impl(*this).text_iterator();
  
  iter.skip_forward_beyond_text_pos(this, *index);
  
  if(direction == LogicalDirection::Forward) {
    if(jumping) {
      iter.move_next_char();
      while(iter.has_more_bytes() && (!iter.is_word_boundary() || !iter.is_cursor_position())) {
        iter.move_next_char();
      }
      
      while(iter.has_more_bytes() && (iter.is_whitespace() || !iter.is_cursor_position())) {
        iter.move_next_char();
      }
    }
    else {
      while(iter.has_more_bytes()) {
        if(auto box = iter.current_box()) {
          *index = -1;
          return box->move_logical(LogicalDirection::Forward, true, index);
        }
        
        auto seq = iter.current_sequence();
        if(seq != this) {
          *index = seq->index_in_ancestor(this, -1);
          if(*index != -1)
            return this;
        }
        
        iter.move_next_char();
        if(!iter.has_more_bytes() || iter.is_cursor_position())
          break;
      }
    }
  }
  else {
    if(jumping) {
      iter.move_previous_char();

      while(iter.byte_index() > 0 && (iter.is_whitespace() || !iter.is_cursor_position())) {
        iter.move_previous_char();
      }
      
      if(!iter.current_box()) {
        while(iter.byte_index() > 0 && (!iter.is_word_boundary() || !iter.is_cursor_position())) {
          iter.move_previous_char();
        }
      }
    }
    else {
      while(iter.byte_index() > 0) {
        iter.move_previous_char();
        
        if(auto box = iter.current_box()) {
          *index = box->length() + 1;
          return box->move_logical(LogicalDirection::Backward, true, index);
        }
        
        auto seq = iter.current_sequence();
        if(seq != this) {
          *index = seq->index_in_ancestor(this, -1);
          if(*index != -1) {
            ++*index;
            return this;
          }
        }
        
        if(iter.byte_index() <= 0 || iter.is_cursor_position())
          break;
      }
    }
  }

  *index = iter.text_index();
  return iter.current_sequence();
}

Box *TextSequence::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  TextLayoutIterator iter = Impl(*this).text_iterator();
  TextSequence &outermost = *iter.outermost_sequence();
  
  int numlines = pango_layout_get_line_count(outermost._layout);
  int line;
  float x = *index_rel_x;
  
  if(*index < 0) {
    if(direction == LogicalDirection::Forward)
      line = 0;
    else
      line = numlines - 1;
  }
  else {
    iter.skip_forward_beyond_text_pos(this, *index);
    
    int px;
    line = iter.find_current_line_x(false, &px);
    
    float lx;
    Impl(outermost).line_extents(line, &lx, nullptr, nullptr);
    
    x += lx + pango_units_to_double(px);
    if(direction == LogicalDirection::Forward)
      ++line;
    else
      --line;
  }
  
  if(line >= 0 && line < numlines) {
    float lx;
    Impl(outermost).line_extents(line, &lx, nullptr, nullptr);
    PangoLayoutLine *pll = pango_layout_get_line_readonly(outermost._layout, line);
    
    int byte_index, trailing;
    pango_layout_line_x_to_index(pll, pango_units_from_double(x - lx), &byte_index, &trailing);
    
    iter.rewind_to_byte(byte_index);
    
    if(auto box = iter.current_box()) { // TODO: ensure_boxes_valid()  ?
      int px;
      pango_layout_line_index_to_x(pll, byte_index, false, &px);
      
      *index_rel_x = x - lx - pango_units_to_double(px);
      *index = -1;
      return box->move_vertical(direction, index_rel_x, index, false);
    }
    
    int px;
    pango_layout_line_index_to_x(pll, byte_index, trailing > 0, &px);
    *index_rel_x = x - pango_units_to_double(px);
    
    while(trailing-- > 0)
      iter.move_next_char();
    
    *index = iter.text_index();
    return iter.current_sequence();
  }
  
  if(auto par = parent()) {
    *index_rel_x = x;
    *index = _index;
    return par->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

VolatileSelection TextSequence::mouse_selection(Point pos, bool *was_inside_start) {
  BoxSize line_size;
  float line_x, line_y;
  
  TextLayoutIterator iter = Impl(*this).text_iterator();
  TextSequence &outermost = *iter.outermost_sequence();
  
  int line_index = 0;
  PangoLayoutIter *pango_iter = Impl(*this).new_pango_iter();
  do {
    Impl(*this).line_extents(pango_iter, line_index, &line_x, &line_y, &line_size);
    
    if(pos.y <= line_y + line_size.descent)
      break;
      
    ++line_index;
  } while(pango_layout_iter_next_line(pango_iter));
  
  PangoLayoutLine *pango_line = pango_layout_iter_get_line_readonly(pango_iter);
  pango_layout_iter_free(pango_iter);
  
  int byte_index, trailing;
  pango_layout_line_x_to_index(
    pango_line,
    pango_units_from_double(pos.x - line_x),
    &byte_index, &trailing);
    
  iter.rewind_to_byte(byte_index);
  
  if(auto box = iter.current_box()) { // TODO: ensure_boxes_valid() ?
    int pango_x;
    pango_layout_line_index_to_x(pango_line, byte_index, 0, &pango_x);
    
    auto px = pos.x - (line_x + pango_units_to_double(pango_x));
    if(trailing == 0 || px <= box->extents().width + 0.375f) {
      pos.y -= line_y;
      pos.x = px;
      
      return box->mouse_selection(pos, was_inside_start);
    }
  }
  
  int x_pos;
  pango_layout_line_index_to_x(pango_line, byte_index, trailing > 0, &x_pos);
  *was_inside_start = (0 <= pos.x && (trailing == 0 || pango_units_to_double(x_pos) < pos.x));
  
  while(trailing-- > 0)
    iter.move_next_char();
  
  return VolatileSelection(iter.current_sequence(), iter.text_index());
}

void TextSequence::child_transformation(int index, cairo_matrix_t *matrix) {
  auto delta = Impl(*this).total_offest_to_index(index);
  
  if(inline_span()) {
    int i = this->index();
    Box* box = parent();
    while(box) {
      if(auto seq = dynamic_cast<TextSequence*>(box)) {
        delta-= Impl(*seq).total_offest_to_index(i);
        break;
      }
      i = box->index();
      box = box->parent();
    }
  }
  
  cairo_matrix_translate(matrix, delta.x, delta.y);
}

bool TextSequence::request_repaint(const RectangleF &rect) { // See MathSequence::request_repaint()
  if(inline_span()) {
    TextSequence &outer = Impl(*this).outermost_span();
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

bool TextSequence::visible_rect(RectangleF &rect, Box *top_most) { // See MathSequence::visible_rect()
  if(inline_span()) {
    TextSequence &outer = Impl(*this).outermost_span();
    
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

int TextSequence::get_line(int index, int guide) {
  if(text_changed()) {
    pmath_debug_print("[get_line ill-defined: text_changed()]\n");
    return 0;
  }

  TextLayoutIterator iter = Impl(*this).text_iterator();
  
  iter.skip_forward_beyond_text_pos(this, index);
  
  return iter.find_current_line(false);
}

void TextSequence::get_line_heights(int line, float *ascent, float *descent) {
  TextSequence &outermost = Impl(*this).outermost_span();
  
  if(line < 0 || line >= outermost.line_y_corrections.length()) {
    *ascent  = 0;
    *descent = 0;
    return;
  }
  
  BoxSize size;
  float x, y;
  Impl(outermost).line_extents(line, &x, &y, &size);
  *ascent  = size.ascent;
  *descent = size.descent;
}

TextSequence &TextSequence::outermost_sequence() {
  return Impl(*this).outermost_span();
}

TextLayoutIterator TextSequence::outermost_layout_iter() {
  return Impl(*this).text_iterator();
}

void TextSequence::ensure_layout_valid() {
  if(!text_changed())
    return;
  
  Impl(outermost_sequence()).update_layout();

  RICHMATH_ASSERT(!text_changed());
}

bool TextSequence::is_word_boundary(int i) {
  if(i <= 0 || i >= length())
    return true;

  ensure_layout_valid();
  
  auto iter = Impl(*this).text_iterator();
  iter.skip_forward_beyond_text_pos(this, i);
  return iter.is_word_boundary();
}

//} ... class TextSequence

//{ class TextSequence::Impl ...

Expr TextSequence::Impl::to_pmath(BoxOutputFlags flags, int start, int end) {
  if(end <= start || start < 0 || end > self.length())
    return strings::EmptyString;
    
  int boxi = 0;
  while(boxi < self.boxes.length() && self.boxes[boxi]->index() < start)
    ++boxi;
    
  if(boxi >= self.boxes.length() || self.boxes[boxi]->index() >= end) 
    return add_debug_metadata(self.text().part(start, end - start), flags, start, end);
  
  Gather g;
  
  int next = self.boxes[boxi]->index();
  while(next < end) {
    if(start < next)
      g.emit(add_debug_metadata(self.text().part(start, next - start), flags, start, next));
      
    g.emit(add_debug_metadata(self.boxes[boxi]->to_pmath(flags), flags, next, next + 1));
    
    start = next + 1;
    ++boxi;
    if(boxi >= self.boxes.length())
      break;
    next = self.boxes[boxi]->index();
  }
  
  if(start < end)
    g.emit(add_debug_metadata(self.text().part(start, end - start), flags, start, end));
    
  return add_debug_metadata(g.end(), flags, start, end);
}

Expr TextSequence::Impl::add_debug_metadata(Expr expr, BoxOutputFlags flags, int start, int end) {
  if(!has(flags, BoxOutputFlags::WithDebugMetadata))
    return expr;
  
  pmath_t obj = expr.release();
  obj = pmath_try_set_debug_metadata(
          obj, 
          SelectionReference(self.id(), start, end).to_pmath().release());
  return Expr{ obj };
}

void TextSequence::Impl::append_object(int &next_box, Expr object, BoxInputFlags options) {
  if(object.is_string()) {
    String s(object.release());
    
    // skip PMATH_CHAR_BOX ...
    const uint16_t *buf = s.buffer();
    int             len = s.length();
    
    int start = 0;
    while(start < len) {
      int next = start;
      while(next < len && buf[next] != PMATH_CHAR_BOX)
        ++next;
        
      self.insert(self.length(), s.part(start, next - start));
      self.text_changed(true);
      
      start = next + 1;
    }
    
    return;
  }
  
  if(next_box < self.boxes.length()) {
    if(self.boxes[next_box]->try_load_from_object(object, options)) {
      self.str += PMATH_CHAR_BOX;
      ++next_box;
      return;
    }
  }
  
  Box *box = BoxFactory::create_empty_box(LayoutKind::Text, object);
  
  int i = self.str.length();
  self.str += PMATH_CHAR_BOX;
  if(next_box < self.boxes.length()) {
    self.boxes[next_box]->safe_destroy();
    self.boxes[next_box] = box;
  }
  else
    self.boxes.add(box);
  
  self.adopt(box, i);
  if(!box->try_load_from_object(object, options)) {
    box = new ErrorSection(object);
    self.boxes[next_box]->safe_destroy();
    self.boxes[next_box] = box;
    self.adopt(box, i);
  }
  
  ++next_box;
}

inline PangoLayoutIter *TextSequence::Impl::new_pango_iter() {
  return pango_layout_get_iter(outermost_span()._layout);
}

void TextSequence::Impl::resize_boxes(Context &context) {
  self.ensure_boxes_valid();
  
  for(auto box : self.boxes) {
    if(auto sub = box->as_inline_text_span()) {
      box->resize_inline(context);
      sub->inline_span(true);
      sub->em = self.get_em();
      Impl(*sub).resize_boxes(context);
    }
    else
      box->resize(context);
  }
}

void TextSequence::Impl::update_layout() {
  {
    Array<char> utf8;
    Utf8Writer(self, utf8).append_all();
    pango_layout_set_text(self._layout, utf8.items(), utf8.length());
    self._buffer_size = utf8.length();
  }
  
  {
    PangoAttrList *attr_list = pango_attr_list_new();
    AttributeWriter(self, attr_list).apply_all();
    pango_layout_set_attributes(self._layout, attr_list);
    pango_attr_list_unref(attr_list);
  }
}

inline TextSequence &TextSequence::Impl::outermost_span() {
  return self.inline_span() ? parent_outermost_span() : self;
}

inline TextSequence &TextSequence::Impl::parent_outermost_span() {
  for(Box *box = self.parent(); box; box = box->parent()) {
    if(auto seq = dynamic_cast<TextSequence*>(box)) {
      if(!seq->inline_span())
        return *seq;
    }
  }
  
  //pmath_debug_print("[inconstistent inline_span() flag]\n");
  return self;
}

Vector2F TextSequence::Impl::total_offest_to_index(int index) {
  TextLayoutIterator iter = text_iterator();
  TextSequence &outermost = *iter.outermost_sequence();
  
  iter.skip_forward_beyond_text_pos(&self, index);
  
  int px;
  int line = iter.find_current_line_x(false, &px);
  
  float x, y;
  Impl(outermost).line_extents(line, &x, &y, nullptr);
  
  x += pango_units_to_double(px);
  return {x, y};
}

void TextSequence::Impl::line_extents(PangoLayoutIter *iter, int line, float *x, float *y, BoxSize *size) {
  PangoRectangle ink, logic;
  pango_layout_iter_get_line_extents(iter, &ink, &logic);
  int base = pango_layout_iter_get_baseline(iter);
  
  if(size) {
    size->width   = pango_units_to_double(logic.width);
    size->ascent  = pango_units_to_double(base - ink.y);
    size->descent = pango_units_to_double(ink.height) - size->ascent;
    
    if(size->ascent < 0.75 * self.em)
      size->ascent = 0.75 * self.em;
      
    if(size->descent < 0.25 * self.em)
      size->descent = 0.25 * self.em;
  }
  
  if(x)
    *x = pango_units_to_double(logic.x);
    
  if(line < self.line_y_corrections.length())
    base -= self.line_y_corrections[line];
    
  if(y)
    *y = pango_units_to_double(base) - self._extents.ascent;
}

void TextSequence::Impl::line_extents(int line, float *x, float *y, BoxSize *size) {
  int i = line;
  PangoLayoutIter *iter = new_pango_iter();
  while(i > 0 && pango_layout_iter_next_line(iter)) {
    --i;
  }
  
  line_extents(iter, line, x, y, size);
  
  pango_layout_iter_free(iter);
}

//} ... class TextSequence::Impl

//{ class TextSequence::Impl::Utf8Writer ...

TextSequence::Impl::Utf8Writer::Utf8Writer(TextSequence &owner, Array<char> &buffer)
: owner{owner},
  buffer{buffer},
  b2t_iter{owner.buffer_to_text.begin()},
  b2seq_iter{owner.buffer_to_inline_sequence.begin()}
{
}

void TextSequence::Impl::Utf8Writer::append_all() {
  append_all(owner);
}

void TextSequence::Impl::Utf8Writer::append_all(TextSequence &seq) {
  seq.text_changed(false);
  
  ArrayView<const uint16_t> buf = buffer_view(seq.text());
  int box_index = -1;
  for(int i = 0; i < buf.length(); ++i) {
    int pos = i;
    uint32_t unichar = buf[i];
    if(is_utf16_high(unichar) && i + 1 < buf.length() && is_utf16_low(buf[i+1])) {
      uint32_t hi = unichar;
      uint32_t lo = buf[i + 1];
      ++i;
      unichar = 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
    }
    
    if(unichar == PMATH_CHAR_BOX) {
      Box *box = seq.boxes[++box_index];
      RICHMATH_ASSERT(box->index() == pos);
      
      append_box_glyphs(seq, pos, box);
      continue;
    }
    
    if(unichar == 0) { // embedded NUL would be truncated -> buffer overflow
      unichar = 0xFFFD;
    } else if(unichar <= 0xFFFF && (is_utf16_high(unichar) || is_utf16_low(unichar))) { // stand alone UTF-16 surrogate char
      unichar = 0xFFFD;
    }
    
    if(unichar <= 0x7F) {
      int start = append_single_char_bytes(seq, pos, 1);
      buffer[start] = (char)unichar;
    }
    else if(unichar <= 0x7FF) {
      int start = append_single_char_bytes(seq, pos, 2);
      buffer[start]   = 0xC0 | (unichar >> 6);
      buffer[start+1] = 0x80 | (unichar & 0x3F);
    }
    else if(unichar <= 0xFFFF) {
      int start = append_single_char_bytes(seq, pos, 3);
      buffer[start]   = 0xE0 | ( unichar >> 12);
      buffer[start+1] = 0x80 | ((unichar >>  6) & 0x3F);
      buffer[start+2] = 0x80 | ( unichar        & 0x3F);
    }
    else if(unichar <= 0x10FFFF) {
      int start = append_single_char_bytes(seq, pos, 4);
      buffer[start]   = 0xF0 | ( unichar >> 18);
      buffer[start+1] = 0x80 | ((unichar >> 12) & 0x3F);
      buffer[start+2] = 0x80 | ((unichar >>  6) & 0x3F);
      buffer[start+3] = 0x80 | ( unichar        & 0x3F);
    }
  }

  b2t_iter.rewind_to(buffer.length());
  b2t_iter.reset_rest(buf.length());
  b2seq_iter.rewind_to(buffer.length());
  b2seq_iter.reset_rest(&seq);
}

void TextSequence::Impl::Utf8Writer::append_box_glyphs(TextSequence &seq, int pos, Box *box) {
  if(auto sub = box->as_inline_text_span()) {
    append_all(*sub);
    return;
  }
  
  int len = Utf8ObjectReplacementCharLen;
  int start = append_single_char_bytes(seq, pos, len);
  memcpy(buffer.items() + start, Utf8ObjectReplacementChar, len);
}

int TextSequence::Impl::Utf8Writer::append_single_char_bytes(TextSequence &seq, int pos, int count) {
  ARRAY_ASSERT(count >= 0);
  
  int old_len = buffer.length();
  
  buffer.length(old_len + count);
  memset(buffer.items() + old_len, 0, sizeof(buffer[0]) * count);
  
  b2t_iter.rewind_to(old_len);
  
  if(count == 4) {
    // 4 UTF-8 bytes mean U+10000 .. U+10FFFF, i.e. 2 UTF-16 code points.
    // Only resetting the first three byte positions will cause the 4'th to point to the second UTF-16 code point.
    // This may be noticable during selection of the last character of the text (selection problems without this hack).
    count = 3;
  }
  
  while(count--) {
    b2t_iter.reset_rest(pos);
    ++b2t_iter;
  }
  
  b2seq_iter.rewind_to(old_len);
  b2seq_iter.reset_rest(&seq == &owner ? nullptr : &seq);
  
  return old_len;
}

//} ... class TextSequence::Impl::Utf8Writer

//{ class TextSequence::Impl::AttributeWriter ...

TextSequence::Impl::AttributeWriter::AttributeWriter(TextSequence &owner, PangoAttrList *attr_list)
  : iter{owner},
    attr_list{attr_list}
{
}

void TextSequence::Impl::AttributeWriter::apply_all() {
  visit_inline_sequence(iter.outermost_sequence());
}

void TextSequence::Impl::AttributeWriter::visit_inline_sequence(TextSequence *seq) {
  seq->ensure_boxes_valid();
  
  for(auto box : seq->boxes)
    visit_box(seq, box);
}

void TextSequence::Impl::AttributeWriter::visit_box(TextSequence *seq, Box *box) {
  iter.skip_forward_beyond_text_pos(seq, box->index());
  int start_byte = iter.byte_index();
  
  if(auto sub = box->as_inline_text_span()) {
    visit_inline_sequence(sub);
    // TODO: add PangeAttributes for Style box
    return;
  }
  
  PangoRectangle rect;
  rect.x      = 0;
  rect.y      = pango_units_from_double(-box->extents().ascent);
  rect.width  = pango_units_from_double(box->extents().width);
  rect.height = pango_units_from_double(box->extents().height());
  
  PangoAttribute *shape = pango_attr_shape_new_with_data(&rect, &rect, box, nullptr, nullptr);
           
  shape->start_index = start_byte;
  shape->end_index   = start_byte + Utf8ObjectReplacementCharLen;
  pango_attr_list_insert(attr_list, shape);
}

//} ... class TextSequence::Impl::AttributeWriter
