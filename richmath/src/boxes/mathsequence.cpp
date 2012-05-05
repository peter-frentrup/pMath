#include <boxes/mathsequence.h>

#include <climits>
#include <cmath>

#include <boxes/buttonbox.h>
#include <boxes/checkboxbox.h>
#include <boxes/dynamicbox.h>
#include <boxes/dynamiclocalbox.h>
#include <boxes/errorbox.h>
#include <boxes/fillbox.h>
#include <boxes/fractionbox.h>
#include <boxes/framebox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/interpretationbox.h>
#include <boxes/numberbox.h>
#include <boxes/ownerbox.h>
#include <boxes/progressindicatorbox.h>
#include <boxes/radicalbox.h>
#include <boxes/radiobuttonbox.h>
#include <boxes/section.h>
#include <boxes/setterbox.h>
#include <boxes/sliderbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/tooltipbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>
#include <boxes/graphics/graphicsbox.h>

#include <eval/binding.h>
#include <eval/application.h>

#include <graphics/context.h>
#include <graphics/scope-colorizer.h>

#include <util/spanexpr.h>

using namespace richmath;

static const float ref_error_indicator_height = 1 / 3.0f;

static inline bool char_is_white(uint16_t ch) {
  return ch == ' ' || ch == '\n';
}

class ScanData {
  public:
    MathSequence *sequence;
    int current_box;
    int flags;
};

//{ class MathSequence ...

MathSequence::MathSequence()
  : AbstractSequence(),
  str(""),
  boxes_invalid(false),
  spans_invalid(false)
{
}

MathSequence::~MathSequence() {
  for(int i = 0; i < boxes.length(); ++i)
    delete boxes[i];
}

Box *MathSequence::item(int i) {
  ensure_boxes_valid();
  return boxes[i];
}

String MathSequence::raw_substring(int start, int length) {
  assert(start >= 0);
  assert(length >= 0);
  assert(start + length <= str.length());
  
  return str.part(start, length);
}

uint32_t MathSequence::char_at(int pos) {
  if(pos < 0 || pos > str.length())
    return 0;
    
  const uint16_t *buf = str.buffer();
  
  if(is_utf16_high(buf[pos]) && is_utf16_low((buf[pos + 1]))) {
    uint32_t hi = buf[pos];
    uint32_t lo = buf[pos + 1];
    
    return 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
  }
  
  return buf[pos];
}

bool MathSequence::expand(const BoxSize &size) {
  if(boxes.length() == 1 && glyphs.length() == 1 && str.length() == 1) {
    if(boxes[0]->expand(size)) {
      _extents = boxes[0]->extents();
      glyphs[0].right  = _extents.width;
      lines[0].ascent  = _extents.ascent;
      lines[0].descent = _extents.descent;
      return true;
    }
  }
  else {
    float uw;
    float w = _extents.width;
    
    hstretch_lines(
      size.width,
      size.width,
      &uw);
      
    return w != _extents.width;
  }
  
  return false;
}

void MathSequence::resize(Context *context) {
  glyphs.length(str.length());
  glyphs.zeromem();
  
  ensure_boxes_valid();
  ensure_spans_valid();
  
  em = context->canvas->get_font_size();
  
  float old_scww = context->section_content_window_width;
  context->section_content_window_width = HUGE_VAL;
  
  int box = 0;
  int pos = 0;
  while(pos < glyphs.length())
    resize_span(context, spans[pos], &pos, &box);
    
  if(context->show_auto_styles) {
    ScopeColorizer colorizer(this);
    
    pos = 0;
    while(pos < glyphs.length())
      colorizer.comments_colorize_span(spans[pos], &pos);
      
    pos = 0;
    while(pos < glyphs.length()) {
      SpanExpr *se = new SpanExpr(pos, spans[pos], this);
      
      if(se->count() == 0 || !se->item_as_text(0).equals("/*")) {
        colorizer.syntax_colorize_spanexpr(        se);
        colorizer.arglist_errors_colorize_spanexpr(se, em * ref_error_indicator_height);
      }
      
      pos = se->end() + 1;
      delete se;
    }
  }
  
  if(context->math_spacing) {
    float ca = 0;
    float cd = 0;
    float a = 0;
    float d = 0;
    
    if(glyphs.length() == 1 &&
        !dynamic_cast<UnderoverscriptBox*>(_parent))
    {
      pmath_token_t tok = pmath_token_analyse(str.buffer(), 1, NULL);
      
      if(tok == PMATH_TOK_INTEGRAL || tok == PMATH_TOK_PREFIX) {
        context->math_shaper->vertical_stretch_char(
          context,
          a,
          d,
          true,
          str[0],
          &glyphs[0]);
          
        BoxSize size;
        context->math_shaper->vertical_glyph_size(
          context,
          str[0],
          glyphs[0],
          &size.ascent,
          &size.descent);
          
        size.bigger_y(&ca, &cd);
        size.bigger_y(&a,  &d);
      }
      else {
        box = 0;
        pos = 0;
        while(pos < glyphs.length())
          stretch_span(context, spans[pos], &pos, &box, &ca, &cd, &a, &d);
      }
    }
    else {
      box = 0;
      pos = 0;
      while(pos < glyphs.length())
        stretch_span(context, spans[pos], &pos, &box, &ca, &cd, &a, &d);
    }
    
    enlarge_space(context);
  }
  
  {
    _extents.width = 0;
    const uint16_t *buf = str.buffer();
    for(pos = 0; pos < glyphs.length(); ++pos)
      if(buf[pos] == '\n')
        glyphs[pos].right = _extents.width;
      else
        glyphs[pos].right = _extents.width += glyphs[pos].right;
  }
  
  lines.length(1);
  lines[0].end = glyphs.length();
  lines[0].ascent = lines[0].descent = 0;
  lines[0].indent = 0;
  lines[0].continuation = 0;
  
  context->section_content_window_width = old_scww;
  
  split_lines(context);
  if(dynamic_cast<Section*>(_parent)) {
    hstretch_lines(
      context->width,
      context->section_content_window_width,
      &context->sequence_unfilled_width);
  }
  
  const uint16_t *buf = str.buffer();
  int line = 0;
  pos = 0;
  box = 0;
  float x = 0;
  _extents.descent = _extents.width = 0;
  if(lines.length() > 1) {
    lines[0].ascent  = 0.75f * em;
    lines[0].descent = 0.25f * em;
  }
  while(pos < glyphs.length()) {
    if(pos == lines[line].end) {
      if(pos > 0) {
        double indent = indention_width(lines[line].indent);
        
        if(_extents.width < glyphs[pos - 1].right - x + indent)
          _extents.width  = glyphs[pos - 1].right - x + indent;
        x = glyphs[pos - 1].right;
      }
      
      _extents.descent += lines[line].ascent + lines[line].descent + line_spacing();
      
      ++line;
      lines[line].ascent  = 0.75f * em;
      lines[line].descent = 0.25f * em;
    }
    
    if(buf[pos] == PMATH_CHAR_BOX) {
      boxes[box]->extents().bigger_y(&lines[line].ascent, &lines[line].descent);
      ++box;
    }
    else if(glyphs[pos].is_normal_text) {
      context->text_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        &lines[line].ascent,
        &lines[line].descent);
    }
    else {
      context->math_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        &lines[line].ascent,
        &lines[line].descent);
    }
    
    ++pos;
  }
  
  if(pos > 0) {
    double indent = indention_width(lines[line].indent);
    
    if(_extents.width < glyphs[pos - 1].right - x + indent)
      _extents.width  = glyphs[pos - 1].right - x + indent;
  }
  
  if(line + 1 < lines.length()) {
    _extents.descent += lines[line].ascent + lines[line].descent;
    ++line;
    lines[line].ascent = 0.75f * em;
    lines[line].descent = 0.25f * em;
  }
  _extents.ascent = lines[0].ascent;
  _extents.descent += lines[line].ascent + lines[line].descent - lines[0].ascent;
  
  if(_extents.width < 0.75 && lines.length() > 1) {
    _extents.width = 0.75;
  }
  
  if(context->sequence_unfilled_width == -HUGE_VAL)
    context->sequence_unfilled_width = _extents.width;
}

void MathSequence::colorize_scope(SyntaxState *state) {
  assert(glyphs.length() == spans.length());
  assert(glyphs.length() == str.length());
  
  ScopeColorizer colorizer(this);
  
  int pos = 0;
  while(pos < glyphs.length()) {
    SpanExpr *se = new SpanExpr(pos, spans[pos], this);
    
    colorizer.scope_colorize_spanexpr(state, se);
    
    pos = se->end() + 1;
    delete se;
  }
}

void MathSequence::paint(Context *context) {
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  
  int default_color = context->canvas->get_color();
  SharedPtr<MathShaper> default_math_shaper = context->math_shaper;
  
  context->syntax->glyph_style_colors[GlyphStyleNone] = default_color;
  
  float y = y0 - lines[0].ascent;
  const uint16_t *buf = str.buffer();
  
  double clip_x1, clip_y1, clip_x2, clip_y2;
  cairo_clip_extents(
    context->canvas->cairo(),
    &clip_x1, &clip_y1,
    &clip_x2,  &clip_y2);
    
  int line = 0;
  // skip invisible lines:
  while(line < lines.length()) {
    float h = lines[line].ascent + lines[line].descent;
    if(y + h >= clip_y1)
      break;
      
    y += h + line_spacing();
    ++line;
  }
  
  if(line < lines.length()) {
    float glyph_left = 0;
    int box = 0;
    int pos = 0;
    
    if(line > 0)
      pos = lines[line-1].end;
      
    if(pos > 0)
      glyph_left = glyphs[pos-1].right;
      
    bool have_style = false;
    bool have_slant = false;
    for(; line < lines.length() && y < clip_y2; ++line) {
      float x_extra = x0 + indention_width(lines[line].indent);
      
      if(pos > 0)
        x_extra -= glyphs[pos - 1].right;
        
      if(pos < glyphs.length())
        x_extra -= glyphs[pos].x_offset;
        
      y += lines[line].ascent;
      
      for(; pos < lines[line].end; ++pos) {
        if(buf[pos] <= '\n') {
          glyph_left = glyphs[pos].right;
          continue;
        }
        
        if(have_style || glyphs[pos].style) {
          int color = context->syntax->glyph_style_colors[glyphs[pos].style];
          
          context->canvas->set_color(color);
          have_style = color != default_color;
        }
        
        if(have_slant || glyphs[pos].slant) {
          if(glyphs[pos].slant == FontSlantItalic) {
            context->math_shaper = default_math_shaper->set_style(
                                     default_math_shaper->get_style() + Italic);
            have_slant = true;
          }
          else if(glyphs[pos].slant == FontSlantPlain) {
            context->math_shaper = default_math_shaper->set_style(
                                     default_math_shaper->get_style() - Italic);
            have_slant = true;
          }
          else {
            context->math_shaper = default_math_shaper;
            have_slant = false;
          }
        }
        
        if(buf[pos] == PMATH_CHAR_BOX) {
          while(boxes[box]->index() < pos)
            ++box;
            
          context->canvas->move_to(glyph_left + x_extra + glyphs[pos].x_offset, y);
          
          boxes[box]->paint(context);
          
          context->syntax->glyph_style_colors[GlyphStyleNone] = default_color;
          ++box;
        }
        else if(glyphs[pos].index ||
                glyphs[pos].composed ||
                glyphs[pos].horizontal_stretch)
        {
          if(glyphs[pos].is_normal_text) {
            context->text_shaper->show_glyph(
              context,
              glyph_left + x_extra,
              y,
              buf[pos],
              glyphs[pos]);
          }
          else {
            context->math_shaper->show_glyph(
              context,
              glyph_left + x_extra,
              y,
              buf[pos],
              glyphs[pos]);
          }
        }
        
        if(glyphs[pos].missing_after) {
          float d = em * ref_error_indicator_height * 2 / 3.0f;
          float dd = d / 4;
          
          context->canvas->move_to(glyphs[pos].right + x_extra, y + em / 8);
          if(pos + 1 < glyphs.length())
            context->canvas->rel_move_to(glyphs[pos + 1].x_offset / 2, 0);
            
          context->canvas->rel_line_to(-d, d);
          context->canvas->rel_line_to(dd, dd);
          context->canvas->rel_line_to(d - dd, dd - d);
          context->canvas->rel_line_to(d - dd, d - dd);
          context->canvas->rel_line_to(dd, -dd);
          context->canvas->rel_line_to(-d, -d);
          
          context->canvas->close_path();
          context->canvas->set_color(
            context->syntax->glyph_style_colors[GlyphStyleMissingArg]);
          context->canvas->fill();
          
          have_style = true;
        }
        
        glyph_left = glyphs[pos].right;
      }
      
      if(lines[line].continuation) {
        GlyphInfo gi;
        memset(&gi, 0, sizeof(GlyphInfo));
        uint16_t cont = CHAR_LINE_CONTINUATION;
        context->math_shaper->decode_token(
          context,
          1,
          &cont,
          &gi);
          
        context->math_shaper->show_glyph(
          context,
          glyph_left + x_extra,
          y,
          cont,
          gi);
      }
      
      y += lines[line].descent + line_spacing();
    }
    
  }
  
  if(context->selection.get() == this && !context->canvas->show_only_text) {
    context->canvas->move_to(x0, y0);
    
    selection_path(
      context,
      context->canvas,
      context->selection.start,
      context->selection.end);
      
    context->draw_selection_path();
  }
  
  context->canvas->set_color(default_color);
  context->math_shaper = default_math_shaper;
}

void MathSequence::selection_path(Canvas *canvas, int start, int end) {
  selection_path(0, canvas, start, end);
}

void MathSequence::selection_path(Context *opt_context, Canvas *canvas, int start, int end) {
  float x0, y0, x1, y1, x2, y2;
  
  canvas->current_pos(&x0, &y0);
  
  y0 -= lines[0].ascent;
  y1 = y0;
  
  int startline = 0;
  while(startline < lines.length() && start >= lines[startline].end) {
    y1 += lines[startline].ascent + lines[startline].descent + line_spacing();
    ++startline;
  }
  
  if(startline == lines.length()) {
    --startline;
    y1 -= lines[startline].ascent + lines[startline].descent + line_spacing();
  }
  
  y2 = y1;
  int endline = startline;
  while(endline < lines.length() && end >= lines[endline].end) {
    y2 += lines[endline].ascent + lines[endline].descent + line_spacing();
    ++endline;
  }
  
  if(endline == lines.length()) {
    --endline;
    y2 -= lines[endline].ascent + lines[endline].descent + line_spacing();
  }
  
  x1 = x0;
  if(start > 0)
    x1 += glyphs[start - 1].right;
    
  if(startline > 0) {
    x1 -= glyphs[lines[startline - 1].end - 1].right;
    
    if(start > lines[startline - 1].end) {
      x1 -= glyphs[lines[startline - 1].end].x_offset;
      
      if(start < glyphs.length())
        x1 += glyphs[start].x_offset / 2;
    }
  }
  else if(start < glyphs.length())
    x1 += glyphs[start].x_offset / 2;
    
  x1 += indention_width(lines[startline].indent);
  
  
  x2 = x0;
  if(end > 0)
    x2 += glyphs[end - 1].right;
    
  if(endline > 0) {
    x2 -= glyphs[lines[endline - 1].end - 1].right;
    
    if(end > lines[endline - 1].end) {
      x2 -= glyphs[lines[endline - 1].end].x_offset;
      
      if(end < glyphs.length())
        x2 += glyphs[end].x_offset / 2;
    }
  }
  else if(end < glyphs.length())
    x2 += glyphs[end].x_offset / 2;
    
  x2 += indention_width(lines[endline].indent);
  
  
  if(endline == startline) {
    float a = 0.5 * em;
    float d = 0;
    
    if(opt_context) {
      if(start == end) {
        const uint16_t *buf = str.buffer();
        int box = 0;
        
        for(int i = 0; i < start; ++i)
          if(buf[i] == PMATH_CHAR_BOX)
            ++box;
            
        caret_size(opt_context, start, box, &a, &d);
      }
      else {
        boxes_size(
          opt_context,
          start,
          end,
          &a, &d);
      }
    }
    else {
      a = lines[startline].ascent;
      d = lines[startline].descent;
    }
    
    y1 += lines[startline].ascent;
    y2 = y1 + d + 1;
    y1 -= a + 1;
    
    if(start == end) {
      canvas->align_point(&x1, &y1, true);
      canvas->align_point(&x2, &y2, true);
      
      canvas->move_to(x1, y1);
      canvas->line_to(x2, y2);
    }
    else
      canvas->pixrect(x1, y1, x2, y2, false);
  }
  else {
    y2 = y1;
    for(int line = startline; line <= endline; ++line)
      y2 += lines[line].ascent + lines[line].descent + line_spacing();
    y2 -= line_spacing();
    
    /*    1----3
          |    |
      7---8    |
      |      5-4
      |      |
      6------2
     */
    
    float x3, y3, x4, y4, x5, y5, x6, y6, x7, y7, x8, y8;
    
    x3 = x4 = x0 + _extents.width;
    x5 = x2;
    x6 = x7 = x0;
    x8 = x1;
    
    y3 = y1;
    y4 = y5 = y2 - lines[endline].ascent - lines[endline].descent - line_spacing() / 2;
    y6 = y2;
    y7 = y8 = y1 + lines[startline].ascent + lines[startline].descent + line_spacing() / 2;
    
    canvas->align_point(&x1, &y1, false);
    canvas->align_point(&x2, &y2, false);
    canvas->align_point(&x3, &y3, false);
    canvas->align_point(&x4, &y4, false);
    canvas->align_point(&x5, &y5, false);
    canvas->align_point(&x6, &y6, false);
    canvas->align_point(&x7, &y7, false);
    canvas->align_point(&x8, &y8, false);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x3, y3);
    canvas->line_to(x4, y4);
    canvas->line_to(x5, y5);
    canvas->line_to(x2, y2);
    canvas->line_to(x6, y6);
    canvas->line_to(x7, y7);
    canvas->line_to(x8, y8);
    canvas->close_path();
  }
}

Expr MathSequence::to_pmath(int flags) {
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = flags;
  
  ensure_spans_valid();
  
  return Expr(pmath_boxes_from_spans(
                spans.array(),
                str.get(),
                flags & BoxFlagParseable,
                box_at_index,
                &data));
}

Expr MathSequence::to_pmath(int flags, int start, int end) {
  if(start == 0 && end >= length())
    return to_pmath(flags);
    
  const uint16_t *buf = str.buffer();
  int firstbox = 0;
  
  for(int i = 0; i < start; ++i)
    if(buf[i] == PMATH_CHAR_BOX)
      ++firstbox;
      
  MathSequence *tmp = new MathSequence();
  tmp->insert(0, this, start, end);
  tmp->ensure_spans_valid();
  tmp->ensure_boxes_valid();
  
  Expr result = tmp->to_pmath(flags);
  
  for(int i = 0; i < tmp->boxes.length(); ++i) {
    Box *box          = boxes[firstbox + i];
    Box *tmp_box      = tmp->boxes[i];
    int box_index     = box->index();
    int tmp_box_index = tmp_box->index();
    
    abandon(box);
    tmp->abandon(tmp_box);
    
    adopt(tmp_box, box_index);
    tmp->adopt(box, tmp_box_index);
    
    boxes[firstbox + i] = tmp_box;
    tmp->boxes[i] = box;
  }
  
  delete tmp;
  return result;
}

Box *MathSequence::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(direction == Forward) {
    if(*index >= length()) {
      if(_parent) {
        *index = _index;
        return _parent->move_logical(Forward, true, index);
      }
      return this;
    }
    
    if(jumping || *index < 0 || str[*index] != PMATH_CHAR_BOX) {
      if(jumping) {
        while(*index + 1 < length() && !spans.is_token_end(*index))
          ++*index;
          
        ++*index;
      }
      else {
        if( is_utf16_high(str[*index]) &&
            is_utf16_low(str[*index + 1]))
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
    return boxes[b]->move_logical(Forward, true, index);
  }
  
  if(*index <= 0) {
    if(_parent) {
      *index = _index + 1;
      return _parent->move_logical(Backward, true, index);
    }
    return this;
  }
  
  if(jumping) {
    do {
      --*index;
    } while(*index > 0 && !spans.is_token_end(*index - 1));
    
    return this;
  }
  
  if(str[*index - 1] != PMATH_CHAR_BOX) {
    --*index;
    
    if( is_utf16_high(str[*index - 1]) &&
        is_utf16_low(str[*index]))
      --*index;
      
    return this;
  }
  
  ensure_boxes_valid();
  
  int b = 0;
  while(boxes[b]->index() != *index - 1)
    ++b;
  *index = boxes[b]->length() + 1;
  return boxes[b]->move_logical(Backward, true, index);
}

Box *MathSequence::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index
) {
  int line, dstline;
  float x = *index_rel_x;
  
  if(*index >= 0) {
    line = 0;
    while(line < lines.length() - 1
          && lines[line].end <= *index)
      ++line;
      
    if(*index > 0) {
      x += glyphs[*index - 1].right + indention_width(lines[line].indent);
      if(line > 0)
        x -= glyphs[lines[line-1].end - 1].right;
    }
    dstline = direction == Forward ? line + 1 : line - 1;
  }
  else if(direction == Forward) {
    line = -1;
    dstline = 0;
  }
  else {
    line = lines.length();
    dstline = line - 1;
  }
  
  if(dstline >= 0 && dstline < lines.length()) {
    int i = 0;
    float l = indention_width(lines[dstline].indent);
    if(dstline > 0) {
      i = lines[dstline - 1].end;
      l -= glyphs[lines[dstline-1].end - 1].right;
    }
    
    while(i < lines[dstline].end
          && glyphs[i].right + l < x)
      ++i;
      
    if(i < lines[dstline].end
        && str[i] != PMATH_CHAR_BOX) {
      if( (i == 0 && l +  glyphs[i].right                      / 2 <= x) ||
          (i >  0 && l + (glyphs[i].right + glyphs[i-1].right) / 2 <= x))
        ++i;
        
      if(is_utf16_high(str[i - 1]))
        --i;
      else if(is_utf16_low(str[i]))
        ++i;
    }
    
    if(i > 0
        && i < glyphs.length()
        && i == lines[dstline].end
        && (direction == Backward || str[i - 1] == '\n'))
      --i;
      
    *index_rel_x = x - indention_width(lines[dstline].indent);
    if(i == lines[dstline].end && dstline < lines.length() - 1) {
      *index_rel_x += indention_width(lines[dstline + 1].indent);
    }
    else if(i > 0) {
      *index_rel_x -= glyphs[i - 1].right;
      if(dstline > 0)
        *index_rel_x += glyphs[lines[dstline-1].end - 1].right;
    }
    
    if(i < glyphs.length()
        && str[i] == PMATH_CHAR_BOX
        && *index_rel_x > 0
        && x < glyphs[i].right + l) {
      ensure_boxes_valid();
      int b = 0;
      while(boxes[b]->index() < i)
        ++b;
      *index = -1;
      return boxes[b]->move_vertical(direction, index_rel_x, index);
    }
    
    *index = i;
    return this;
  }
  
  if(_parent) {
    *index_rel_x = x;
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index);
  }
  
  return this;
}

Box *MathSequence::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  
  if(lines.length() == 0){
    *start = *end = 0;
    return this;
  }
  
  int line = 0;
  while(line < lines.length() - 1 && y > lines[line].descent + 0.1 * em) {
    y -= lines[line].descent + line_spacing() + lines[line + 1].ascent;
    ++line;
  }
  
  if(line > 0)
    *start = lines[line-1].end;
  else
    *start = 0;
    
  const uint16_t *buf = str.buffer();
  
  x -= indention_width(lines[line].indent);
  if(line > 0 && lines[line - 1].end < glyphs.length())
    x += glyphs[lines[line - 1].end].x_offset;
    
  if(x < 0) {
    *was_inside_start = false;
    *end = *start;
//    if(is_placeholder(*start))
//      ++*end;
    return this;
  }
  
  float line_start = 0;
  if(*start > 0)
    line_start += glyphs[*start - 1].right;
    
  while(*start < lines[line].end) {
    if(x <= glyphs[*start].right - line_start) {
      float prev = 0;
      if(*start > 0)
        prev = glyphs[*start - 1].right;
        
      if(is_placeholder(*start)) {
        *was_inside_start = true;
        *end = *start + 1;
        return this;
      }
      
      if(buf[*start] == PMATH_CHAR_BOX) {
        ensure_boxes_valid();
        int b = 0;
        while(b < boxes.length() && boxes[b]->index() < *start)
          ++b;
          
        if(x > prev - line_start + boxes[b]->extents().width) {
          *was_inside_start = false;
          ++*start;
          *end = *start;
          return this;
        }
        
        if(x < prev - line_start + glyphs[*start].x_offset) {
          *was_inside_start = false;
          *end = *start;
          return this;
        }
        
        return boxes[b]->mouse_selection(
                 x - prev + line_start,
                 y,
                 start,
                 end,
                 was_inside_start);
      }
      
      if(line_start + x > (prev + glyphs[*start].right) / 2) {
        *was_inside_start = false;
        ++*start;
        *end = *start;
        return this;
      }
      
      *end = *start;
      return this;
    }
    
    ++*start;
  }
  
  if(*start > 0) {
    if(buf[*start - 1] == '\n' && (line == 0 || lines[line - 1].end != lines[line].end)) {
      --*start;
    }
    else if(buf[*start - 1] == ' '
            && *start < glyphs.length())
      --*start;
  }
  
  *end = *start;
//  if(is_placeholder(*start - 1)){
//    --*start;
//    *was_inside_start = false;
//  }
  return this;
}

void MathSequence::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(lines.length() == 0 || index > glyphs.length())
    return;
    
  float x = 0;
  float y = 0;
  
  int l = 0;
  while(l + 1 < lines.length() && lines[l].end <= index) {
    y += lines[l].descent + line_spacing() + lines[l+1].ascent;
    ++l;
  }
  
  x += indention_width(lines[l].indent);
  
  if(index < glyphs.length())
    x += glyphs[index].x_offset;
    
  if(index > 0) {
    x += glyphs[index - 1].right;
    
    if(l > 0 && lines[l - 1].end > 0) {
      x -= glyphs[lines[l - 1].end - 1].right;
      
      if(lines[l - 1].end < glyphs.length())
        x -= glyphs[lines[l - 1].end].x_offset;
    }
  }
  
  cairo_matrix_translate(matrix, x, y);
}

Box *MathSequence::normalize_selection(int *start, int *end) {
  if(is_utf16_high(str[*start - 1]))
    --*start;
    
  if(is_utf16_low(str[*end]))
    ++*end;
    
  return this;
}

bool MathSequence::is_inside_string(int pos) {
  const uint16_t *buf = str.buffer();
  int i = 0;
  while(i < pos) {
    if(buf[i] == '"') {
      Span span = spans[i];
      
      while(span.next()) {
        span = span.next();
      }
      
      if(span)
        i = span.end() + 1;
      else
        ++i;
    }
    else
      ++i;
  }
  
  return i > pos;
}

void MathSequence::ensure_boxes_valid() {
  if(!boxes_invalid)
    return;
    
  boxes_invalid = false;
  const uint16_t *buf = str.buffer();
  int len = str.length();
  int box = 0;
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_BOX)
      adopt(boxes[box++], i);
}

void MathSequence::ensure_spans_valid() {
  if(!spans_invalid)
    return;
    
  spans_invalid = false;
  
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = 0;
  
  pmath_string_t code = str.get_as_string();
  spans = pmath_spans_from_string(
            &code,
            0,
            subsuperscriptbox_at_index,
            underoverscriptbox_at_index,
            syntax_error,
            &data);
}

bool MathSequence::is_placeholder() {
  return str.length() == 1 && is_placeholder(0);
}

bool MathSequence::is_placeholder(int i) {
  if(i < 0 || i >= str.length())
    return false;
    
  if(str[i] == PMATH_CHAR_PLACEHOLDER || str[i] == CHAR_REPLACEMENT)
    return true;
    
  if(str[i] == PMATH_CHAR_BOX) {
    ensure_boxes_valid();
    int b = 0;
    
    while(boxes[b]->index() < i)
      ++b;
      
    return boxes[b]->get_own_style(Placeholder);
  }
  
  return false;
}

int MathSequence::matching_fence(int pos) {
  int len = str.length();
  if(pos < 0 || pos >= len)
    return -1;
    
  const uint16_t *buf = str.buffer();
  if(pmath_char_is_left(buf[pos])) {
    ensure_spans_valid();
    
    uint16_t ch = pmath_right_fence(buf[pos]);
    
    if(!ch)
      return -1;
      
    ++pos;
    while(pos < len && buf[pos] != ch) {
      if(spans[pos]) {
        pos = spans[pos].end() + 1;
      }
      else
        ++pos;
    }
    
    if(pos < len && buf[pos] == ch)
      return pos;
  }
  else if(pmath_char_is_right(buf[pos])) {
    ensure_spans_valid();
    
    int right = pos;
    do {
      --pos;
    } while(pos >= 0 && char_is_white(buf[pos]));
    
    if(pos >= 0 && pmath_right_fence(buf[pos]) == buf[right])
      return pos;
      
    for(; pos >= 0; --pos) {
      Span span = spans[pos];
      if(span && span.end() >= right) {
        while(span.next() && span.next().end() >= right)
          span = span.next();
          
        if(pmath_right_fence(buf[pos]) == buf[right])
          return pos;
          
        ++pos;
        while(pos < right
              && pmath_right_fence(buf[pos]) != buf[right]) {
          if(spans[pos])
            pos = spans[pos].end() + 1;
          else
            ++pos;
        }
        
        if(pos < right)
          return pos;
          
        return -1;
      }
    }
  }
  
  return -1;
}

pmath_bool_t MathSequence::subsuperscriptbox_at_index(int i, void *_data) {
  ScanData *data = (ScanData*)_data;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    if(data->sequence->boxes[data->current_box]->index() == i) {
      SubsuperscriptBox *b = dynamic_cast<SubsuperscriptBox*>(
                               data->sequence->boxes[data->current_box]);
      return 0 != b;
    }
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start) {
    if(data->sequence->boxes[data->current_box]->index() == i)
      return 0 != dynamic_cast<SubsuperscriptBox*>(
               data->sequence->boxes[data->current_box]);
    ++data->current_box;
  }
  
  return FALSE;
}

pmath_string_t MathSequence::underoverscriptbox_at_index(int i, void *_data) {
  ScanData *data = (ScanData*)_data;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    if(data->sequence->boxes[data->current_box]->index() == i) {
      UnderoverscriptBox *box = dynamic_cast<UnderoverscriptBox*>(
                                  data->sequence->boxes[data->current_box]);
                                  
      if(box)
        return pmath_ref(box->base()->text().get_as_string());
    }
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start) {
    if(data->sequence->boxes[data->current_box]->index() == i) {
      UnderoverscriptBox *box = dynamic_cast<UnderoverscriptBox*>(
                                  data->sequence->boxes[data->current_box]);
                                  
      if(box)
        return pmath_ref(box->base()->text().get_as_string());
    }
    ++data->current_box;
  }
  
  return PMATH_NULL;
}

void MathSequence::syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err) {
  ScanData *data = (ScanData*)_data;
  
  if(!data->sequence->get_style(ShowAutoStyles))
    return;
    
  const uint16_t *buf = pmath_string_buffer(&code);
  int             len = pmath_string_length(code);
  
  if(err) {
    if(pos < data->sequence->glyphs.length()
        && data->sequence->glyphs.length() > pos) {
      data->sequence->glyphs[pos].style = GlyphStyleSyntaxError;
    }
  }
  else if(pos < len && buf[pos] == '\n') { // new line character interpreted as multiplication
    while(pos > 0 && buf[pos] == '\n')
      --pos;
      
    if(pos >= 0
        && pos < data->sequence->glyphs.length()
        && data->sequence->glyphs.length() > pos) {
      data->sequence->glyphs[pos].missing_after = true;
    }
  }
}

pmath_t MathSequence::box_at_index(int i, void *_data) {
  ScanData *data = (ScanData*)_data;
  
  int flags = data->flags;
  if((flags & BoxFlagParseable) && data->sequence->is_inside_string(i)) {
    flags &= ~BoxFlagParseable;
  }
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()) {
    if(data->sequence->boxes[data->current_box]->index() == i)
      return data->sequence->boxes[data->current_box]->to_pmath(flags).release();
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start) {
    if(data->sequence->boxes[data->current_box]->index() == i)
      return data->sequence->boxes[data->current_box]->to_pmath(flags).release();
    ++data->current_box;
  }
  
  return PMATH_NULL;
}

void MathSequence::boxes_size(
  Context *context,
  int      start,
  int      end,
  float   *a,
  float   *d
) {
  int box = -1;
  const uint16_t *buf = str.buffer();
  for(int i = start; i < end; ++i) {
    if(buf[i] == PMATH_CHAR_BOX) {
      if(box < 0) {
        do {
          ++box;
        } while(boxes[box]->index() < i);
      }
      boxes[box++]->extents().bigger_y(a, d);
    }
    else if(glyphs[i].is_normal_text) {
      context->text_shaper->vertical_glyph_size(
        context,
        buf[i],
        glyphs[i],
        a,
        d);
    }
    else {
      context->math_shaper->vertical_glyph_size(
        context,
        buf[i],
        glyphs[i],
        a,
        d);
    }
  }
}

void MathSequence::box_size(
  Context *context,
  int      pos,
  int      box,
  float   *a,
  float   *d
) {
  if(pos >= 0 && pos < glyphs.length()) {
    const uint16_t *buf = str.buffer();
    if(buf[pos] == PMATH_CHAR_BOX) {
      if(box < 0)
        box = get_box(pos);
        
      boxes[box]->extents().bigger_y(a, d);
    }
    else if(glyphs[pos].is_normal_text)
      context->text_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        a,
        d);
    else
      context->math_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        a,
        d);
  }
}

void MathSequence::caret_size(
  Context *context,
  int      pos,
  int      box,
  float   *a,
  float   *d
) {
  if(glyphs.length() > 0) {
    box_size(context, pos - 1, box - 1, a, d);
    box_size(context, pos,     box,     a, d);
  }
  else {
    *a = _extents.ascent;
    *d = _extents.descent;
  }
}

void MathSequence::resize_span(
  Context *context,
  Span     span,
  int     *pos,
  int     *box
) {
  if(!span) {
    if(str[*pos] == PMATH_CHAR_BOX) {
      boxes[*box]->resize(context);
      
      glyphs[*pos].right = boxes[*box]->extents().width;
      glyphs[*pos].composed = 1;
      ++*box;
      ++*pos;
      return;
    }
    
    int next  = *pos;
    while(next < glyphs.length() && !spans.is_token_end(next))
      ++next;
      
    if(next < glyphs.length())
      ++next;
      
    const uint16_t *buf = str.buffer();
    
    if(context->math_spacing) {
      context->math_shaper->decode_token(
        context,
        next - *pos,
        buf + *pos,
        glyphs.items() + *pos);
    }
    else {
      context->text_shaper->decode_token(
        context,
        next - *pos,
        buf + *pos,
        glyphs.items() + *pos);
        
      for(int i = *pos; i < next; ++i) {
        if(glyphs[i].index) {
          glyphs[i].is_normal_text = 1;
        }
        else {
          context->math_shaper->decode_token(
            context,
            1,
            buf + i,
            glyphs.items() + i);
        }
      }
    }
    
    *pos = next;
    return;
  }
  
  if(!span.next() && str[*pos] == '"') {
    const uint16_t *buf = str.buffer();
    int end = span.end();
    if(!context->show_string_characters) {
      ++*pos;
      if(buf[end] == '"')
        --end;
    }
    else {
      context->math_shaper->decode_token(
        context,
        1,
        buf + *pos,
        glyphs.items() + *pos);
        
      if(buf[end] == '"')
        context->math_shaper->decode_token(
          context,
          1,
          buf + end,
          glyphs.items() + end);
    }
    
    bool old_math_styling = context->math_spacing;
    context->math_spacing = false;
    
    while(*pos <= end) {
      if(buf[*pos] == PMATH_CHAR_BOX) {
        boxes[*box]->resize(context);
        glyphs[*pos].right = boxes[*box]->extents().width;
        glyphs[*pos].composed = 1;
        ++*box;
        ++*pos;
      }
      else {
        int next = *pos;
        while(next <= end && !spans.is_token_end(next))
          ++next;
        ++next;
        
        if(!context->show_string_characters
            && buf[*pos] == '\\')
          ++*pos;
          
        context->text_shaper->decode_token(
          context,
          next - *pos,
          buf + *pos,
          glyphs.items() + *pos);
          
        for(int i = *pos; i < next; ++i) {
          glyphs[i].is_normal_text = 1;
        }
        
        *pos = next;
      }
    }
    
    context->math_spacing = old_math_styling;
    
    *pos = span.end() + 1;
  }
  else {
    resize_span(context, span.next(), pos, box);
    while(*pos <= span.end())
      resize_span(context, spans[*pos], pos, box);
  }
}

void MathSequence::stretch_span(
  Context *context,
  Span     span,
  int     *pos,
  int     *box,
  float   *core_ascent,
  float   *core_descent,
  float   *ascent,
  float   *descent
) {
  const uint16_t *buf = str.buffer();
  
  if(span) {
    int start = *pos;
    if(!span.next()) {
      uint16_t ch = buf[start];
      
      if(ch == '"') {
        for(; *pos <= span.end(); ++*pos) {
          if(buf[*pos] == PMATH_CHAR_BOX)
            ++*box;
        }
        
        return;
      }
      
      if(ch == PMATH_CHAR_BOX) {
        UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[*box]);
        if(underover && underover->base()->length() == 1)
          ch = underover->base()->str[0];
      }
      
      if(pmath_char_is_left(ch)) {
        float ca = 0;
        float cd = 0;
        float a = 0;
        float d = 0;
        
        ++*pos;
        while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
          stretch_span(context, spans[*pos], pos, box, &ca, &cd, &a, &d);
          
        if(*pos <= span.end() && pmath_char_is_right(buf[*pos])) {
          context->math_shaper->vertical_stretch_char(
            context, a, d, true, buf[*pos], &glyphs[*pos]);
            
          ++*pos;
        }
        
        context->math_shaper->vertical_stretch_char(
          context, a, d, true, buf[start], &glyphs[start]);
          
        if(*ascent < a)
          *ascent = a;
          
        if(*core_ascent <  a) *core_ascent =  a;
        else if(*core_ascent < ca) *core_ascent = ca;
        
        if(*descent < d)
          *descent = d;
          
        if(*core_descent <  d) *core_descent =  d;
        else if(*core_descent < cd) *core_descent = cd;
      }
      else if(pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)) {
        float a = 0;
        float d = 0;
        int startbox = *box;
        
        if(buf[start] == PMATH_CHAR_BOX) {
          assert(dynamic_cast<UnderoverscriptBox*>(boxes[startbox]) != 0);
          
          ++*pos;
          ++*box;
        }
        else {
          ++*pos;
          
          if(*pos < glyphs.length()
              && buf[*pos] == PMATH_CHAR_BOX) {
            SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[startbox]);
            
            if(subsup) {
              ++*box;
              ++*pos;
            }
          }
        }
        
        while(*pos <= span.end())
          stretch_span(context, spans[*pos], pos, box, &a, &d, ascent, descent);
          
        if(buf[start] == PMATH_CHAR_BOX) {
          UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[startbox]);
          
          assert(underover != 0);
          
          context->math_shaper->vertical_stretch_char(
            context,
            a,
            d,
            true,
            underover->base()->str[0],
            &underover->base()->glyphs[0]);
            
          context->math_shaper->vertical_glyph_size(
            context,
            underover->base()->str[0],
            underover->base()->glyphs[0],
            &underover->base()->_extents.ascent,
            &underover->base()->_extents.descent);
            
          underover->base()->_extents.width = underover->base()->glyphs[0].right;
          
          underover->after_items_resize(context);
          
          glyphs[start].right = underover->extents().width;
          
          underover->base()->extents().bigger_y(core_ascent, core_descent);
          underover->extents().bigger_y(ascent, descent);
        }
        else {
          context->math_shaper->vertical_stretch_char(
            context,
            a,
            d,
            true,
            buf[start],
            &glyphs[start]);
            
          BoxSize size;
          context->math_shaper->vertical_glyph_size(
            context,
            buf[start],
            glyphs[start],
            &size.ascent,
            &size.descent);
            
          size.bigger_y(core_ascent, core_descent);
          size.bigger_y(ascent,      descent);
          
          if(start + 1 < glyphs.length()
              && buf[start + 1] == PMATH_CHAR_BOX) {
            SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[startbox]);
            
            if(subsup) {
              subsup->stretch(context, size);
              subsup->extents().bigger_y(ascent, descent);
              
              subsup->adjust_x(context, buf[start], glyphs[start]);
            }
          }
        }
        
        if(*core_ascent < a)
          *core_ascent = a;
        if(*core_descent < d)
          *core_descent = d;
      }
      else
        stretch_span(context, span.next(), pos, box, core_ascent, core_descent, ascent, descent);
    }
    else
      stretch_span(context, span.next(), pos, box, core_ascent, core_descent, ascent, descent);
      
    if(*pos <= span.end()
        && buf[*pos] == '/'
        && spans.is_token_end(*pos)) {
      start = *pos;
      
      ++*pos;
      while(*pos <= span.end())
        stretch_span(context, spans[*pos], pos, box, core_ascent, core_descent, ascent, descent);
        
      context->math_shaper->vertical_stretch_char(
        context,
        *core_ascent  - 0.1 * em,
        *core_descent - 0.1 * em,
        true,
        buf[start],
        &glyphs[start]);
        
      BoxSize size;
      context->math_shaper->vertical_glyph_size(
        context,
        buf[start],
        glyphs[start],
        &size.ascent,
        &size.descent);
        
      size.bigger_y(core_ascent, core_descent);
      size.bigger_y(ascent,      descent);
    }
    
    while(*pos <= span.end()
          && (!pmath_char_is_left(buf[*pos]) || spans[*pos]))
      stretch_span(context, spans[*pos], pos, box, core_ascent, core_descent, ascent, descent);
      
    if(*pos < span.end()) {
      start = *pos;
      
      float ca = 0;
      float cd = 0;
      float a = 0;
      float d = 0;
      
      ++*pos;
      while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
        stretch_span(context, spans[*pos], pos, box, &ca, &cd, &a, &d);
        
      if(*pos <= span.end() && pmath_char_is_right(buf[*pos])) {
        context->math_shaper->vertical_stretch_char(
          context, a, d, false, buf[*pos], &glyphs[*pos]);
          
        ++*pos;
      }
      
      context->math_shaper->vertical_stretch_char(
        context, a, d, false, buf[start], &glyphs[start]);
        
      if(*ascent < a)
        *ascent = a;
        
      if(*core_ascent <  a) *core_ascent =  a;
      else if(*core_ascent < ca) *core_ascent = ca;
      
      if(*descent < d)
        *descent = d;
        
      if(*core_descent <  d) *core_descent =  d;
      else if(*core_descent < cd) *core_descent = cd;
      
      while(*pos <= span.end()) {
        stretch_span(
          context,
          spans[*pos],
          pos,
          box,
          core_ascent,
          core_descent,
          ascent, descent);
      }
    }
    
    return;
  }
  
  if(buf[*pos] == PMATH_CHAR_BOX) {
    SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[*box]);
    
    if(subsup && *pos > 0) {
      if(buf[*pos - 1] == PMATH_CHAR_BOX) {
        subsup->stretch(context, boxes[*box - 1]->extents());
      }
      else {
        BoxSize size;
        
        context->math_shaper->vertical_glyph_size(
          context, buf[*pos - 1], glyphs[*pos - 1],
          &size.ascent, &size.descent);
          
        subsup->stretch(context, size);
        subsup->adjust_x(context, buf[*pos - 1], glyphs[*pos - 1]);
      }
      
      subsup->extents().bigger_y(ascent,      descent);
      subsup->extents().bigger_y(core_ascent, core_descent);
    }
    else {
      UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[*box]);
      
      if(underover) {
        uint16_t ch = 0;
        
        if(underover->base()->length() == 1)
          ch = underover->base()->text()[0];
          
        if(spans.is_operand_start(*pos) &&
            (pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)))
        {
          context->math_shaper->vertical_stretch_char(
            context,
            0,
            0,
            true,
            underover->base()->str[0],
            &underover->base()->glyphs[0]);
            
          context->math_shaper->vertical_glyph_size(
            context,
            underover->base()->str[0],
            underover->base()->glyphs[0],
            &underover->base()->_extents.ascent,
            &underover->base()->_extents.descent);
            
          underover->base()->_extents.width = underover->base()->glyphs[0].right;
          
          underover->after_items_resize(context);
          
          glyphs[*pos].right = underover->extents().width;
        }
        
        underover->base()->extents().bigger_y(core_ascent, core_descent);
      }
      else
        boxes[*box]->extents().bigger_y(core_ascent, core_descent);
    }
    
    boxes[*box]->extents().bigger_y(ascent, descent);
    ++*box;
    ++*pos;
    return;
  }
  
  if(spans.is_operand_start(*pos)
      && length() > 1
      && (pmath_char_maybe_bigop(buf[*pos]) || pmath_char_is_integral(buf[*pos]))) {
    context->math_shaper->vertical_stretch_char(
      context,
      0,
      0,
      true,
      buf[*pos],
      &glyphs[*pos]);
      
    BoxSize size;
    context->math_shaper->vertical_glyph_size(
      context,
      buf[*pos],
      glyphs[*pos],
      &size.ascent,
      &size.descent);
      
    size.bigger_y(core_ascent, core_descent);
    size.bigger_y(ascent,      descent);
    
    ++*pos;
    return;
  }
  
  do {
    context->math_shaper->vertical_glyph_size(
      context, buf[*pos], glyphs[*pos], core_ascent, core_descent);
    ++*pos;
  } while(*pos < str.length() && !spans.is_token_end(*pos - 1));
  
  if(*ascent < *core_ascent)
    *ascent = *core_ascent;
  if(*descent < *core_descent)
    *descent = *core_descent;
}

void MathSequence::enlarge_space(Context *context) {
  if(context->script_indent > 0)
    return;
    
  int box = 0;
  bool in_string = false;
  bool in_alias = false;
  const uint16_t *buf = str.buffer();
  int i;
  bool last_was_factor = false;
  bool last_was_number = false;
  bool last_was_space  = false;
  bool last_was_left   = false;
  
  int e = -1;
  while(true) {
    i = e += 1;
    if(i >= glyphs.length())
      break;
      
    while(e < glyphs.length() && !spans.is_token_end(e))
      ++e;
      
    // italic correction
    if(glyphs[e].slant == FontSlantItalic
        && buf[e] != PMATH_CHAR_BOX
        && (e + 1 == glyphs.length() || glyphs[e+1].slant != FontSlantItalic)) {
      float ital_corr = context->math_shaper->italic_correction(
                          context,
                          buf[e],
                          glyphs[e]);
                          
      ital_corr *= em;
      if(e + 1 < glyphs.length()) {
        glyphs[e+1].x_offset += ital_corr;
        glyphs[e+1].right += ital_corr;
      }
      else
        glyphs[e].right += ital_corr;
    }
    
    while(e + 1 < glyphs.length() &&
          buf[e + 1] == PMATH_CHAR_BOX &&
          box < boxes.length())
    {
      while(box < boxes.length() && boxes[box]->index() <= e)
        ++box;
        
      if(box == boxes.length() || !dynamic_cast<SubsuperscriptBox*>(boxes[box]))
        break;
        
      ++e;
    }
    
    if(buf[i] == '\t') {
      static uint16_t arrow = 0x21e2;//0x27F6;
      float width = 2 * context->canvas->get_font_size();
      
      if(context->show_auto_styles) {
        context->math_shaper->decode_token(
          context,
          1,
          &arrow,
          &glyphs[i]);
          
        glyphs[i].x_offset = (width - glyphs[i].right) / 2;
        
        glyphs[i].style = GlyphStyleImplicit;
      }
      
      glyphs[i].right = width;
      continue;
    }
    
    if(buf[i] == '"') {
      in_string = !in_string;
      last_was_factor = false;
      continue;
    }
    
    if(buf[i] == PMATH_CHAR_ALIASDELIMITER) {
      in_alias = !in_alias;
      last_was_factor = false;
      continue;
    }
    
    if(in_string || in_alias || e >= glyphs.length())
      continue;
      
    const uint16_t *op = buf;
    int ii = i;
    int ee = e;
    if(op[ii] == PMATH_CHAR_BOX) {
      while(boxes[box]->index() < i)
        ++box;
        
      if(box < boxes.length()) {
        UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[box]);
        if(underover && underover->base()->length() > 0) {
          op = underover->base()->text().buffer();
          ii = 0;
          ee = ii;
          while(ee < underover->base()->length()
                && !underover->base()->span_array().is_token_end(ee))
            ++ee;
            
          if(ee != underover->base()->length() - 1) {
            op = buf;
            ii = i;
            ee = e;
          }
        }
      }
    }
    
    int prec;
    pmath_token_t tok = pmath_token_analyse(op + ii, ee - ii + 1, &prec);
    float space_left  = 0.0;
    float space_right = 0.0;
    
    bool lwf = false; // new last_was_factor
    bool lwl = false; // new last_was_left
    switch(tok) {
      case PMATH_TOK_PLUSPLUS: {
          if(spans.is_operand_start(i)) {
            prec = PMATH_PREC_CALL;
            goto PREFIX;
          }
          
          if(e + 1 < glyphs.length()
              && spans.is_operand_start(e + 1))
            goto INFIX;
            
          prec = PMATH_PREC_CALL;
        } goto POSTFIX;
        
      case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
      case PMATH_TOK_NARY_OR_PREFIX: {
          if(spans.is_operand_start(i)) {
            prec = pmath_token_prefix_precedence(op + ii, ee - ii + 1, prec);
            goto PREFIX;
          }
        } goto INFIX;
        
      case PMATH_TOK_BINARY_LEFT_AUTOARG:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_NARY:
      case PMATH_TOK_QUESTION: {
        INFIX:
          switch(prec) {
            case PMATH_PREC_SEQ:
            case PMATH_PREC_EVAL:
              space_right = em * 6 / 18;
              break;
              
            case PMATH_PREC_ASS:
            case PMATH_PREC_MODY:
              space_left  = em * 4 / 18;
              space_right = em * 8 / 18;
              break;
              
            case PMATH_PREC_LAZY:
            case PMATH_PREC_REPL:
            case PMATH_PREC_RULE:
            case PMATH_PREC_MAP:
            case PMATH_PREC_STR:
            case PMATH_PREC_COND:
            case PMATH_PREC_ARROW:
            case PMATH_PREC_REL:
              space_left = space_right = em * 5 / 18;
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
              space_left = space_right = em * 4 / 18;
              break;
              
            case PMATH_PREC_CIRCADD:
            case PMATH_PREC_CIRCMUL:
            case PMATH_PREC_MUL:
            case PMATH_PREC_DIV:
            case PMATH_PREC_MIDDOT:
            case PMATH_PREC_MUL2:
              space_left = space_right = em * 3 / 18;
              break;
              
            case PMATH_PREC_CROSS:
            case PMATH_PREC_POW:
            case PMATH_PREC_APL:
            case PMATH_PREC_TEST:
              space_left = space_right = em * 2 / 18;
              break;
              
            case PMATH_PREC_REPEAT:
            case PMATH_PREC_INC:
            case PMATH_PREC_CALL:
            case PMATH_PREC_DIFF:
            case PMATH_PREC_PRIM:
              break;
          }
        } break;
        
      case PMATH_TOK_POSTFIX_OR_PREFIX:
        if(!spans.is_operand_start(i))
          goto POSTFIX;
          
        prec = pmath_token_prefix_precedence(op + ii, ee - ii + 1, prec);
        goto PREFIX;
        
      case PMATH_TOK_PREFIX: {
        PREFIX:
          switch(prec) {
            case PMATH_PREC_REL: // not
              space_right = em * 4 / 18;
              break;
              
            case PMATH_PREC_ADD:
              space_right = em * 1 / 18;
              break;
              
            case PMATH_PREC_DIV:
              if(op[ii] == PMATH_CHAR_INTEGRAL_D) {
                space_left = em * 3 / 18;
              }
              break;
              
            default: break;
          }
        } break;
        
      case PMATH_TOK_POSTFIX: {
        POSTFIX:
          switch(prec) {
            case PMATH_PREC_FAC:
            case PMATH_PREC_FUNC:
              space_left = em * 2 / 18;
              break;
              
            default: break;
          }
        } break;
        
      case PMATH_TOK_COLON:
      case PMATH_TOK_ASSIGNTAG:
        space_left = space_right = em * 4 / 18;
        break;
        
      case PMATH_TOK_SPACE: {
          // implicit multiplication:
          if(buf[i] == ' '
              && e + 1 < glyphs.length()
              && last_was_factor && context->multiplication_sign) {
            pmath_token_t tok2 = pmath_token_analyse(buf + e + 1, 1, NULL);
            
            if(tok2 == PMATH_TOK_DIGIT
                || tok2 == PMATH_TOK_LEFT
                || tok2 == PMATH_TOK_LEFTCALL) {
              context->math_shaper->decode_token(
                context,
                1,
                &context->multiplication_sign,
                &glyphs[i]);
                
              space_left = space_right = em * 3 / 18;
              
              //if(context->show_auto_styles)
              glyphs[i].style = GlyphStyleImplicit;
            }
          }
          else {
            last_was_space = true;
            continue;
          }
        } break;
        
      case PMATH_TOK_DIGIT:
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
        lwf = true;
        /* no break */
      case PMATH_TOK_SLOT:
        if(last_was_factor) {
          space_left = em * 3 / 18;
        }
        break;
        
      case PMATH_TOK_LEFT:
        lwl = true;
        if(last_was_factor) {
          space_left = em * 3 / 18;
        }
        break;
        
      case PMATH_TOK_RIGHT:
        if(last_was_left) {
          space_left = em * 3 / 18;
        }
        lwf = true;
        break;
        
      case PMATH_TOK_PRETEXT:
        if(i + 1 == e && buf[i] == '<') {
          glyphs[e].x_offset -= em * 4 / 18;
          glyphs[e].right -=    em * 2 / 18;
        }
        break;
        
      case PMATH_TOK_LEFTCALL:
        lwl = true;
        break;
        
      case PMATH_TOK_NONE:
      case PMATH_TOK_CALL:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_INTEGRAL:
      case PMATH_TOK_COMMENTEND:
        break;
    }
    
    last_was_number = tok == PMATH_TOK_DIGIT;
    last_was_factor = lwf;
    last_was_left   = lwl;
    
    if(last_was_space) {
      last_was_space = false;
      space_left     = 0;
    }
    
    if(i > 0 || e + 1 < glyphs.length()) {
      glyphs[i].x_offset += space_left;
      glyphs[i].right +=    space_left;
      if(e + 1 < glyphs.length()) {
        glyphs[e+1].x_offset += space_right;
        glyphs[e+1].right +=    space_right;
      }
      else
        glyphs[e].right += space_right;
    }
  }
}

/* indention_array[i]: indention of the next line, if there is a line break
   before the i-th character.
 */
static Array<int> indention_array(0);

/* penalty_array[i]: A penalty value, which is used to decide whether a line
   break should be placed after (testme: before???) the i-th character.
   The higher this value is, the lower is the probability of a line break after
   character i.
 */
static Array<double> penalty_array(0);

static const double DepthPenalty = 1.0;
static const double WordPenalty = 100.0;//2.0;
static const double BestLineWidth = 0.95;
static const double LineWidthFactor = 2.0;

class BreakPositionWithPenalty {
  public:
    BreakPositionWithPenalty()
      : text_position(-1),
      prev_break_index(-1),
      penalty(Infinity)
    {
    }
    
    BreakPositionWithPenalty(int tpos, int prev, double pen)
      : text_position(tpos),
      prev_break_index(prev),
      penalty(pen)
    {}
    
  public:
    int text_position; // after that character is a break
    int prev_break_index;
    double penalty;
};

static Array<BreakPositionWithPenalty>  break_array(0);
static Array<int>                       break_result(0);

void MathSequence::split_lines(Context *context) {
  if(glyphs.length() == 0)
    return;
    
  const uint16_t *buf = str.buffer();
  
  if(glyphs[glyphs.length() - 1].right <= context->width) {
    bool have_newline = false;
    
    for(int i = 0; i < glyphs.length(); ++i)
      if(buf[i] == '\n') {
        have_newline = true;
        break;
      }
      
    if(!have_newline)
      return;
  }
  
  indention_array.length(glyphs.length() + 1);
  penalty_array.length(glyphs.length());
  
  indention_array.zeromem();
  penalty_array.zeromem();
  
  int pos = 0;
  while(pos < glyphs.length())
    pos = fill_indention_array(spans[pos], 0, pos);
    
  indention_array[glyphs.length()] = indention_array[glyphs.length() - 1];
  
  int box = 0;
  pos = 0;
  while(pos < glyphs.length())
    pos = fill_penalty_array(spans[pos], 0, pos, &box);
    
  if(buf[glyphs.length() - 1] != '\n')
    penalty_array[glyphs.length() - 1] = HUGE_VAL;
    
  _extents.width = context->width;
  for(int start_of_paragraph = 0; start_of_paragraph < glyphs.length();) {
    int end_of_paragraph = start_of_paragraph + 1;
    while(end_of_paragraph < glyphs.length()
          && buf[end_of_paragraph-1] != '\n')
      ++end_of_paragraph;
      
    break_array.length(0);
    break_array.add(BreakPositionWithPenalty(start_of_paragraph - 1, 0, 0.0));
    
    for(pos = start_of_paragraph; pos < end_of_paragraph; ++pos) {
      float xend = glyphs[pos].right;
      
      int current = break_array.length();
      break_array.add(BreakPositionWithPenalty(pos, -1, Infinity));
      
      for(int i = current - 1; i >= 0; --i) {
        float xstart    = 0;
        float indention = 0;
        double penalty  = break_array[i].penalty;
        if(break_array[i].text_position >= 0) {
          int tp = break_array[i].text_position;
          xstart    = glyphs[tp].right;
          indention = indention_width(indention_array[tp + 1]);
          penalty  += penalty_array[tp];
        }
        
        if(xend - xstart + indention > context->width
            && i + 1 < current)
          break;
          
        double best = context->width * BestLineWidth;
        if(pos + 1 < end_of_paragraph
            || best < xend - xstart + indention) {
          double factor = 0;
          if(context->width > 0)
            factor = LineWidthFactor / context->width;
          double rel_amplitude = ((xend - xstart + indention) - best) * factor;
          penalty += rel_amplitude * rel_amplitude;
        }
        
        if(!(penalty >= break_array[current].penalty)) {
          break_array[current].penalty = penalty;
          break_array[current].prev_break_index = i;
        }
      }
    }
    
    int mini = break_array.length() - 1;
    for(int i = mini - 1; i >= 0 && break_array[i].text_position + 1 == end_of_paragraph; --i) {
      if(break_array[i].penalty < break_array[mini].penalty)
        mini = i;
    }
    
    if(buf[end_of_paragraph - 1] != '\n')
      mini = break_array[mini].prev_break_index;
      
    break_result.length(0);
    for(int i = mini; i > 0; i = break_array[i].prev_break_index)
      break_result.add(i);
      
    for(int i = break_result.length() - 1; i >= 0; --i) {
      int j = break_result[i];
      pos = break_array[j].text_position;
      
      while(pos + 1 < end_of_paragraph && buf[pos+1] == ' ')
        ++pos;
        
      if(pos < end_of_paragraph) {
        new_line(
          pos + 1,
          indention_array[pos + 1],
          !spans.is_token_end(pos));
      }
    }
    
    start_of_paragraph = end_of_paragraph;
  }
  
  // Move FillBoxes to the beginning of the next line,
  // so aaaaaaaaaaaaaa.........bbbbb will become
  //    aaaaaaaaaaaaaa
  //    ............bbbbb
  // when the window is resized.
  box = 0;
  for(int line = 0; line < lines.length() - 1; ++line) {
    if(lines[line].end > 0 && buf[lines[line].end - 1] == PMATH_CHAR_BOX) {
      while(boxes[box]->index() < lines[line].end - 1)
        ++box;
        
      FillBox *fb = dynamic_cast<FillBox*>(boxes[box]);
      if(fb) {
        if(buf[lines[line].end] == PMATH_CHAR_BOX
            && dynamic_cast<FillBox*>(boxes[box+1]))
          continue;
          
        float w = glyphs[lines[line+1].end - 1].right - glyphs[lines[line].end - 1].right;
        
        if(fb->extents().width + w + indention_width(lines[line+1].indent) <= context->width) {
          lines[line].end--;
        }
      }
    }
  }
}

int MathSequence::fill_penalty_array(
  Span  span,
  int   depth,
  int   pos,
  int  *box
) {
  const uint16_t *buf = str.buffer();
  
  if(!span) {
    if(pos > 0) {
      if(buf[pos] == ','
          || buf[pos] == ';'
          || buf[pos] == ':'
          || buf[pos] == PMATH_CHAR_ASSIGN
          || buf[pos] == PMATH_CHAR_ASSIGNDELAYED) {
        penalty_array[pos-1] += DepthPenalty;
        //--depth;
      }
      
      if(buf[pos] == 0xA0   /* \[NonBreakingSpace] */
          || buf[pos] == 0x2011 /* non breaking hyphen */
          || buf[pos] == 0x2060 /* \[NonBreak] */) {
        penalty_array[pos-1] = Infinity;
        penalty_array[pos]   = Infinity;
        ++pos;
      }
    }
    
    if(buf[pos] == PMATH_CHAR_BOX && pos > 0) {
      ensure_boxes_valid();
      
      while(boxes[*box]->index() < pos)
        ++*box;
        
      if(dynamic_cast<SubsuperscriptBox*>(boxes[*box])) {
        penalty_array[pos-1] = Infinity;
        return pos + 1;
      }
      
      if(spans.is_operand_start(pos - 1)
          && dynamic_cast<GridBox*>(boxes[*box])) {
        if(pos > 0 && spans.is_operand_start(pos - 1)) {
          if(buf[pos-1] == PMATH_CHAR_PIECEWISE) {
            penalty_array[pos-1] = Infinity;
            return pos + 1;
          }
          
          pmath_token_t tok = pmath_token_analyse(buf + pos - 1, 1, NULL);
          
          if(tok == PMATH_TOK_LEFT
              || tok == PMATH_TOK_LEFTCALL)
            penalty_array[pos-1] = Infinity;
        }
        
        if(pos + 1 < glyphs.length()) {
          pmath_token_t tok = pmath_token_analyse(buf + pos + 1, 1, NULL);
          
          if(tok == PMATH_TOK_RIGHT)
            penalty_array[pos] = Infinity;
          return pos + 1;
        }
      }
    }
    
    if(!spans.is_operand_start(pos))
      depth++;
      
    if(buf[pos] == ' ') {
      penalty_array[pos] += depth * DepthPenalty;
      
      return pos + 1;
    }
    
    while(pos < spans.length() && !spans.is_token_end(pos)) {
      penalty_array[pos] += depth * DepthPenalty + WordPenalty;
      ++pos;
    }
    
    if(pos < spans.length()) {
      penalty_array[pos] += depth * DepthPenalty;
      ++pos;
    }
    
    return pos;
  }
  
  ++depth;
  
  int next = fill_penalty_array(span.next(), depth, pos, box);
  
  if(pmath_char_is_left(buf[pos])) {
    penalty_array[pos] += WordPenalty + DepthPenalty;
  }
  
  if(buf[pos] == '"' && !span.next()) {
    ++depth;
    
    penalty_array[pos] = Infinity;
    
    bool last_was_special = false;
    while(next < span.end()) {
      pmath_token_t tok = pmath_token_analyse(buf + next, 1, NULL);
      penalty_array[next] += depth * DepthPenalty + WordPenalty;
      
      switch(tok) {
        case PMATH_TOK_SPACE:
          penalty_array[next] -= WordPenalty;
          last_was_special = false;
          break;
          
        case PMATH_TOK_STRING:
          last_was_special = false;
          break;
          
        case PMATH_TOK_NAME:
        case PMATH_TOK_NAME2:
        case PMATH_TOK_DIGIT:
          if(last_was_special)
            penalty_array[next-1] -= WordPenalty;
          last_was_special = false;
          break;
          
        default:
          last_was_special = true;
          break;
      }
      
      ++next;
    }
    return next;
  }
  
  int func_depth = depth - 1;
  float inc_penalty = 0.0;
  float dec_penalty = 0.0;
  while(next <= span.end()) {
    switch(buf[next]) {
      case ';': dec_penalty = DepthPenalty; break;
      
      case PMATH_CHAR_ASSIGN:
      case PMATH_CHAR_ASSIGNDELAYED:
      case PMATH_CHAR_RULE:
      case PMATH_CHAR_RULEDELAYED:   inc_penalty = DepthPenalty; break;
      
      case ':': {
          if((next + 2 <= span.end() && buf[next + 1] == ':' && buf[next + 2] == '=')
              || (next + 1 <= span.end() && (buf[next + 1] == '>' || buf[next + 1] == '=')))
            inc_penalty = DepthPenalty;
        } break;
        
      case '-': {
          if(next + 1 <= span.end() && (buf[next + 1] == '>' || buf[next + 1] == '='))
            inc_penalty = DepthPenalty;
        } break;
        
      case '+': {
          if(next + 1 <= span.end() && buf[next + 1] == '=')
            inc_penalty = DepthPenalty;
        } break;
        
      default:
        if(pmath_char_is_left(buf[next])) {
          if(spans.is_operand_start(next))
            penalty_array[next] += WordPenalty;
          else
            penalty_array[next - 1] += WordPenalty;
            
          depth = func_depth;
        }
        else if(pmath_char_is_right(buf[next])) {
          penalty_array[next - 1] += WordPenalty;
        }
    }
    
    next = fill_penalty_array(spans[next], depth, next, box);
  }
  
  inc_penalty -= dec_penalty;
  if(inc_penalty != 0) {
    for(; pos < next; ++pos)
      penalty_array[pos] += inc_penalty;
  }
  
  return next;
}

int MathSequence::fill_indention_array(
  Span  span,
  int   depth,
  int   pos
) {
  const uint16_t *buf = str.buffer();
  
  if(!span) {
    indention_array[pos] = depth;
    
    ++pos;
    if(pos == spans.length() || spans.is_token_end(pos - 1))
      return pos;
      
    ++depth;
    do {
      indention_array[pos] = depth;
      ++pos;
    } while(pos < spans.length() && !spans.is_token_end(pos - 1));
    
    return pos;
  }
  
  bool in_string = buf[pos] == '\"' && !span.next();
  int next = fill_indention_array(span.next(), depth + 1, pos);
  
  indention_array[pos] = depth;
//  if(in_string)
//    depth+= 1;

  bool depth_dec = false;
  while(next <= span.end()) {
    if(in_string && next > 0 && buf[next-1] == '\n') {
      indention_array[next] = depth; // 0
      ++next;
    }
    else {
      if((buf[next] == ';' || buf[next] == ',') && !depth_dec && !in_string) {
        --depth;
        depth_dec = true;
      }
      
      next = fill_indention_array(spans[next], depth + 1, next);
    }
  }
  
  return next;
}

void MathSequence::new_line(int pos, unsigned int indent, bool continuation) {
  int len = lines.length();
  if(lines[len - 1].end < pos
      || pos == 0
      || (len >= 2 && lines[len - 2].end >= pos))
    return;
    
  lines.length(len + 1);
  lines[len].end = lines[len - 1].end;
  lines[len - 1].end = pos;
  lines[len - 1].continuation = continuation;
  
  lines[len].ascent = lines[len].descent = 0;
  lines[len].indent = indent;
  lines[len].continuation = 0;
  return;
}

void MathSequence::hstretch_lines(
  float width,
  float window_width,
  float *unfilled_width
) {
  *unfilled_width = -HUGE_VAL;
  
  if(width == HUGE_VAL) {
    if(window_width == HUGE_VAL)
      return;
      
    width = window_width;
  }
  
  const uint16_t *buf = str.buffer();
  
  int box = 0;
  int start = 0;
  
  float delta_x = 0;
  for(int line = 0; line < lines.length(); line++) {
    float total_fill_weight = 0;
    float white = 0;
    
    int oldbox = box;
    for(int pos = start; pos < lines[line].end; ++pos) {
      if(buf[pos] == PMATH_CHAR_BOX) {
        while(boxes[box]->index() < pos)
          ++box;
          
        FillBox *fillbox = dynamic_cast<FillBox*>(boxes[box]);
        if(fillbox && fillbox->weight > 0) {
          total_fill_weight += fillbox->weight;
          white += fillbox->extents().width;
        }
      }
      
      glyphs[pos].right += delta_x;
    }
    
    float line_width = indention_width(line);
    if(start > 0)
      line_width -= glyphs[start - 1].right;
    if(lines[line].end > 0)
      line_width += glyphs[lines[line].end - 1].right;
      
    if(total_fill_weight > 0) {
      if(width - line_width > 0) {
        float dx = 0;
        
        white += width - line_width;
        
        box = oldbox;
        for(int pos = start; pos < lines[line].end; ++pos) {
          if(buf[pos] == PMATH_CHAR_BOX) {
            while(boxes[box]->index() < pos)
              ++box;
              
            FillBox *fillbox = dynamic_cast<FillBox*>(boxes[box]);
            if(fillbox && fillbox->weight > 0) {
              BoxSize size = fillbox->extents();
              dx -= size.width;
              
              size.width = white * fillbox->weight / total_fill_weight;
              fillbox->expand(size);
              
              dx += size.width;
              
              if(lines[line].ascent < fillbox->extents().ascent)
                lines[line].ascent = fillbox->extents().ascent;
                
              if(lines[line].descent < fillbox->extents().descent)
                lines[line].descent = fillbox->extents().descent;
            }
          }
          
          glyphs[pos].right += dx;
        }
        
        delta_x += dx;
      }
    }
    
    line_width += indention_width(lines[line].indent);
    if(*unfilled_width < line_width)
      *unfilled_width = line_width;
      
    start = lines[line].end;
  }
}

//{ insert/remove ...

int MathSequence::insert(int pos, uint16_t chr) {
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, &chr, 1);
  invalidate();
  return pos + 1;
}

int MathSequence::insert(int pos, const uint16_t *ucs2, int len) {
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, ucs2, len);
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const char *latin1, int len) {
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, latin1, len);
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const String &s) {
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, s);
  invalidate();
  return pos + s.length();
}

int MathSequence::insert(int pos, Box *box) {
  if(pos > length())
    pos = length();
    
  if(MathSequence *sequence = dynamic_cast<MathSequence*>(box)) {
    pos = insert(pos, sequence, 0, sequence->length());
    delete sequence;
    return pos;
  }
  
  ensure_boxes_valid();
  
  spans_invalid = true;
  boxes_invalid = true;
  uint16_t ch = PMATH_CHAR_BOX;
  str.insert(pos, &ch, 1);
  adopt(box, pos);
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < pos)
    ++i;
  boxes.insert(i, 1, &box);
  invalidate();
  return pos + 1;
}

void MathSequence::remove(int start, int end) {
  ensure_boxes_valid();
  
  spans_invalid = true;
  
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < start)
    ++i;
    
  int j = i;
  while(j < boxes.length() && boxes[j]->index() < end)
    delete boxes[j++];
    
  boxes_invalid = i < boxes.length();
  boxes.remove(i, j - i);
  str.remove(start, end - start);
  invalidate();
}

Box *MathSequence::remove(int *index) {
  remove(*index, *index + 1);
  return this;
}

Box *MathSequence::extract_box(int boxindex) {
  Box *box = boxes[boxindex];
  
  DummyBox *dummy = new DummyBox();
  adopt(dummy, box->index());
  boxes.set(boxindex, dummy);
  
  abandon(box);
  return box;
}

////} ... insert/remove

template <class T>
static Box *create_or_error(Expr expr, int options) {
  T *box = Box::try_create<T>(expr, options);
  if(box)
    return box;
    
  return new ErrorBox(expr);
}

static Box *create_box(Expr expr, int options) {
  if(expr.is_string()) {
    InlineSequenceBox *box = new InlineSequenceBox;
    box->content()->load_from_object(expr, options);
    return box;
  }
  
  if(!expr.is_expr())
    return new ErrorBox(expr);
    
  if(expr[0] == PMATH_SYMBOL_LIST) {
    if(expr.expr_length() == 1) {
      expr = expr[1];
      return create_box(expr, options);
    }
    
    InlineSequenceBox *box = new InlineSequenceBox;
    box->content()->load_from_object(expr, options);
    return box;
  }
  
  if(expr[0] == PMATH_SYMBOL_BUTTONBOX) 
    return create_or_error<  ButtonBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_CHECKBOXBOX) 
    return create_or_error<  CheckboxBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_DYNAMICBOX) 
    return create_or_error<  DynamicBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_DYNAMICLOCALBOX) 
    return create_or_error<  DynamicLocalBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_FILLBOX) 
    return create_or_error<  FillBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_FRACTIONBOX) 
    return create_or_error<  FractionBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_FRAMEBOX) 
    return create_or_error<  FrameBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_GRAPHICSBOX)
    return create_or_error<  GraphicsBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_GRIDBOX) 
    return create_or_error<  GridBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_INPUTFIELDBOX)
    return create_or_error<  InputFieldBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_INTERPRETATIONBOX)
    return create_or_error<  InterpretationBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_PROGRESSINDICATORBOX)
    return create_or_error<  ProgressIndicatorBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_RADICALBOX)
    return create_or_error<  RadicalBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_RADIOBUTTONBOX)
    return create_or_error<  RadioButtonBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_ROTATIONBOX) 
    return create_or_error<  RotationBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_SETTERBOX) 
    return create_or_error<  SetterBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_SLIDERBOX) 
    return create_or_error<  SliderBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_SUBSCRIPTBOX)
    return create_or_error<  SubsuperscriptBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_SUPERSCRIPTBOX)
    return create_or_error<  SubsuperscriptBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_SUBSUPERSCRIPTBOX)
    return create_or_error<  SubsuperscriptBox>(expr, options);
  
  if( expr[0] == PMATH_SYMBOL_SQRTBOX)
    return create_or_error<  RadicalBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_STYLEBOX)
    return create_or_error<  StyleBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_TAGBOX)
    return create_or_error<  TagBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_TOOLTIPBOX)
    return create_or_error<  TooltipBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_TRANSFORMATIONBOX)
    return create_or_error<  TransformationBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_UNDERSCRIPTBOX)
    return create_or_error<  UnderoverscriptBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_OVERSCRIPTBOX)
    return create_or_error<  UnderoverscriptBox>(expr, options);
  
  if(expr[0] == PMATH_SYMBOL_UNDEROVERSCRIPTBOX)
    return create_or_error<  UnderoverscriptBox>(expr, options);
  
  if(expr[0] == GetSymbol( NumberBoxSymbol))
    return create_or_error<NumberBox>(expr, options);
  
  return new ErrorBox(expr);
}

class PositionedExpr {
  public:
    PositionedExpr()
      : pos(0)
    {
    }
    
    PositionedExpr(Expr _expr, int _pos)
      : expr(_expr), pos(_pos)
    {
    }
    
  public:
    Expr expr;
    int  pos;
};

static void defered_make_box(int pos, pmath_t obj, void *data) {
  Array<PositionedExpr> *boxes = (Array<PositionedExpr>*)data;
  
  boxes->add(PositionedExpr(Expr(obj), pos));
}

class SpanSynchronizer: public Base {
  public:
    SpanSynchronizer(
      int                    _new_load_options,
      Array<Box*>           &_old_boxes,
      SpanArray             &_old_spans,
      Array<PositionedExpr> &_new_boxes,
      SpanArray             &_new_spans
    ) : Base(),
      old_boxes(       _old_boxes),
      old_spans(       _old_spans),
      old_pos(         0),
      old_next_box(    0),
      new_load_options(_new_load_options),
      new_boxes(       _new_boxes),
      new_spans(       _new_spans),
      new_pos(         0),
      new_next_box(    0)
    {
    }
    
    bool is_in_range() {
      if(old_pos >= old_spans.length())
        return false;
        
      if(old_next_box >= old_boxes.length())
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
        assert(old_next_box == old_boxes.length());
      }
      
      if(new_pos == new_spans.length()) {
        assert(new_next_box == new_boxes.length());
      }
      
      int rem = 0;
      while(old_next_box + rem < old_boxes.length() &&
            old_boxes[old_next_box + rem]->index() < old_spans.length())
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          delete old_boxes[old_next_box + i];
          
        old_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        assert(new_box.pos < new_spans.length());
        
        Box *box = create_box(new_box.expr, new_load_options);
        old_boxes.insert(old_next_box, 1, &box);
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  protected:
    void next(Span old_span, Span new_span) {
      if(!is_in_range()){
        old_pos = old_spans.length();
        new_pos = new_spans.length();
        return;
      }
        
      if(old_span && new_span) {
        int old_start = old_pos;
        int new_start = new_pos;
        
        next(old_span.next(), new_span.next());
        
        assert(old_start < old_pos);
        assert(new_start < new_pos);
        
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
      
      while(old_next_box < old_boxes.length() &&
            new_next_box < new_boxes.length())
      {
        Box *box = old_boxes[old_next_box];
        
        if(box->index() >= old_pos)
          break;
          
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        if(!box->try_load_from_object(new_box.expr, new_load_options)) {
          box = create_box(new_box.expr, new_load_options);
          
          old_boxes.set(old_next_box, box);
        }
        
        ++old_next_box;
        ++new_next_box;
      }
      
      int rem = 0;
      while(old_next_box + rem < old_boxes.length() &&
            old_boxes[old_next_box + rem]->index() < old_pos)
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          delete old_boxes[old_next_box + i];
          
        old_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        Box *box = create_box(new_box.expr, new_load_options);
        old_boxes.insert(old_next_box, 1, &box);
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  public:
    Array<Box*>     &old_boxes;
    const SpanArray &old_spans;
    int              old_pos;
    int              old_next_box;
    
    int                         new_load_options;
    const Array<PositionedExpr> &new_boxes;
    const SpanArray             &new_spans;
    int                          new_pos;
    int                          new_next_box;
};

void MathSequence::load_from_object(Expr object, int options) {
  ensure_boxes_valid();
  
  Array<PositionedExpr> new_boxes;
  pmath_string_t        new_string;
  SpanArray             new_spans;
  
  Expr obj = object;
  
  if(obj[0] == PMATH_SYMBOL_BOXDATA && obj.expr_length() == 1)
    obj = obj[1];
    
  if(options & BoxOptionFormatNumbers)
    obj = NumberBox::prepare_boxes(obj);
    
  new_spans = pmath_spans_from_boxes(
                pmath_ref(obj.get()),
                &new_string,
                defered_make_box,
                &new_boxes);
                
  SpanSynchronizer syncer(options, boxes, spans, new_boxes, new_spans);
  
  while(syncer.is_in_range())
    syncer.next();
  syncer.finish();
  
  spans         = new_spans.extract_array();
  str           = String(new_string);
  boxes_invalid = true;
}

bool MathSequence::stretch_horizontal(Context *context, float width) {
  if(glyphs.length() != 1 || str[0] == PMATH_CHAR_BOX)
    return false;
    
  if(context->math_shaper->horizontal_stretch_char(
        context,
        width,
        str[0],
        &glyphs[0]))
  {
    _extents.width = glyphs[0].right;
    _extents.ascent  = _extents.descent = -1e9;
    context->math_shaper->vertical_glyph_size(
      context, str[0], glyphs[0], &_extents.ascent, &_extents.descent);
    lines[0].ascent  = _extents.ascent;
    lines[0].descent = _extents.descent;
    return true;
  }
  return false;
}

int MathSequence::get_line(int index, int guide) {
  if(guide >= lines.length())
    guide = lines.length() - 1;
  if(guide < 0)
    guide = 0;
    
  int line = guide;
  
  if(line < lines.length() && lines[line].end > index) {
    while(line > 0) {
      if(lines[line-1].end <= index)
        return line;
        
      --line;
    }
    
    return 0;
  }
  
  while(line < lines.length()) {
    if(lines[line].end > index)
      return line;
      
    ++line;
  }
  
  return lines.length() > 0 ? lines.length() - 1 : 0;
}

void MathSequence::get_line_heights(int line, float *ascent, float *descent) {
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

int MathSequence::get_box(int index, int guide) {
  assert(str[index] == PMATH_CHAR_BOX);
  
  ensure_boxes_valid();
  if(guide < 0)
    guide = 0;
    
  for(int box = guide; box < boxes.length(); ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  
  if(guide >= boxes.length())
    guide = boxes.length();
    
  for(int box = 0; box < guide; ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  assert(0 && "no box found at index.");
  return -1;
}

float MathSequence::indention_width(int i) {
  float f = i * em / 2;
  
  if(f <= _extents.width / 2)
    return f;
    
  return floor(_extents.width / em) * em / 2;
}

//} ... class MathSequence

