#include <boxes/mathsequence.h>

#include <climits>
#include <cmath>

#include <boxes/buttonbox.h>
#include <boxes/dynamicbox.h>
#include <boxes/errorbox.h>
#include <boxes/fillbox.h>
#include <boxes/fractionbox.h>
#include <boxes/framebox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/interpretationbox.h>
#include <boxes/numberbox.h>
#include <boxes/ownerbox.h>
#include <boxes/radicalbox.h>
#include <boxes/section.h>
#include <boxes/setterbox.h>
#include <boxes/sliderbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>

#include <eval/binding.h>
#include <eval/client.h>

#include <graphics/context.h>

#include <util/spanexpr.h>

using namespace richmath;

static const float ref_error_indicator_height = 1 / 3.0f;

static inline bool char_is_white(uint16_t ch){
  return ch == ' ' || ch == '\n';
}

class ScanData{
  public:
    MathSequence *sequence;
    int current_box;
    bool parseable;
};

//{ class MathSequence ...

MathSequence::MathSequence()
: AbstractSequence(),
  str(""),
  boxes_invalid(false),
  spans_invalid(false)
{
}

MathSequence::~MathSequence(){
  for(int i = 0;i < boxes.length();++i)
    delete boxes[i];
}

Box *MathSequence::item(int i){ 
  ensure_boxes_valid();
  return boxes[i]; 
}

bool MathSequence::expand(const BoxSize &size){
  if(boxes.length() == 1 && glyphs.length() == 1 && str.length() == 1){
    if(boxes[0]->expand(size)){
      _extents = boxes[0]->extents();
      glyphs[0].right = _extents.width;
      lines[0].ascent  = _extents.ascent;
      lines[0].descent = _extents.descent;
      return true;
    }
  }
  else{
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

void MathSequence::resize(Context *context){
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
  
  if(context->show_auto_styles){
    pos = 0;
    while(pos < glyphs.length())
      syntax_restyle_span(context, spans[pos], &pos);
      
    pos = 0;
    while(pos < glyphs.length())
      check_argcount_span(context, spans[pos], &pos);
  }
  
  if(context->math_spacing){
    float ca = 0;
    float cd = 0;
    float a = 0;
    float d = 0;
    
    if(glyphs.length() == 1
    && !dynamic_cast<UnderoverscriptBox*>(_parent)){
      pmath_token_t tok = pmath_token_analyse(str.buffer(), 1, NULL);
      
      if(tok == PMATH_TOK_INTEGRAL || tok == PMATH_TOK_PREFIX){
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
      else{
        box = 0;
        pos = 0;
        while(pos < glyphs.length())
          stretch_span(context, spans[pos], &pos, &box, &ca, &cd, &a, &d);
      }
    }
    else{
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
    for(pos = 0;pos < glyphs.length();++pos)
      if(buf[pos] == '\n')
        glyphs[pos].right = _extents.width;
      else
        glyphs[pos].right = _extents.width+= glyphs[pos].right;
  }
  
  lines.length(1);
  lines[0].end = glyphs.length();
  lines[0].ascent = lines[0].descent = 0;
  lines[0].indent = 0;
  lines[0].continuation = 0;
  
  context->section_content_window_width = old_scww;
  
  split_lines(context);
  if(dynamic_cast<Section*>(_parent)){
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
  if(lines.length() > 1){
    lines[0].ascent  = 0.75f * em;
    lines[0].descent = 0.25f * em;
  }
  while(pos < glyphs.length()){
    if(pos == lines[line].end){
      if(_extents.width < glyphs[pos - 1].right - x)
        _extents.width = glyphs[pos - 1].right - x;
      x = glyphs[pos - 1].right;
      
      _extents.descent+= lines[line].ascent + lines[line].descent + line_spacing();
        
      ++line;
      lines[line].ascent  = 0.75f * em;
      lines[line].descent = 0.25f * em;
    }
      
    if(buf[pos] == PMATH_CHAR_BOX){
      boxes[box]->extents().bigger_y(&lines[line].ascent, &lines[line].descent);
      ++box;
    }
    else{
      context->math_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        &lines[line].ascent, 
        &lines[line].descent);
    }
    
    ++pos;
  }
  
  if(pos > 0 && _extents.width < glyphs[pos - 1].right - x)
    _extents.width = glyphs[pos - 1].right - x;
    
  if(line + 1 < lines.length()){
    _extents.descent+= lines[line].ascent + lines[line].descent;
    ++line;
    lines[line].ascent = 0.75f * em;
    lines[line].descent = 0.25f * em;
  }
  _extents.ascent = lines[0].ascent;
  _extents.descent+= lines[line].ascent + lines[line].descent - lines[0].ascent;
  
  if(_extents.width < 0.75 && lines.length() > 1){
    _extents.width = 0.75;
  }
  
  if(context->sequence_unfilled_width == -HUGE_VAL)
    context->sequence_unfilled_width = _extents.width;
}

void MathSequence::colorize_scope(SyntaxState *state){
  assert(glyphs.length() == spans.length());
  assert(glyphs.length() == str.length());
  
  int pos = 0;
  while(pos < glyphs.length()){
    SpanExpr *se = new SpanExpr(pos, spans[pos], this);
    
    scope_colorize_spanexpr(state, se);
    
    pos = se->end() + 1;
    delete se;
  }
}

void MathSequence::clear_coloring(){
  for(int i = 0;i < glyphs.length();++i)
    glyphs[i].style = GlyphStyleNone;
  
  Box::clear_coloring();
}

void MathSequence::paint(Context *context){
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
  while(line < lines.length()){
    float h = lines[line].ascent + lines[line].descent;
    if(y + h >= clip_y1)
      break;
      
    y+= h + line_spacing();
    ++line;
  }
  
  if(line < lines.length()){
    float glyph_left = 0;
    int box = 0;
    int pos = 0;
    
    if(line > 0)
      pos = lines[line-1].end;
    
    if(pos > 0)
      glyph_left = glyphs[pos-1].right;
    
    bool have_style = false;
    bool have_slant = false;
    for(;line < lines.length() && y < clip_y2;++line){
      float x_extra = x0 + indention_width(lines[line].indent);
      
      if(pos > 0)
        x_extra-= glyphs[pos - 1].right;
          
      if(pos < glyphs.length())
        x_extra-= glyphs[pos].x_offset;
        
      y+= lines[line].ascent;
      
      for(;pos < lines[line].end;++pos){
        if(buf[pos] <= '\n')
          continue;
        
        if(have_style || glyphs[pos].style){
          int color = context->syntax->glyph_style_colors[glyphs[pos].style];
          
          context->canvas->set_color(color);
          have_style = color != default_color;
        }
        
        if(have_slant || glyphs[pos].slant){
          if(glyphs[pos].slant == FontSlantItalic){
            context->math_shaper = default_math_shaper->set_style(
              default_math_shaper->get_style() + Italic);
            have_slant = true;
          }
          else if(glyphs[pos].slant == FontSlantPlain){
            context->math_shaper = default_math_shaper->set_style(
              default_math_shaper->get_style() - Italic);
            have_slant = true;
          }
          else{
            context->math_shaper = default_math_shaper;
            have_slant = false;
          }
        }
        
        if(buf[pos] == PMATH_CHAR_BOX){
          while(boxes[box]->index() < pos)
            ++box;
          
          context->canvas->move_to(glyph_left + x_extra + glyphs[pos].x_offset, y);
          
          boxes[box]->paint(context);
          
          context->syntax->glyph_style_colors[GlyphStyleNone] = default_color;
          ++box;
        }
        else if(glyphs[pos].index 
        || glyphs[pos].composed 
        || glyphs[pos].horizontal_stretch){
          if(glyphs[pos].is_normal_text){
            context->text_shaper->show_glyph(
              context,
              glyph_left + x_extra, 
              y,
              buf[pos],
              glyphs[pos]);
          }
          else{
            context->math_shaper->show_glyph(
              context,
              glyph_left + x_extra, 
              y,
              buf[pos],
              glyphs[pos]);
          }
        }
        
        if(glyphs[pos].missing_after){
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
      
      if(lines[line].continuation){
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
      
      y+= lines[line].descent + line_spacing();
    }
    
  }
  
  if(context->selection.get() == this && !context->canvas->show_only_text){
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

void MathSequence::selection_path(Canvas *canvas, int start, int end){
  selection_path(0, canvas, start, end);
}

void MathSequence::selection_path(Context *opt_context, Canvas *canvas, int start, int end){
  float x0, y0, x1, y1, x2, y2;
  
  canvas->current_pos(&x0, &y0);
  
  y0-= lines[0].ascent;
  y1 = y0;
  
  int startline = 0;
  while(startline < lines.length()
  && start >= lines[startline].end){
    y1+= lines[startline].ascent + lines[startline].descent + line_spacing();
    ++startline;
  }
  
  if(startline == lines.length()){
    --startline;
    y1-= lines[startline].ascent + lines[startline].descent + line_spacing();
  }
  
  y2 = y1;
  int endline = startline;
  while(endline < lines.length()
  && end >= lines[endline].end){
    y2+= lines[endline].ascent + lines[endline].descent + line_spacing();
    ++endline;
  }
  
  if(endline == lines.length()){
    --endline;
    y2-= lines[endline].ascent + lines[endline].descent + line_spacing();
  }
  
  x1 = x0;
  if(start > 0)
    x1+= glyphs[start - 1].right;
  
  if(startline > 0){
    x1-= glyphs[lines[startline - 1].end - 1].right;
    
    if(start > lines[startline - 1].end){
      x1-= glyphs[lines[startline - 1].end].x_offset;
      
      if(start < glyphs.length())
        x1+= glyphs[start].x_offset / 2;
    }
  }
  else if(start < glyphs.length())
    x1+= glyphs[start].x_offset / 2;
  
  x1+= indention_width(lines[startline].indent);
  
  
  x2 = x0;
  if(end > 0)
    x2+= glyphs[end - 1].right;
  
  if(endline > 0){
    x2-= glyphs[lines[endline - 1].end - 1].right;
    
    if(end > lines[endline - 1].end){
      x2-= glyphs[lines[endline - 1].end].x_offset;
      
      if(end < glyphs.length())
        x2+= glyphs[end].x_offset / 2;
    }
  }
  else if(end < glyphs.length())
    x2+= glyphs[end].x_offset / 2;
  
  x2+= indention_width(lines[endline].indent);
  
  
  if(endline == startline){
    float a = 0.5 * em;
    float d = 0;
    
    if(opt_context){
      if(start == end){
        const uint16_t *buf = str.buffer();
        int box = 0;
        
        for(int i = 0;i < start;++i)
          if(buf[i] == PMATH_CHAR_BOX)
            ++box;
          
        caret_size(opt_context, start, box, &a, &d);
      }
      else{
        boxes_size(
          opt_context, 
          start,
          end,
          &a, &d);
      }
    }
    else{
      a = lines[startline].ascent;
      d = lines[startline].descent;
    }
    
    y1+= lines[startline].ascent;
    y2 = y1 + d + 1;
    y1-= a + 1;
    
    if(start == end){
      canvas->align_point(&x1, &y1, true);
      canvas->align_point(&x2, &y2, true);
      
      canvas->move_to(x1, y1);
      canvas->line_to(x2, y2);
    }
    else
      canvas->pixrect(x1, y1, x2, y2, false);
  }
  else{
    y2 = y1;
    for(int line = startline;line <= endline;++line)
      y2+= lines[line].ascent + lines[line].descent + line_spacing();
    y2-= line_spacing();
    
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

Expr MathSequence::to_pmath(bool parseable){
  ScanData data;
  data.sequence = this;
  data.current_box = 0;
  data.parseable = parseable;
  
  ensure_spans_valid();
  
  return Expr(pmath_boxes_from_spans(
    spans.array(),
    str.get(),
    parseable,
    box_at_index,
    &data));
}

Expr MathSequence::to_pmath(bool parseable, int start, int end){
  if(start == 0 && end >= length())
    return to_pmath(parseable);
    
  const uint16_t *buf = str.buffer();
  int firstbox = 0;
  for(int i = 0;i < start;++i)
    if(buf[i] == PMATH_CHAR_BOX)
      ++firstbox;
  
  MathSequence *tmp = new MathSequence();
  tmp->insert(0, this, start, end);
  tmp->ensure_spans_valid();
  tmp->ensure_boxes_valid();
  
  Expr result = tmp->to_pmath(parseable);
  
  for(int i = 0;i < tmp->boxes.length();++i){
    Box *box     = boxes[firstbox + i];
    Box *tmp_box = tmp->boxes[i];
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
){
  if(direction == Forward){
    if(*index >= length()){
      if(_parent){
        *index = _index;
        return _parent->move_logical(Forward, true, index);
      }
      return this;
    }
    
    if(jumping || *index < 0 || str[*index] != PMATH_CHAR_BOX){
      if(jumping){
        while(*index + 1 < length() && !spans.is_token_end(*index))
          ++*index;
        
        ++*index;
      }
      else{
        if(is_utf16_high(str[*index])
        && is_utf16_low( str[*index + 1]))
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
  
  if(*index <= 0){
    if(_parent){
      *index = _index + 1;
      return _parent->move_logical(Backward, true, index);
    }
    return this;
  }
  
  if(jumping){
    do{
      --*index;
    }while(*index > 0 && !spans.is_token_end(*index - 1));
    
    return this;
  }
  
  if(str[*index - 1] != PMATH_CHAR_BOX){
    --*index;
    
    if(is_utf16_high(str[*index - 1])
    && is_utf16_low( str[*index]))
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
){
  int line, dstline;
  float x = *index_rel_x;
  
  if(*index >= 0){
    line = 0;
    while(line < lines.length() - 1
    && lines[line].end <= *index)
      ++line;
    
    if(*index > 0){
      x+= glyphs[*index - 1].right + indention_width(lines[line].indent);
      if(line > 0)
        x-= glyphs[lines[line-1].end-1].right;
    }
    dstline = direction == Forward ? line + 1 : line - 1;
  }
  else if(direction == Forward){
    line = -1;
    dstline = 0;
  }
  else{
    line = lines.length();
    dstline = line - 1;
  }
  
  if(dstline >= 0 && dstline < lines.length()){
    int i = 0;
    float l = indention_width(lines[dstline].indent);
    if(dstline > 0){
      i = lines[dstline - 1].end;
      l-= glyphs[lines[dstline-1].end-1].right;
    }
    
    while(i < lines[dstline].end
    && glyphs[i].right + l < x)
      ++i;
    
    if(i < lines[dstline].end
    && str[i] != PMATH_CHAR_BOX){
      if((i == 0 && l +  glyphs[i].right                      / 2 <= x)
      || (i >  0 && l + (glyphs[i].right + glyphs[i-1].right) / 2 <= x))
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
    if(i == lines[dstline].end && dstline < lines.length() - 1){
      *index_rel_x+= indention_width(lines[dstline + 1].indent);
    }
    else if(i > 0){
      *index_rel_x-= glyphs[i - 1].right;
      if(dstline > 0)
        *index_rel_x+= glyphs[lines[dstline-1].end - 1].right;
    }
    
    if(i < glyphs.length()
    && str[i] == PMATH_CHAR_BOX
    && *index_rel_x > 0
    && x < glyphs[i].right + l){
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
  
  if(_parent){
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
){
  *was_inside_start = true;
  int line = 0;
  while(line < lines.length() - 1
  && y > lines[line].descent + 0.1 * em){
    y-= lines[line].descent + line_spacing() + lines[line + 1].ascent;
    ++line;
  }
  
  if(line > 0)
    *start = lines[line-1].end;
  else
    *start = 0;
    
  const uint16_t *buf = str.buffer();
  
  x-= indention_width(lines[line].indent);
  if(line > 0 && lines[line - 1].end < glyphs.length())
    x+= glyphs[lines[line - 1].end].x_offset;
  
  if(x < 0){
    *was_inside_start = false;
    *end = *start;
//    if(is_placeholder(*start))
//      ++*end;
    return this;
  }
  
  float line_start = 0;
  if(*start > 0)
    line_start+= glyphs[*start - 1].right;
  
  while(*start < lines[line].end){
    if(x <= glyphs[*start].right - line_start){
      float prev = 0;
      if(*start > 0)
        prev = glyphs[*start - 1].right;
        
      if(is_placeholder(*start)){
        *was_inside_start = true;
        *end = *start + 1;
        return this;
      }
      
      if(buf[*start] == PMATH_CHAR_BOX){
        ensure_boxes_valid();
        int b = 0;
        while(b < boxes.length() && boxes[b]->index() < *start)
          ++b;
        
        if(x > prev - line_start + boxes[b]->extents().width){
          *was_inside_start = false;
          ++*start;
          *end = *start;
          return this;
        }
        
        if(x < prev - line_start + glyphs[*start].x_offset){
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
      
      if(line_start + x > (prev + glyphs[*start].right) / 2){
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
  
  if(*start > 0){
    if(buf[*start - 1] == '\n' && (line == 0 || lines[line - 1].end != lines[line].end)){
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
){
  float x = 0;
  float y = 0;
  
  int l = 0;
  while(l + 1 < lines.length() && lines[l].end <= index){
    y+= lines[l].descent + line_spacing() + lines[l+1].ascent;
    ++l;
  }
  
  x+= indention_width(lines[l].indent);
  if(index < glyphs.length())
    x+= glyphs[index].x_offset;
    
  if(index > 0){
    x+= glyphs[index - 1].right;
    
    if(l > 0 && lines[l - 1].end > 0)
      x-= glyphs[lines[l - 1].end - 1].right;
  }
  
  cairo_matrix_translate(matrix, x, y);
}

Box *MathSequence::normalize_selection(int *start, int *end){
  if(is_utf16_high(str[*start - 1]))
    --*start;
  
  if(is_utf16_low(str[*end]))
    ++*end;
    
  return this;
}

bool MathSequence::is_inside_string(int pos){
  const uint16_t *buf = str.buffer();
  int i = 0;
  while(i < pos){
    if(buf[i] == '"'){
      Span span = spans[i];
      
      while(span.next()){
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

void MathSequence::ensure_boxes_valid(){
  if(!boxes_invalid)
    return;
  
  boxes_invalid = false;
  const uint16_t *buf = str.buffer();
  int len = str.length();
  int box = 0;
  for(int i = 0;i < len;++i)
    if(buf[i] == PMATH_CHAR_BOX)
      adopt(boxes[box++], i);
}

void MathSequence::ensure_spans_valid(){
  if(!spans_invalid)
    return;
  
  spans_invalid = false;
  
  ScanData data;
  data.sequence = this;
  data.current_box = 0;
  data.parseable = false;
  
  pmath_string_t code = str.get_as_string();
  spans = pmath_spans_from_string(
    &code, 
    0,
    subsuperscriptbox_at_index,
    underoverscriptbox_at_index,
    syntax_error,
    &data);
}

bool MathSequence::is_placeholder(){
  return str.length() == 1 && is_placeholder(0);
}

bool MathSequence::is_placeholder(int i){
  if(i < 0 || i >= str.length())
    return false;
  
  if(str[i] == PMATH_CHAR_PLACEHOLDER
  || str[i] == CHAR_REPLACEMENT)
    return true;
  
  if(str[i] == PMATH_CHAR_BOX){
    ensure_boxes_valid();
    int b = 0;
    
    while(boxes[b]->index() < i)
      ++b;
    
    return boxes[b]->get_own_style(Placeholder);
  }
  
  return false;
}

int MathSequence::matching_fence(int pos){
  int len = str.length();
  if(pos < 0 || pos >= len)
    return -1;
  
  const uint16_t *buf = str.buffer();
  if(pmath_char_is_left(buf[pos])){
    ensure_spans_valid();
    
    uint16_t ch = pmath_right_fence(buf[pos]);
    
    if(!ch)
      return -1;
    
    ++pos;
    while(pos < len && buf[pos] != ch){
      if(spans[pos]){
        pos = spans[pos].end() + 1;
      }
      else
        ++pos;
    }
    
    if(pos < len && buf[pos] == ch)
      return pos;
  }
  else if(pmath_char_is_right(buf[pos])){
    ensure_spans_valid();
    
    int right = pos;
    do{
      --pos;
    }while(pos >= 0 && char_is_white(buf[pos]));
    
    if(pos >= 0 && pmath_right_fence(buf[pos]) == buf[right])
      return pos;
    
    for(;pos >= 0;--pos){
      Span span = spans[pos];
      if(span && span.end() >= right){
        while(span.next() && span.next().end() >= right)
          span = span.next();
        
        if(pmath_right_fence(buf[pos]) == buf[right])
          return pos;
        
        ++pos;
        while(pos < right
        && pmath_right_fence(buf[pos]) != buf[right]){
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

pmath_bool_t MathSequence::subsuperscriptbox_at_index(int i, void *_data){
  ScanData *data = (ScanData*)_data;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()){
    if(data->sequence->boxes[data->current_box]->index() == i){
      SubsuperscriptBox *b = dynamic_cast<SubsuperscriptBox*>(
        data->sequence->boxes[data->current_box]);
      return 0 != b;
    }
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start){
    if(data->sequence->boxes[data->current_box]->index() == i)
      return 0 != dynamic_cast<SubsuperscriptBox*>(
        data->sequence->boxes[data->current_box]);
    ++data->current_box;
  }
  
  return FALSE;
}

pmath_string_t MathSequence::underoverscriptbox_at_index(int i, void *_data){
  ScanData *data = (ScanData*)_data;
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()){
    if(data->sequence->boxes[data->current_box]->index() == i){
      UnderoverscriptBox *box = dynamic_cast<UnderoverscriptBox*>(
        data->sequence->boxes[data->current_box]);
      
      if(box)
        return pmath_ref(box->base()->text().get_as_string());
    }
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start){
    if(data->sequence->boxes[data->current_box]->index() == i){
      UnderoverscriptBox *box = dynamic_cast<UnderoverscriptBox*>(
        data->sequence->boxes[data->current_box]);
      
      if(box)
        return pmath_ref(box->base()->text().get_as_string());
    }
    ++data->current_box;
  }
  
  return PMATH_NULL;
}

void MathSequence::syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err){
  ScanData *data = (ScanData*)_data;
  
  if(err){
    if(pos < data->sequence->glyphs.length()
    && data->sequence->glyphs.length() > 1
    && data->sequence->get_style(ShowAutoStyles)){
      data->sequence->glyphs[pos].style = GlyphStyleSyntaxError;
    }
  }
}

pmath_t MathSequence::box_at_index(int i, void *_data){
  ScanData *data = (ScanData*)_data;
  
  bool parseable = data->parseable;
  if(parseable && data->sequence->is_inside_string(i)){
    parseable = false;
  }
  
  int start = data->current_box;
  while(data->current_box < data->sequence->boxes.length()){
    if(data->sequence->boxes[data->current_box]->index() == i)
      return data->sequence->boxes[data->current_box]->to_pmath(parseable).release();
    ++data->current_box;
  }
  
  data->current_box = 0;
  while(data->current_box < start){
    if(data->sequence->boxes[data->current_box]->index() == i)
      return data->sequence->boxes[data->current_box]->to_pmath(parseable).release();
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
){
  int box = -1;
  const uint16_t *buf = str.buffer();
  for(int i = start;i < end;++i){
    if(buf[i] == PMATH_CHAR_BOX){
      if(box < 0){
        do{
          ++box;
        }while(boxes[box]->index() < i);
      }
      boxes[box++]->extents().bigger_y(a, d);
    }
    else if(glyphs[i].is_normal_text){
      context->text_shaper->vertical_glyph_size(
        context,
        buf[i],
        glyphs[i],
        a,
        d);
    }
    else{
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
){
  if(pos >= 0 && pos < glyphs.length()){
    const uint16_t *buf = str.buffer();
    if(buf[pos] == PMATH_CHAR_BOX){
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
){
  if(glyphs.length() > 0){
    box_size(context, pos - 1, box - 1, a, d);
    box_size(context, pos,     box,     a, d);
  }
  else{
    *a = _extents.ascent;
    *d = _extents.descent;
  }
}

void MathSequence::resize_span(
  Context *context, 
  Span     span, 
  int     *pos, 
  int     *box
){
  if(!span){
    if(str[*pos] == PMATH_CHAR_BOX){
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
    
    if(context->math_spacing){
      context->math_shaper->decode_token(
        context,
        next - *pos,
        buf + *pos,
        glyphs.items() + *pos);
    }
    else{
      context->text_shaper->decode_token(
        context,
        next - *pos,
        buf + *pos,
        glyphs.items() + *pos);
      
      for(int i = *pos;i < next;++i){
        if(glyphs[i].index){
          glyphs[i].is_normal_text = 1;
        }
        else{
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
  
  if(!span.next() && str[*pos] == '"'){
    const uint16_t *buf = str.buffer();
    int end = span.end();
    if(!context->show_string_characters){
      ++*pos;
      if(buf[end] == '"')
        --end;
    }
    else{
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
    
    while(*pos <= end){
      if(buf[*pos] == PMATH_CHAR_BOX){
        boxes[*box]->resize(context);
        glyphs[*pos].right = boxes[*box]->extents().width;
        glyphs[*pos].composed = 1;
        ++*box;
        ++*pos;
      }
      else{
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
        
        for(int i = *pos;i < next;++i){
          glyphs[i].is_normal_text = 1;
        }
        
        *pos = next;
      }
    }
    
    context->math_spacing = old_math_styling;
    
    *pos = span.end() + 1;
  }
  else{
    resize_span(context, span.next(), pos, box);
    while(*pos <= span.end())
      resize_span(context, spans[*pos], pos, box);
  }
}

void MathSequence::check_options(
  Expr         options,
  Context     *context,
  int          pos,
  int          end
){
  if(!options.is_expr()
  || options.expr_length() == 0
  || options[0] != PMATH_SYMBOL_LIST){
    for(;pos <= end;++pos)
      glyphs[pos].style = GlyphStyleExcessArg;
  }
  
  const uint16_t *buf = str.buffer();
  
  while(pos <= end){
    Span arg = spans[pos];
    if(arg){
      int first = pos;
      int second;
      
      if(arg.next()){
        second = arg.next().end() + 1;
      }
      else{
        second = pos;
        while(second <= end && !spans.is_token_end(second))
          ++second;
        
        ++second;
        while(second <= end && char_is_white(buf[second]))
          ++second;
      }
      
      check_argcount_span(context, arg.next(), &pos);
      while(pos < second)
        check_argcount_span(context, spans[pos], &pos);
      
      if(second <= end
      && (buf[second] == 0x2192   // ->
       || buf[second] == 0x29F4   // :>
       || (second + 1 <= end
        && buf[second + 1] == '>'
        && (buf[second] == '-' 
         || buf[second] == ':'))))
      {
        bool valid_option = false;
        
        if(pmath_token_analyse(buf + first, 1, NULL) == PMATH_TOK_NAME/*pmath_char_is_name(buf[first])*/){
          int e = first;
          while(e < second && !spans.is_token_end(e))
            ++e;
          
          String name = str.part(first, e - first + 1);
          Expr sym(pmath_symbol_find(pmath_ref(name.get()), FALSE));
          if(!sym.is_null()){
            for(size_t i = options.expr_length();i > 0;--i){
              if(options[i].is_expr()
              && options[i][1] == sym
              && options[i].expr_length() == 2
              && (options[i][0] == PMATH_SYMBOL_RULE
               || options[i][0] == PMATH_SYMBOL_RULEDELAYED))
              {
                valid_option = true;
                break;
              }
            }
          }
        }
        
        if(!valid_option){
          while(first < second){
            if(pmath_token_analyse(buf + first, 1, NULL) == PMATH_TOK_NAME//pmath_char_is_name(buf[first])
            && !glyphs[first].style){
              do{
                glyphs[first].style = GlyphStyleInvalidOption;
                ++first;
              }while(first < second && !spans.is_token_end(first - 1));
            }
            else{
              while(first < second && !spans.is_token_end(first))
                ++first;
              
              ++first;
            }
          }
        }
      }
      
      while(pos <= arg.end())
        check_argcount_span(context, spans[pos], &pos);
    }
    else{
      while(pos <= end && !spans.is_token_end(pos))
        ++pos;
      
      ++pos;
    }
  }
}

bool MathSequence::is_arglist_span(Span span, int pos){
  if(!span)
    return false;
  
  const uint16_t *buf = str.buffer();
  
  if(buf[pos] == ',')
    return true;
  
  if(span.next()){
    pos = span.next().end() + 1;
  }
  else{
    while(pos <= span.end() && !spans.is_token_end(pos))
      ++pos;
    
    ++pos;
  }
  
  while(pos <= span.end() && char_is_white(buf[pos]))
    ++pos;
  
  return pos <= span.end() && buf[pos] == ',';
}

void MathSequence::syntax_restyle_span(
  Context *context, 
  Span     span, 
  int     *pos
){
  const uint16_t *buf = str.buffer();
  
  if(!span){
    if(!glyphs[*pos].style
    && (pmath_char_is_left(buf[*pos])
     || pmath_char_is_right(buf[*pos]))){
      glyphs[*pos].style = GlyphStyleSyntaxError;
      ++*pos;
      return;
    }
    
    while(*pos < glyphs.length() && !spans.is_token_end(*pos))
      ++*pos;
    
    ++*pos;
    return;
  }
  
  int opening_pos = -1;
  uint16_t closing = 0;
  
  if(!span.next()){
    if(buf[*pos] == '"'){
      ++*pos;
      for(;*pos <= span.end();++*pos)
        glyphs[*pos].style = GlyphStyleString;
      
      if(buf[*pos - 1] == '"')
        glyphs[*pos - 1].style = GlyphStyleNone;
      
      return;
    }
    
    if(*pos + 1 < span.end()
    && buf[*pos]     == '/'
    && buf[*pos + 1] == '*'){
      for(;*pos <= span.end();++*pos)
        glyphs[*pos].style = GlyphStyleComment;
      
      return;
    }
    
    if(!glyphs[*pos].style
    && pmath_char_is_left(buf[*pos])){
      opening_pos = *pos;
      closing = pmath_right_fence(buf[*pos]);
      if(!closing)
        closing = 0xFFFF;
    }
  }
  
  syntax_restyle_span(context, span.next(), pos);
  
  if(!closing){
    if(*pos + 2 <= span.end()
    && buf[*pos]     == ':'
    && buf[*pos + 1] == ':'
    && spans.is_token_end(*pos + 1)
    && !spans[*pos + 2]){
      *pos+= 2;
      for(;*pos <= span.end();++*pos)
        glyphs[*pos].style = GlyphStyleString;
      
      return;
    }
  
    while(*pos <= span.end()){
      if(!spans[*pos] && pmath_char_is_left(buf[*pos])){
        opening_pos = *pos;
        closing = pmath_right_fence(buf[*pos]);
        break;
      }
      syntax_restyle_span(context, spans[*pos], pos);
    }
  }
  
  if(closing == UnknownGlyph){
    glyphs[opening_pos].style = GlyphStyleNone;
    
    while(*pos <= span.end())
      syntax_restyle_span(context, spans[*pos], pos);
  }
  else if(closing){
    while(*pos <= span.end()){
      if(buf[*pos] == closing){
        do{
          glyphs[opening_pos++].style = GlyphStyleNone;
        }while(!spans.is_token_end(opening_pos - 1));
        
        do{
          ++*pos;
        }while(!spans.is_token_end(*pos - 1));
        
        while(*pos <= span.end())
          syntax_restyle_span(context, spans[*pos], pos);
        
        return;
      }
      else
        syntax_restyle_span(context, spans[*pos], pos);
    }
  }
  else{
    while(*pos <= span.end())
      syntax_restyle_span(context, spans[*pos], pos);
  }
}

void MathSequence::check_argcount_span(
  Context *context, 
  Span     span, 
  int     *pos
){
  if(!span){
    while(*pos < glyphs.length() && !spans.is_token_end(*pos))
      ++*pos;
    
    ++*pos;
    return;
  }
  
  const uint16_t *buf = str.buffer();
  
  int second = *pos;
  if(span.next()){
    second = span.next().end() + 1;
  }
  else{
    while(second < glyphs.length() && !spans.is_token_end(second))
      ++second;
    
    ++second;
  }
  
  while(second < glyphs.length() && char_is_white(buf[second]))
    ++second;
  
  String name;
  
  if(second < glyphs.length()){
    if(buf[second] == '.'){
      int third = second + 1;
      while(third < glyphs.length() && char_is_white(buf[third]))
        ++third;
      
      if(third <= span.end()
      && pmath_token_analyse(buf + third, 1, NULL) == PMATH_TOK_NAME/*pmath_char_is_name(buf[third])*/){
        int n = third + 1;
        while(n <= span.end()){
          pmath_token_t tok = pmath_token_analyse(buf + n, 1, NULL);
          
          if(tok != PMATH_TOK_NAME && tok != PMATH_TOK_DIGIT)
            break;
          
          ++n;
        }
        
        name = str.part(third, n - third);
      }
    }
    else if(buf[second] == '('
    && pmath_token_analyse(buf + *pos, 1, NULL) == PMATH_TOK_NAME/*pmath_char_is_name(buf[*pos])*/){
      int n = *pos + 1;
      while(n <= second){
        pmath_token_t tok = pmath_token_analyse(buf + n, 1, NULL);
        
        if(tok != PMATH_TOK_NAME && tok != PMATH_TOK_DIGIT)
          break;
        
        ++n;
      }
      
      name = str.part(*pos, n - *pos);
    }
  }
  
  if(name.length() > 0){
    Expr options = Client::interrupt_cached(Expr(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_OPTIONS), 1,
        pmath_ref(name.get()))));
    
    if(options != PMATH_SYMBOL_FAILED){
      SyntaxInformation info(name); // sym
      
      if(info.minargs > 0 || info.maxargs < INT_MAX){
        int args = 0;
        
        if(buf[second] == '.'){
          int after_name = second + 1;
          while(after_name <= glyphs.length() && char_is_white(buf[after_name]))
            ++after_name;
          
          // skip function name:
          if(spans[after_name]){
            after_name= spans[after_name].end() + 1;
          }
          else{
            while(after_name < glyphs.length() && !spans.is_token_end(after_name))
              ++after_name;
            
            ++after_name;
          }
          
          if(info.maxargs == 0){
            check_options(options, context, *pos,       second - 1);
            check_options(options, context, after_name, span.end());
            
            *pos = span.end() + 1;
            return;
          }
          
          ++args;
          check_argcount_span(context, span.next(), pos);
          while(*pos < after_name)
            check_argcount_span(context, spans[*pos], pos);
        }
        else{ // buf[second] == '('
          check_argcount_span(context, span.next(), pos);
          while(*pos < second)
            check_argcount_span(context, spans[*pos], pos);
        }
        
        if(*pos <= span.end()
        && buf[*pos] == '('){
          ++*pos;
          while(*pos < glyphs.length() && char_is_white(buf[*pos]))
            ++*pos;
          
          if(*pos <= span.end()){
            Span argspan = spans[*pos];
            
            if(is_arglist_span(argspan, *pos)){
              if(args++ == info.maxargs)
                goto EXCESS;
                
              if(buf[*pos] == ','){
                if(args++ == info.maxargs)
                  goto EXCESS;
              }
              
              check_argcount_span(context, argspan.next(), pos);
              while(*pos <= argspan.end()){
                if(buf[*pos] == ','){
                  if(args++ == info.maxargs)
                    goto EXCESS;
                  
                  ++*pos;
                }
                else
                  check_argcount_span(context, spans[*pos], pos);
              }
              
              if(buf[*pos - 1] == ',')
                --args;
            }
            else if(buf[*pos] == ','){
              if(info.maxargs < args + 2)
                goto EXCESS;
              
              args+= 2;
            }
            else if(buf[*pos] != ')'){
              if(info.maxargs < args + 1)
                goto EXCESS;
              
              check_argcount_span(context, spans[*pos], pos);
              
              ++args;
            }
          
            if(false){ EXCESS:
              int e;
              if(argspan){
                e = argspan.end();
              }
              else{
                e = span.end();
                if(buf[e] == ')')
                  --e;
              }
              
              check_options(options, context, *pos, e);
              
              *pos = span.end() + 1;
              return;
            }
          }
        }
        
        if(args < info.minargs){
          if(*pos <= glyphs.length()){
            float d = em * ref_error_indicator_height;
            
            glyphs[*pos - 1].missing_after = 1;
            if(*pos < glyphs.length()){
              glyphs[*pos].x_offset+= 2 * d;
              glyphs[*pos].right+=    2 * d;
            }
            else
              glyphs[*pos - 1].right+= d;
          }
        }
        
        *pos = span.end() + 1;
        return;
      }
    }
  }
  
  check_argcount_span(context, span.next(), pos);
  while(*pos <= span.end())
    check_argcount_span(context, spans[*pos], pos);
}

int MathSequence::symbol_colorize(
  SyntaxState *state,
  int          start,
  SymbolKind   kind
){
  int end = start;
  while(end < glyphs.length() && !spans.is_token_end(end))
    ++end;
  ++end;
  
  if(is_placeholder(start) && start + 1 == end)
    return end;
  
  if(glyphs[start].style != GlyphStyleNone
  && glyphs[start].style != GlyphStyleParameter
  && glyphs[start].style != GylphStyleLocal
  && glyphs[start].style != GlyphStyleNewSymbol)
    return end;
  
  String name(str.part(start, end - start));
  
  SharedPtr<SymbolInfo> info = state->local_symbols[name];
  uint8_t style = GlyphStyleNone;
  
  if(kind == Global){
    while(info){
      if(info->pos->contains(state->current_pos)){
        switch(info->kind){
          case LocalSymbol:  style = GylphStyleLocal;      break;
          case Special:      style = GlyphStyleSpecialUse; break;
          case Parameter:    style = GlyphStyleParameter;  break;
          case Error:        style = GylphStyleScopeError; break;
          default: ;
        }
        
        break;
      }
      
      info = info->next;
    };
    
    if(!info){
      Expr syminfo = Client::interrupt_cached(Call(
        GetSymbol(SymbolInfoSymbol),
        name));
      
      if(syminfo == PMATH_SYMBOL_FALSE)
        style = GlyphStyleNewSymbol;
      else if(syminfo == PMATH_SYMBOL_FAILED)
        style = GlyphStyleShadowError;
      else if(syminfo == PMATH_SYMBOL_SYNTAX)
        style = GlyphStyleSyntaxError;
    }
  }
  else if(info){
    SharedPtr<SymbolInfo> si = info;
    
    do{
      if(info->pos.ptr() == state->current_pos.ptr()){
        if(info->kind == kind)
          break;
        
        if(info->kind == LocalSymbol && kind == Special){
          si->add(kind, state->current_pos);
          break;
        }
        
        if(kind == Special){
          kind = info->kind;
          break;
        }
        
        si->add(Error, state->current_pos);
        style = GylphStyleScopeError;
        break;
      }
      
      if(info->pos->contains(state->current_pos)){
        if(info->kind == LocalSymbol && kind != LocalSymbol/* && kind == Special*/){
          si->add(kind, state->current_pos);
          break;
        }
        
        if(kind == Special){
          kind = info->kind;
          break;
        }
        
        si->add(Error, state->current_pos);
        style = GylphStyleScopeError;
        break;
      }
        
      info = info->next;
    }while(info);
    
    if(!info){
      si->kind = kind;
      si->pos = state->current_pos;
      si->next = 0;
    }
  }
  else
    state->local_symbols.set(name, new SymbolInfo(kind, state->current_pos));
  
  if(!style){
    switch(kind){
      case LocalSymbol:  style = GylphStyleLocal;       break;
      case Special:      style = GlyphStyleSpecialUse; break;
      case Parameter:    style = GlyphStyleParameter;   break;
      default:           return end;
    }
  }
  
  for(int i = start;i < end;++i)
    glyphs[i].style = style;
  
//  if(style == GlyphStyleParameter)
//    for(int i = start;i < end;++i)
//      glyphs[i].slant = FontSlantItalic;
  
  return end;
}

void MathSequence::symdef_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "x"  "x:=y"
  SymbolKind   kind
){
  if(se->count() >= 2 && se->count() <= 3
  && (se->item_as_char(1) == PMATH_CHAR_ASSIGN
   || se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED
   || se->item_equals(1, ":=")
   || se->item_equals(1, "::="))
  && pmath_char_is_name(se->item_first_char(0))){
    symbol_colorize(state, se->item_pos(0), kind);
  }
  else if(pmath_char_is_name(se->first_char()))
    symbol_colorize(state, se->start(), kind);
}
  
void MathSequence::symdeflist_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "{symdefs ...}"
  SymbolKind   kind
){
  if(se->count() < 2 || se->count() > 3 || se->item_as_char(0) != '{')
    return;
  
  se = se->item(1);
  if(se->count() >= 2 && se->item_as_char(1) == ','){
    for(int i = 0;i < se->count();++i)
      symdef_colorize_spanexpr(state, se->item(i), kind);
  }
  else
    symdef_colorize_spanexpr(state, se, kind);
}

void MathSequence::replacement_colorize_spanexpr(
  SyntaxState *state,
  SpanExpr    *se,    // "x->value"
  SymbolKind   kind
){
  if(se->count() >= 2 
  && (se->item_as_char(1) == PMATH_CHAR_RULE
   || se->item_equals(1, "->"))
  && pmath_char_is_name(se->item_first_char(0))){
    symbol_colorize(state, se->item_pos(0), kind);
  }
}

void MathSequence::scope_colorize_spanexpr(SyntaxState *state, SpanExpr *se){
  assert(se != 0);
  
  if(se->count() == 0){ // identifiers   #   ~
    if(glyphs[se->start()].style)
      return;
    
    if(se->is_box()){
      se->as_box()->colorize_scope(state);
      return;
    }
    
    if(pmath_char_is_name(se->first_char())){
      symbol_colorize(state, se->start(), Global);
      return;
    }
    
    if(se->first_char() == '#'){
      uint8_t style;
      if(state->in_function)
        style = GlyphStyleParameter;
      else
        style = GylphStyleScopeError;
      
      for(int i = se->start();i <= se->end();++i)
        glyphs[i].style = style;
      
      return;
    }
    
    if(se->first_char() == '~' && state->in_pattern){
      for(int i = se->start();i <= se->end();++i)
        glyphs[i].style = GlyphStyleParameter;
      
      return;
    }
  }
  
  if(se->count() == 2){ // #x   ~x   ?x   x&   <<x
    if(se->item_first_char(0) == '#'
    && pmath_char_is_digit(se->item_first_char(1))){
      uint8_t style;
      if(state->in_function)
        style = GlyphStyleParameter;
      else
        style = GylphStyleScopeError;
      
      for(int i = se->start();i <= se->end();++i)
        glyphs[i].style = style;
      
      return;
    }
    
    if((se->item_first_char(0) == '~'
     || se->item_as_char(0)    == '?')
    && pmath_char_is_name(se->item_first_char(1))){
      if(!state->in_pattern)
        return;
        
      symbol_colorize(state, se->item_pos(1), Parameter);
    
      for(int i = se->item_pos(0);i < se->item_pos(1);++i)
        glyphs[i].style = glyphs[se->item_pos(1)].style;
        
      return;
    }
    
    if(se->item_as_char(1) == '&'){
      bool old_in_function = state->in_function;
      state->in_function = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_function = old_in_function;
      return;
    }
  
    if(se->item_equals(0, "<<")
    || se->item_equals(0, "??")){
      return;
    }
  }
  
  if(se->count() >= 2 && se->count() <= 3){ // ~:t   x:p   x->y   x:=y   integrals   bigops
    if(se->item_as_char(1) == ':'){
      if(se->item_first_char(0) == '~'){
        for(int i = se->start();i <= se->end();++i)
          glyphs[i].style = GlyphStyleParameter;
        
        return;
      }
      
      if(pmath_char_is_name(se->item_first_char(0))){
        if(!state->in_pattern)
          return;
          
        for(int i = 0;i < se->count();++i){
          SpanExpr *sub = se->item(i);
          
          scope_colorize_spanexpr(state, sub);
        }
          
        symbol_colorize(state, se->item_pos(0), Parameter);
        return;
      }
    }
    
    if(se->item_as_char(1) == 0x21A6){ // mapsto
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      SpanExpr *sub = se->item(0);
      scope_colorize_spanexpr(state, sub);
      
      if(sub->count() == 3
      && sub->item_as_char(0) == '('
      && sub->item_as_char(2) == ')'){
        sub = sub->item(1);
      }
      
      if(sub->count() == 0
      && pmath_char_is_name(sub->first_char())){
        symbol_colorize(state, sub->start(), Parameter);
      }
      else if(sub->count() > 0
      && sub->item_as_char(1) == ','){
        const uint16_t *buf = str.buffer();
        
        for(int i = 0;i < sub->count();++i)
          if(pmath_char_is_name(buf[sub->item_pos(i)]))
            symbol_colorize(state, sub->item_pos(i), Parameter);
      }
      
      if(se->count() == 3){
        scope_colorize_spanexpr(state, se->item(2));
      }
      else{
        for(int i = se->item_pos(1);i <= se->item(1)->end();++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      state->current_pos = next_scope;
      return;
    }
  
    if(se->item_as_char(1) == PMATH_CHAR_RULE
    || se->item_as_char(1) == PMATH_CHAR_ASSIGN
    || se->item_equals(1, "->")
    || se->item_equals(1, ":=")){
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      bool old_in_pattern = state->in_pattern;
      state->in_pattern = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_pattern = old_in_pattern;
      state->current_pos = next_scope;
      
      if(se->count() == 3){
        scope_colorize_spanexpr(state, se->item(2));
      }
      else{
        for(int i = se->item_pos(1);i <= se->item(1)->end();++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      return;
    }
    
    if(se->item_as_char(1) == PMATH_CHAR_RULEDELAYED
    || se->item_as_char(1) == PMATH_CHAR_ASSIGNDELAYED
    || se->item_equals(1, ":>")
    || se->item_equals(1, "::=")){
      SharedPtr<ScopePos> next_scope = state->new_scope();
      state->new_scope();
      
      bool old_in_pattern = state->in_pattern;
      state->in_pattern = true;
      
      scope_colorize_spanexpr(state, se->item(0));
      
      state->in_pattern = old_in_pattern;
      
      if(se->count() == 3){
        scope_colorize_spanexpr(state, se->item(2));
      }
      else{
        for(int i = se->item_pos(1);i <= se->item(1)->end();++i)
          glyphs[i].style = GlyphStyleSyntaxError;
      }
      
      state->current_pos = next_scope;
      return;
    }
    
    bool have_integral = false;
    if(pmath_char_is_integral(se->item_as_char(0))){
      have_integral = true;
    }
    else{
      UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
      
      if(uo 
      && uo->base()->length() == 1 
      && pmath_char_is_integral(uo->base()->text()[0])){
        have_integral = true;
      }
    }
    
    if(have_integral){
      SpanExpr *integrand = se->item(se->count()-1);
      
      if(integrand && integrand->count() >= 1){
        SpanExpr *dx = integrand->item(integrand->count()-1);
        
        if(dx 
        && dx->count() == 2 
        && dx->item_as_char(0) == PMATH_CHAR_INTEGRAL_D
        && pmath_char_is_name(dx->item_first_char(1))){
          for(int i = 0;i < se->count() - 1;++i)
            scope_colorize_spanexpr(state, se->item(i));
          
          SharedPtr<ScopePos> next_scope = state->new_scope();
          state->new_scope();
          
          symbol_colorize(state, dx->item_pos(1), Special);
          
          for(int i = 0;i < integrand->count() - 1;++i)
            scope_colorize_spanexpr(state, integrand->item(i));
          
          state->current_pos = next_scope;
          return;
        }
      }
    }
    
    MathSequence *bigop_init = 0;
    int next_item = 1;
    if(pmath_char_maybe_bigop(se->item_as_char(0))){
      SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(se->item_as_box(1));
      
      if(subsup){
        bigop_init = subsup->subscript();
        ++next_item;
      }
    }
    else{
      UnderoverscriptBox *uo = dynamic_cast<UnderoverscriptBox*>(se->item_as_box(0));
      
      if(uo 
      && uo->base()->length() == 1 
      && pmath_char_maybe_bigop(uo->base()->text()[0]))
        bigop_init = uo->underscript();
    }
    
    if(bigop_init
    && bigop_init->length() > 0
    && bigop_init->span_array()[0]){
      SpanExpr *init = new SpanExpr(0, bigop_init->span_array()[0], bigop_init);
      
      if(init->end() + 1 == bigop_init->length()
      && init->count() >= 2
      && init->item_as_char(1) == '='
      && pmath_char_is_name(init->item_first_char(0))){
        scope_colorize_spanexpr(state, se->item(next_item - 1));
      
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bigop_init->symbol_colorize(state, init->item_pos(0), Special);
        
        for(int i = next_item;i < se->count();++i)
          scope_colorize_spanexpr(state, se->item(i));
        
        state->current_pos = next_scope;
        delete init;
        return;
      }
      
      delete init;
    }
  }
  
  if(se->count() >= 3 && se->count() <= 4){ // F(x)   ~x:t
    if(se->item_as_char(1) == '('
    && pmath_char_is_name(se->item_first_char(0))){
      String name = se->item_as_text(0);
      SyntaxInformation info(name);
      
      SpanExpr *args = se->item(2);
      bool multiargs = args->count() >= 2 && args->item_as_char(1) == ',';
      
      scope_colorize_spanexpr(state, se->item(0));
      
      if(name.equals("Local") || name.equals("With")){
        if(multiargs)
          scope_colorize_spanexpr(state, args->item(0));
        else
          scope_colorize_spanexpr(state, args);
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        symdeflist_colorize_spanexpr(state, multiargs ? args->item(0) : args, LocalSymbol);
        
        if(multiargs && args->count() >= 3){
          scope_colorize_spanexpr(state, args->item(2));
        }
        
        state->current_pos = next_scope;
        
        if(multiargs){
          for(int i = 3;i < args->count();++i)
            scope_colorize_spanexpr(state, args->item(i));
        }
        
        return;
      }
      
      if(name.equals("Function")){
        if(multiargs){
          scope_colorize_spanexpr(state, args->item(0));
        
          SharedPtr<ScopePos> next_scope = state->new_scope();
          state->new_scope();
          
          if(multiargs){
            if(pmath_char_is_name(args->item(0)->first_char()))
              symbol_colorize(state, args->item_pos(0), Parameter);
            else
              symdeflist_colorize_spanexpr(state, args->item(0), Parameter);
          }
          else if(pmath_char_is_name(args->first_char()))
            symbol_colorize(state, args->start(), Parameter);
          else
            symdeflist_colorize_spanexpr(state, args, Parameter);
          
          if(multiargs && args->count() >= 3){
            scope_colorize_spanexpr(state, args->item(2));
          }
          
          state->current_pos = next_scope;
          
          if(multiargs){
            for(int i = 3;i < args->count();++i)
              scope_colorize_spanexpr(state, args->item(i));
          }
          
          return;
        }
        
        bool old_in_function = state->in_function;
        state->in_function = true;
        
        scope_colorize_spanexpr(state, args);
        
        state->in_function = old_in_function;
        return;
      }
      
      if(info.locals_form == TableSpec){
        int locals_min_item = 0;
        int locals_max_item = 0;
        
        if(multiargs){
          int argpos = 1;
          int i;
          
          for(i = 0;i < args->count() && argpos <= info.locals_max;++i){
            if(args->item_as_char(i) == ','){
              ++argpos;
              
              if(argpos == info.locals_min)
                locals_min_item = i + 1;
            }
            else if(argpos >= info.locals_min)
              scope_colorize_spanexpr(state, args->item(i));
          }
          
          locals_max_item = i - 1;
        }
        else if(info.locals_min <= 1 && info.locals_max >= 1){
          scope_colorize_spanexpr(state, args);
        }
        else
          goto COLORIZE_ITEMS;
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        if(multiargs){
          for(int i = locals_min_item;i <= locals_max_item;++i){
            replacement_colorize_spanexpr(state, args->item(i), Special);
            
            for(int j = i+1;j <= locals_max_item;++j){
              for(int p = args->item(j)->start();p <= args->item(j)->end();++p)
                glyphs[p].style = GlyphStyleNone;
                
              scope_colorize_spanexpr(state, args->item(j));
            }
          }
          
          for(int i = 0;i < locals_min_item;++i)
            scope_colorize_spanexpr(state, args->item(i));
            
          for(int i = locals_max_item + 1;i < args->count();++i)
            scope_colorize_spanexpr(state, args->item(i));
        }
        else
          replacement_colorize_spanexpr(state, args, Special);
        
        state->current_pos = next_scope;
        
        return;
      }
    }
  
    if(se->item_as_char(2) == ':'
    && se->item_first_char(0) == '~'
    && pmath_char_is_name(se->item_first_char(1))){
      if(!state->in_pattern)
        return;
        
      symbol_colorize(state, se->item_pos(1), Parameter);
    
      for(int i = se->item_pos(0);i <= se->end();++i)
        glyphs[i].style = glyphs[se->item_pos(1)].style;
        
      return;
    }
    
  }
  
  if(se->count() >= 4 && se->count() <= 5){ // x/:y:=z   x/:y::=z
    if(se->item_equals(1, "/:")){
      if(se->item_as_char(3) == PMATH_CHAR_ASSIGN
      || se->item_equals(3, ":=")){
        scope_colorize_spanexpr(state, se->item(0));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(state, se->item(2));
        
        state->in_pattern = old_in_pattern;
        state->current_pos = next_scope;
        
        if(se->count() == 5){
          scope_colorize_spanexpr(state, se->item(4));
        }
        else{
          for(int i = se->item_pos(3);i <= se->item(3)->end();++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        return;
      }
      
      if(se->item_as_char(3) == PMATH_CHAR_ASSIGNDELAYED
      || se->item_equals(3, "::=")){
        scope_colorize_spanexpr(state, se->item(0));
        
        SharedPtr<ScopePos> next_scope = state->new_scope();
        state->new_scope();
        
        bool old_in_pattern = state->in_pattern;
        state->in_pattern = true;
        
        scope_colorize_spanexpr(state, se->item(2));
        
        state->in_pattern = old_in_pattern;
        
        if(se->count() == 5){
          scope_colorize_spanexpr(state, se->item(4));
        }
        else{
          for(int i = se->item_pos(3);i <= se->item(3)->end();++i)
            glyphs[i].style = GlyphStyleSyntaxError;
        }
        
        state->current_pos = next_scope;
        return;
      }
    }
  }
  
 COLORIZE_ITEMS:
  for(int i = 0;i < se->count();++i){
    SpanExpr *sub = se->item(i);
    
    scope_colorize_spanexpr(state, sub);
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
){
  const uint16_t *buf = str.buffer();
  
  if(span){
    int start = *pos;
    if(!span.next()){
      uint16_t ch = buf[start];
      
      if(ch == '"'){
        for(;*pos <= span.end();++*pos){
          if(buf[*pos] == PMATH_CHAR_BOX)
            ++*box;
        }
        
        return;
      }
      
      if(ch == PMATH_CHAR_BOX){
        UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[*box]);
        if(underover && underover->base()->length() == 1)
          ch = underover->base()->str[0];
      }
      
      if(pmath_char_is_left(ch)){
        float ca = 0;
        float cd = 0;
        float a = 0;
        float d = 0;
        
        ++*pos;
        while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
          stretch_span(context, spans[*pos], pos, box, &ca, &cd, &a, &d);
        
        if(*pos <= span.end() && pmath_char_is_right(buf[*pos])){
          context->math_shaper->vertical_stretch_char(
            context, a, d, true, buf[*pos], &glyphs[*pos]);
            
          ++*pos;
        }
        
        context->math_shaper->vertical_stretch_char(
          context, a, d, true, buf[start], &glyphs[start]);
        
        if(*ascent < a)
           *ascent = a;
           
        if(     *core_ascent <  a) *core_ascent =  a;
        else if(*core_ascent < ca) *core_ascent = ca;
           
        if(*descent < d)
           *descent = d;
           
        if(     *core_descent <  d) *core_descent =  d;
        else if(*core_descent < cd) *core_descent = cd;
      }
      else if(pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)){
        float a = 0;
        float d = 0;
        int startbox = *box;
        
        if(buf[start] == PMATH_CHAR_BOX){
          assert(dynamic_cast<UnderoverscriptBox*>(boxes[startbox]) != 0);
          
          ++*pos;
          ++*box;
        }
        else{
          ++*pos;
             
          if(*pos < glyphs.length()
          && buf[*pos] == PMATH_CHAR_BOX){
            SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[startbox]);
            
            if(subsup){
              ++*box;
              ++*pos;
            }
          }
        }
        
        while(*pos <= span.end())
          stretch_span(context, spans[*pos], pos, box, &a, &d, ascent, descent);
        
        if(buf[start] == PMATH_CHAR_BOX){
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
        else{
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
          size.bigger_y(     ascent,      descent);
             
          if(start + 1 < glyphs.length()
          && buf[start + 1] == PMATH_CHAR_BOX){
            SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[startbox]);
            
            if(subsup){
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
    && spans.is_token_end(*pos)){
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
      size.bigger_y(     ascent,      descent);
    }
    
    while(*pos <= span.end()
    && (!pmath_char_is_left(buf[*pos]) || spans[*pos]))
      stretch_span(context, spans[*pos], pos, box, core_ascent, core_descent, ascent, descent);
    
    if(*pos < span.end()){
      start = *pos;
      
      float ca = 0;
      float cd = 0;
      float a = 0;
      float d = 0;
      
      ++*pos;
      while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
        stretch_span(context, spans[*pos], pos, box, &ca, &cd, &a, &d);
      
      if(*pos <= span.end() && pmath_char_is_right(buf[*pos])){
        context->math_shaper->vertical_stretch_char(
          context, a, d, false, buf[*pos], &glyphs[*pos]);
          
        ++*pos;
      }
      
      context->math_shaper->vertical_stretch_char(
        context, a, d, false, buf[start], &glyphs[start]);
      
      if(*ascent < a)
         *ascent = a;
         
      if(     *core_ascent <  a) *core_ascent =  a;
      else if(*core_ascent < ca) *core_ascent = ca;
         
      if(*descent < d)
         *descent = d;
         
      if(     *core_descent <  d) *core_descent =  d;
      else if(*core_descent < cd) *core_descent = cd;
    
      while(*pos <= span.end()){
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
  
  if(buf[*pos] == PMATH_CHAR_BOX){
    SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox*>(boxes[*box]);
    
    if(subsup && *pos > 0){
      if(buf[*pos - 1] == PMATH_CHAR_BOX){
        subsup->stretch(context, boxes[*box - 1]->extents());
      }
      else{
        BoxSize size;
        
        context->math_shaper->vertical_glyph_size(
          context, buf[*pos - 1], glyphs[*pos - 1], 
          &size.ascent, &size.descent);
        
        subsup->stretch(context, size);
        subsup->adjust_x(context, buf[*pos - 1], glyphs[*pos - 1]);
      }
      
      subsup->extents().bigger_y(     ascent,      descent);
      subsup->extents().bigger_y(core_ascent, core_descent);
    }
    else{
      UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[*box]);
        
      if(underover){
        uint16_t ch = 0;
        
        if(underover->base()->length() == 1)
          ch = underover->base()->text()[0];
        
        if(spans.is_operand_start(*pos)
        && (pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch))){
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
  && (pmath_char_maybe_bigop(buf[*pos]) || pmath_char_is_integral(buf[*pos]))){
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
    size.bigger_y(     ascent,      descent);
       
    ++*pos;
    return;
  }

  do{
    context->math_shaper->vertical_glyph_size(
      context, buf[*pos], glyphs[*pos], core_ascent, core_descent);
    ++*pos;
  }while(*pos < str.length() && !spans.is_token_end(*pos - 1));
  
  if(*ascent < *core_ascent)
     *ascent = *core_ascent;
  if(*descent < *core_descent)
     *descent = *core_descent;
}

void MathSequence::enlarge_space(Context *context){
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
  
  int e = -1;
  while(true){
    i = e+= 1;
    if(i >= glyphs.length())
      break;
      
    while(e < glyphs.length() && !spans.is_token_end(e))
      ++e;
    
    // italic correction
    if(glyphs[e].slant == FontSlantItalic
    && buf[e] != PMATH_CHAR_BOX
    && (e + 1 == glyphs.length() || glyphs[e+1].slant != FontSlantItalic)){
      float ital_corr = context->math_shaper->italic_correction(
        context,
        buf[e],
        glyphs[e]);
      
      ital_corr*= em;
      if(e + 1 < glyphs.length()){
        glyphs[e+1].x_offset+= ital_corr;
        glyphs[e+1].right+= ital_corr;
      }
      else
        glyphs[e].right+= ital_corr;
    }
      
    while(e + 1 < glyphs.length() 
    && buf[e + 1] == PMATH_CHAR_BOX
    && box < boxes.length()){
      while(box < boxes.length()
      && boxes[box]->index() <= e)
        ++box;
        
      if(box == boxes.length() 
      || !dynamic_cast<SubsuperscriptBox*>(boxes[box]))
        break;
      
      ++e;
    }
    
    if(buf[i] == '\t'){
      static uint16_t arrow = 0x21e2;//0x27F6;
      float width = 2 * context->canvas->get_font_size();
      
      if(context->show_auto_styles){
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
    
    if(buf[i] == '"'){
      in_string = !in_string;
      last_was_factor = false;
      continue;
    }
    
    if(buf[i] == PMATH_CHAR_ALIASDELIMITER){
      in_alias = !in_alias;
      last_was_factor = false;
      continue;
    }
    
    if(in_string || in_alias || e >= glyphs.length())
      continue;
    
    const uint16_t *op = buf;
    int ii = i;
    int ee = e;
    if(op[ii] == PMATH_CHAR_BOX){
      while(boxes[box]->index() < i)
        ++box;
      
      if(box < boxes.length()){
        UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox*>(boxes[box]);
        if(underover && underover->base()->length() > 0){
          op = underover->base()->text().buffer();
          ii = 0;
          ee = ii;
          while(ee < underover->base()->length()
          && !underover->base()->span_array().is_token_end(ee))
            ++ee;
          
          if(ee != underover->base()->length() - 1){
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
    
    bool lwf = false;
    switch(tok){
      case PMATH_TOK_PLUSPLUS: {
        if(spans.is_operand_start(i)){
          prec = PMATH_PREC_CALL;
          goto PREFIX;
        }
        
        if(e + 1 < glyphs.length()
        && spans.is_operand_start(e+1))
          goto INFIX;
        
        prec = PMATH_PREC_CALL;
      } goto POSTFIX;
      
      case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
      case PMATH_TOK_NARY_OR_PREFIX: {
        if(spans.is_operand_start(i)){
          prec = pmath_token_prefix_precedence(op + ii, ee - ii + 1, prec);
          goto PREFIX;
        }
      } goto INFIX;
        
      case PMATH_TOK_BINARY_LEFT_AUTOARG:
      case PMATH_TOK_NARY_AUTOARG: 
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_NARY: 
      case PMATH_TOK_QUESTION: { INFIX:
        switch(prec){
          case PMATH_PREC_SEQ:
          case PMATH_PREC_EVAL: 
            space_right = em * 6/18;
            break;
          
          case PMATH_PREC_ASS: 
          case PMATH_PREC_MODY: 
            space_left  = em * 4/18;
            space_right = em * 8/18;
            break;
          
          case PMATH_PREC_LAZY:
          case PMATH_PREC_REPL:
          case PMATH_PREC_RULE:
          case PMATH_PREC_MAP:
          case PMATH_PREC_STR:
          case PMATH_PREC_COND:
          case PMATH_PREC_ARROW:
          case PMATH_PREC_REL:
            space_left = space_right = em * 5/18;
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
            space_left = space_right = em * 4/18;
            break;
          
          case PMATH_PREC_CIRCADD:
          case PMATH_PREC_CIRCMUL:
          case PMATH_PREC_MUL:
          case PMATH_PREC_DIV:
          case PMATH_PREC_MIDDOT:
          case PMATH_PREC_MUL2:
            space_left = space_right = em * 3/18;
            break;
          
          case PMATH_PREC_CROSS:
          case PMATH_PREC_POW:
          case PMATH_PREC_APL:
          case PMATH_PREC_TEST:
            space_left = space_right = em * 2/18;
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
      
      case PMATH_TOK_PREFIX: { PREFIX:
        switch(prec){
          case PMATH_PREC_REL: // not
            space_right = em * 4/18;
            break;
             
          case PMATH_PREC_ADD:
            space_right = em * 1/18;
            break;
             
          case PMATH_PREC_DIV:
            if(op[ii] == PMATH_CHAR_INTEGRAL_D){
              space_left = em * 3/18;
            }
            break;
          
          default: break;
        }
      } break;
         
      case PMATH_TOK_POSTFIX: { POSTFIX:
        switch(prec){
          case PMATH_PREC_FAC:
          case PMATH_PREC_FUNC:
            space_left = em * 2/18;
            break;
          
          default: break;
        }
      } break;
      
      case PMATH_TOK_COLON:
      case PMATH_TOK_ASSIGNTAG:
        space_left = space_right = em * 4/18;
        break;
      
      case PMATH_TOK_SPACE: {
        // implicit multiplication:
        if(buf[i] == ' ' 
        && e + 1 < glyphs.length()
        && last_was_factor && context->multiplication_sign){
          pmath_token_t tok2 = pmath_token_analyse(buf + e+1, 1, NULL);
          
          if(tok2 == PMATH_TOK_DIGIT
          || tok2 == PMATH_TOK_LEFT
          || tok2 == PMATH_TOK_LEFTCALL){
            context->math_shaper->decode_token(
              context,
              1,
              &context->multiplication_sign,
              &glyphs[i]);
            
            space_left = space_right = em * 3/18;
            
            //if(context->show_auto_styles)
            glyphs[i].style = GlyphStyleImplicit;
          }
        }
        else{
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
      case PMATH_TOK_LEFT:
        if(last_was_factor){
          space_left = em * 3/18;
        }
        break;
        
      case PMATH_TOK_RIGHT:
        lwf = true;
        break;
        
      case PMATH_TOK_PRETEXT:
        if(i+1 == e && buf[i] == '<'){
          glyphs[e].x_offset-= em * 4/18;
          glyphs[e].right-=    em * 2/18;
        }
        break;
        
      case PMATH_TOK_NONE:
      case PMATH_TOK_CALL:
      case PMATH_TOK_LEFTCALL:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_INTEGRAL:
      case PMATH_TOK_COMMENTEND:
        break;
    }
    
    last_was_number = tok == PMATH_TOK_DIGIT;
    last_was_factor = lwf;
    
    if(last_was_space){
      last_was_space = false;
      space_left     = 0;
    }
    
    if(i > 0 || e+1 < glyphs.length()){
      glyphs[i].x_offset+= space_left;
      glyphs[i].right+=    space_left;
      if(e + 1 < glyphs.length()){
        glyphs[e+1].x_offset+= space_right;
        glyphs[e+1].right+=    space_right;
      }
      else
        glyphs[e].right+= space_right;
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

class BreakPositionWithPenalty{
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

void MathSequence::split_lines(Context *context){
  if(glyphs.length() == 0)
    return;
  
  const uint16_t *buf = str.buffer();
    
  if(glyphs[glyphs.length() - 1].right <= context->width){
    bool have_newline = false;
    
    for(int i = 0;i < glyphs.length();++i)
      if(buf[i] == '\n'){
        have_newline = true;
        break;
      }
    
    if(!have_newline)
      return;
  }
  
  indention_array.length(glyphs.length() + 1);
  penalty_array.length(  glyphs.length());
  
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
  for(int start_of_paragraph = 0;start_of_paragraph < glyphs.length();){
    int end_of_paragraph = start_of_paragraph + 1;
    while(end_of_paragraph < glyphs.length() 
    && buf[end_of_paragraph-1] != '\n')
      ++end_of_paragraph;
    
    break_array.length(0);
    break_array.add(BreakPositionWithPenalty(start_of_paragraph-1,0,0.0));
    
    for(pos = start_of_paragraph;pos < end_of_paragraph;++pos){
      float xend = glyphs[pos].right;
      
      int current = break_array.length();
      break_array.add(BreakPositionWithPenalty(pos, -1, Infinity));
      
      for(int i = current - 1;i >= 0;--i){
        float xstart    = 0;
        float indention = 0;
        double penalty  = break_array[i].penalty;
        if(break_array[i].text_position >= 0){
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
        || best < xend - xstart + indention){
          double factor = 0;
          if(context->width > 0)
            factor = LineWidthFactor/context->width;
          double rel_amplitude = ((xend - xstart + indention) - best) * factor;
          penalty+= rel_amplitude * rel_amplitude;
        }
        
        if(!(penalty > break_array[current].penalty)){
          break_array[current].penalty = penalty;
          break_array[current].prev_break_index = i;
        }
      }
    }
    
    int mini = break_array.length() - 1;
    for(int i = mini - 1;i >= 0 && break_array[i].text_position + 1 == end_of_paragraph;--i){
      if(break_array[i].penalty < break_array[mini].penalty)
        mini = i;
    }
    
    if(buf[end_of_paragraph - 1] != '\n')
      mini = break_array[mini].prev_break_index;
    
    break_result.length(0);
    for(int i = mini;i > 0;i = break_array[i].prev_break_index)
      break_result.add(i);
    
    for(int i = break_result.length()-1;i >= 0;--i){
      int j = break_result[i];
      pos = break_array[j].text_position;
      
      while(pos+1 < end_of_paragraph && buf[pos+1] == ' ')
        ++pos;
      
      if(pos < end_of_paragraph){
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
  for(int line = 0;line < lines.length() - 1;++line){
    if(lines[line].end > 0 && buf[lines[line].end-1] == PMATH_CHAR_BOX){
      while(boxes[box]->index() < lines[line].end-1)
        ++box;
      
      FillBox *fb = dynamic_cast<FillBox*>(boxes[box]);
      if(fb){
        if(buf[lines[line].end] == PMATH_CHAR_BOX
        && dynamic_cast<FillBox*>(boxes[box+1]))
          continue;
          
        float w = glyphs[lines[line+1].end-1].right - glyphs[lines[line].end-1].right;
        
        if(fb->extents().width + w + indention_width(lines[line+1].indent) <= context->width){
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
){
  const uint16_t *buf = str.buffer();
  
  if(!span){
    if(pos > 0){
      if(buf[pos] == ',' 
      || buf[pos] == ';' 
      || buf[pos] == ':'
      || buf[pos] == PMATH_CHAR_ASSIGN
      || buf[pos] == PMATH_CHAR_ASSIGNDELAYED){
        penalty_array[pos-1]+= DepthPenalty;
        //--depth;
      }
    }
    
    if(buf[pos] == PMATH_CHAR_BOX && pos > 0){
      ensure_boxes_valid();
    
      while(boxes[*box]->index() < pos)
        ++*box;
      
      if(dynamic_cast<SubsuperscriptBox*>(boxes[*box])){
        penalty_array[pos-1] = Infinity;
        return pos+1;
      }
      
      if(spans.is_operand_start(pos-1)
      && dynamic_cast<GridBox*>(boxes[*box])){
        if(pos > 0 && spans.is_operand_start(pos-1)){
          if(buf[pos-1] == PMATH_CHAR_PIECEWISE){
            penalty_array[pos-1] = Infinity;
            return pos+1;
          }
          
          pmath_token_t tok = pmath_token_analyse(buf + pos - 1, 1, NULL);
          
          if(tok == PMATH_TOK_LEFT
          || tok == PMATH_TOK_LEFTCALL)
            penalty_array[pos-1] = Infinity;
        }
        
        if(pos+1 < glyphs.length()){
          pmath_token_t tok = pmath_token_analyse(buf + pos + 1, 1, NULL);
          
          if(tok == PMATH_TOK_RIGHT)
            penalty_array[pos] = Infinity;
          return pos+1;
        }
      }
    }
    
    if(!spans.is_operand_start(pos))
      depth++;
    
    if(buf[pos] == ' '){
      penalty_array[pos] = depth * DepthPenalty;
      
      return pos + 1;
    }
    
    while(pos < spans.length() && !spans.is_token_end(pos)){
      penalty_array[pos] = depth * DepthPenalty + WordPenalty;
      ++pos;
    }
    
    if(pos < spans.length()){
      penalty_array[pos] = depth * DepthPenalty;
      ++pos;
    }
    
    return pos;
  }
  
  ++depth;
  
  int next = fill_penalty_array(span.next(), depth, pos, box);
  
  if(pmath_char_is_left(buf[pos])){
    penalty_array[pos]+= WordPenalty + DepthPenalty;
  }
  
  if(buf[pos] == '"' && !span.next()){
    ++depth;
    
    penalty_array[pos] = Infinity;
    
    bool last_was_special = false;
    while(next < span.end()){
      pmath_token_t tok = pmath_token_analyse(buf + next, 1, NULL);
      penalty_array[next]+= depth * DepthPenalty + WordPenalty;
      
      switch(tok){
        case PMATH_TOK_SPACE: 
          penalty_array[next]-= WordPenalty; 
          last_was_special = false;
          break;
        
        case PMATH_TOK_STRING:
          last_was_special = false;
          break;
        
        case PMATH_TOK_NAME:
        case PMATH_TOK_NAME2:
        case PMATH_TOK_DIGIT:
          if(last_was_special)
            penalty_array[next-1]-= WordPenalty; 
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
  while(next <= span.end()){
    switch(buf[next]){
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
        if(pmath_char_is_left(buf[next])){
          penalty_array[next]+= WordPenalty;
          depth = func_depth;
        }
        else if(pmath_char_is_right(buf[next])){
          penalty_array[next - 1]+= WordPenalty;
        }
    }
    
    next = fill_penalty_array(spans[next], depth, next, box);
  }
  
  inc_penalty-= dec_penalty;
  if(inc_penalty != 0){
    for(;pos < next;++pos)
      penalty_array[pos]+= inc_penalty;
  }
  
  return next;
}

int MathSequence::fill_indention_array(
  Span  span,
  int   depth,
  int   pos
){
  const uint16_t *buf = str.buffer();
  
  if(!span){
    indention_array[pos] = depth;
    
    ++pos;
    if(pos == spans.length() || spans.is_token_end(pos - 1))
      return pos;
    
    ++depth;
    do{
      indention_array[pos] = depth;
      ++pos;
    }while(pos < spans.length() && !spans.is_token_end(pos - 1));
    
    return pos;
  }
  
  bool in_string = buf[pos] == '\"' && !span.next();
  int next = fill_indention_array(span.next(), depth + 1, pos);
  
  indention_array[pos] = depth;
//  if(in_string)
//    depth+= 1;
  
  bool depth_dec = false;
  while(next <= span.end()){
    if(in_string && next > 0 && buf[next-1] == '\n'){
      indention_array[next] = depth; // 0
      ++next;
    }
    else{
      if((buf[next] == ';' || buf[next] == ',') && !depth_dec && !in_string){
        --depth;
        depth_dec = true;
      }
      
      next = fill_indention_array(spans[next], depth + 1, next);
    }
  }
  
  return next;
}

void MathSequence::new_line(int pos, unsigned int indent, bool continuation){
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
){
  *unfilled_width = -HUGE_VAL;
  
  if(width == HUGE_VAL){
    if(window_width == HUGE_VAL)
      return;
    
    width = window_width;
  }
  
  const uint16_t *buf = str.buffer();
  
  int box = 0;
  int start = 0;
  
  float delta_x = 0;
  for(int line = 0;line < lines.length();line++){
    float total_fill_weight = 0;
    float white = 0;
    
    int oldbox = box;
    for(int pos = start;pos < lines[line].end;++pos){
      if(buf[pos] == PMATH_CHAR_BOX){
        while(boxes[box]->index() < pos)
          ++box;
        
        FillBox *fillbox = dynamic_cast<FillBox*>(boxes[box]);
        if(fillbox && fillbox->weight > 0){
          total_fill_weight+= fillbox->weight;
          white+= fillbox->extents().width;
        }
      }
      
      glyphs[pos].right+= delta_x;
    }
    
    float line_width = indention_width(line);
    if(start > 0)
      line_width-= glyphs[start - 1].right;
    if(lines[line].end > 0)
      line_width+= glyphs[lines[line].end - 1].right;
      
    if(total_fill_weight > 0){
      if(width - line_width > 0){
        float dx = 0;
        
        white+= width - line_width;
        
        box = oldbox;
        for(int pos = start;pos < lines[line].end;++pos){
          if(buf[pos] == PMATH_CHAR_BOX){
            while(boxes[box]->index() < pos)
              ++box;
            
            FillBox *fillbox = dynamic_cast<FillBox*>(boxes[box]);
            if(fillbox && fillbox->weight > 0){
              BoxSize size = fillbox->extents();
              dx-= size.width;
              
              size.width = white * fillbox->weight / total_fill_weight;
              fillbox->expand(size);
              
              dx+= size.width;
            }
          }
          
          glyphs[pos].right+= dx;
        }
        
        delta_x+= dx;
      }
    }
    
    line_width+= indention_width(lines[line].indent);
    if(*unfilled_width < line_width)
       *unfilled_width = line_width;
    
    start = lines[line].end;
  }
}

//{ insert/remove ...

int MathSequence::insert(int pos, uint16_t chr){
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, &chr, 1);
  invalidate();
  return pos + 1;
}

int MathSequence::insert(int pos, const uint16_t *ucs2, int len){
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, ucs2, len);
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const char *latin1, int len){
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, latin1, len);
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const String &s){
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, s);
  invalidate();
  return pos + s.length();
}

int MathSequence::insert(int pos, Box *box){
  if(pos > length())
    pos = length();
    
  if(MathSequence *sequence = dynamic_cast<MathSequence*>(box)){
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

int MathSequence::insert(int pos, MathSequence *sequence, int start, int end){
  if(pos > length())
    pos = length();
  
  sequence->ensure_boxes_valid();
  
  const uint16_t *buf = sequence->str.buffer();
  int box = -1;
  while(start < end){
    int next = start;
    while(next < end && buf[next] != PMATH_CHAR_BOX)
      ++next;
    
    pos = insert(pos, buf + start, next - start);
    
    if(next < end/* && buf[next] == PMATH_CHAR_BOX*/){
      if(box < 0){
        box = 0;
        while(box < sequence->count() && sequence->boxes[box]->index() < next)
          ++box;
      }
      
      pos = insert(pos, sequence->extract_box(box++));
    }
      
    start = next + 1;
  }
  
  return pos;
}

void MathSequence::remove(int start, int end){
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

Box *MathSequence::remove(int *index){
  remove(*index, *index + 1);
  return this;
}

Box *MathSequence::extract_box(int boxindex){
  Box *box = boxes[boxindex];
  
  DummyBox *dummy = new DummyBox();
  adopt(dummy, box->index());
  boxes.set(boxindex, dummy);
  
  abandon(box);
  return box;
}

////} ... insert/remove

class TmpBox: public OwnerBox{
  public:
    virtual void resize(Context *context){
      bool old_math_spacing = context->math_spacing;
      context->math_spacing = true;
      OwnerBox::resize(context);
      context->math_spacing = old_math_spacing;
    }
    
    virtual void paint(Context *context){
      bool old_math_spacing = context->math_spacing;
      context->math_spacing = true;
      
      Box *b = context->selection.get();
      while(b && b!= this)
        b = b->parent();
      
      if(b == this){
        float x, y;
        int c = context->canvas->get_color();
        context->canvas->current_pos(&x, &y);
        context->canvas->pixrect(
          x,
          y - _extents.ascent - 1,
          x + _extents.width,
          y + _extents.descent + 1,
          false);
        
        context->canvas->set_color(0xF6EDD6);
        context->canvas->fill();
        context->canvas->set_color(c);
        context->canvas->move_to(x, y);
      }
      
      OwnerBox::paint(context);
      context->math_spacing = old_math_spacing;
    }
};

struct make_box_info_t{
  Array<Box*> *boxes;
  int          options;
};

static void make_box(int pos, pmath_t obj, void *data){
  struct make_box_info_t *info = (struct make_box_info_t*)data;
  Expr expr(obj);
  
 START:
  if(expr.is_string()){
    TmpBox *box = new TmpBox;
    box->content()->load_from_object(expr, info->options);
    info->boxes->add(box);
    return;
  }
  
  if(!expr.is_expr()){
    info->boxes->add(new ErrorBox(expr));
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_LIST){
    if(expr.expr_length() == 1){
      expr = expr[1];
      goto START;
    }
    
    TmpBox *box = new TmpBox;
    box->content()->load_from_object(expr, info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_BUTTONBOX){
    ButtonBox *box = ButtonBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_DYNAMICBOX){
    DynamicBox *box = DynamicBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_INPUTFIELDBOX
  && expr.expr_length() == 1){
    InputFieldBox *box = new InputFieldBox(new MathSequence);
    box->content()->load_from_object(expr[1], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_FILLBOX){
    FillBox *box = FillBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_FRACTIONBOX
  && expr.expr_length() == 2){
    FractionBox *box = new FractionBox;
    box->numerator()->load_from_object(  expr[1], info->options);
    box->denominator()->load_from_object(expr[2], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_FRAMEBOX
  && expr.expr_length() == 1){
    FrameBox *box = FrameBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_GRIDBOX){
    GridBox *box = GridBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_INTERPRETATIONBOX
  && expr.expr_length() >= 2){
    Expr options(pmath_options_extract(expr.get(), 2));
    
    if(!options.is_null()){
      InterpretationBox *box = new InterpretationBox;
      box->content()->load_from_object(expr[1], info->options);
      box->interpretation = expr[2];
      
      if(options != PMATH_UNDEFINED){
        if(box->style)
          box->style->add_pmath(options);
        else
          box->style = new Style(options);
      }
      
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_RADICALBOX
  && expr.expr_length() == 2){
    RadicalBox *box = new RadicalBox(new MathSequence, new MathSequence);
    box->radicand()->load_from_object(expr[1], info->options);
    box->exponent()->load_from_object(expr[2], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_ROTATIONBOX){
    RotationBox *box = RotationBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_SETTERBOX){
    SetterBox *box = SetterBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_SLIDERBOX){
    SliderBox *box = SliderBox::create(expr);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_SUBSCRIPTBOX
  && expr.expr_length() == 1){
    SubsuperscriptBox *box = new SubsuperscriptBox(new MathSequence, 0);
    box->subscript()->load_from_object(expr[1], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_SUPERSCRIPTBOX
  && expr.expr_length() == 1){
    SubsuperscriptBox *box = new SubsuperscriptBox(0, new MathSequence);
    box->superscript()->load_from_object(expr[1], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_SUBSUPERSCRIPTBOX
  && expr.expr_length() == 2){
    SubsuperscriptBox *box = new SubsuperscriptBox(new MathSequence, new MathSequence);
    box->subscript()->load_from_object(  expr[1], info->options);
    box->superscript()->load_from_object(expr[2], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_SQRTBOX
  && expr.expr_length() == 1){
    RadicalBox *box = new RadicalBox;
    box->radicand()->load_from_object(expr[1], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_STYLEBOX
  && expr.expr_length() >= 1){
    StyleBox *box = StyleBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_TAGBOX){
    TagBox *box = TagBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_TRANSFORMATIONBOX){
    TransformationBox *box = TransformationBox::create(expr, info->options);
    if(box){
      info->boxes->add(box);
      return;
    }
  }
  
  if(expr[0] == PMATH_SYMBOL_UNDERSCRIPTBOX
  && expr.expr_length() == 2){
    UnderoverscriptBox *box = new UnderoverscriptBox(new MathSequence, new MathSequence, 0);
    box->base()->load_from_object(       expr[1], info->options);
    box->underscript()->load_from_object(expr[2], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_OVERSCRIPTBOX
  && expr.expr_length() == 2){
    UnderoverscriptBox *box = new UnderoverscriptBox(new MathSequence, 0, new MathSequence);
    box->base()->load_from_object(      expr[1], info->options);
    box->overscript()->load_from_object(expr[2], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == PMATH_SYMBOL_UNDEROVERSCRIPTBOX
  && expr.expr_length() == 3){
    UnderoverscriptBox *box = new UnderoverscriptBox(new MathSequence, new MathSequence, new MathSequence);
    box->base()->load_from_object(       expr[1], info->options);
    box->underscript()->load_from_object(expr[2], info->options);
    box->overscript()->load_from_object( expr[3], info->options);
    info->boxes->add(box);
    return;
  }
  
  if(expr[0] == GetSymbol(NumberBoxSymbol) 
  && expr.expr_length() == 1){
    String s(expr[1]);
    
    if(!s.is_null()){
      info->boxes->add(new NumberBox(s));
      return;
    }
  }
  
  info->boxes->add(new ErrorBox(expr));
}

void MathSequence::load_from_object(Expr object, int options){
  struct make_box_info_t info;
  
  for(int i = 0;i < boxes.length();++i)
    delete boxes[i];
  
  boxes.length(0);
  
  pmath_string_t result;
  Expr obj = object;
  
  info.boxes   = &boxes;
  info.options = options;
  
  if(obj[0] == PMATH_SYMBOL_BOXDATA && obj.expr_length() == 1)
    obj = obj[1];
  
  if(options & BoxOptionFormatNumbers){
    obj = NumberBox::prepare_boxes(obj);
  }
  
  spans = pmath_spans_from_boxes(
    pmath_ref(obj.get()),
    &result,
    make_box,
    &info);
  
  str = String(result);
  boxes_invalid = true;
}

bool MathSequence::stretch_horizontal(Context *context, float width){
  if(glyphs.length() != 1 || str[0] == PMATH_CHAR_BOX)
    return false;
    
  if(context->math_shaper->horizontal_stretch_char(
    context, 
    width, 
    str[0], 
    &glyphs[0])
  ){
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

int MathSequence::get_line(int index, int guide){
  if(guide >= lines.length())
    guide = lines.length() - 1;
  if(guide < 0)
    guide = 0;
    
  int line = guide;
  
  if(line < lines.length() && lines[line].end > index){
    while(line > 0){
      if(lines[line-1].end <= index)
        return line;
      
      --line;
    }
    
    return 0;
  }
  
  while(line < lines.length()){
    if(lines[line].end > index)
      return line;
    
    ++line;
  }
  
  return lines.length() > 0 ? lines.length()-1 : 0;
}

int MathSequence::get_box(int index, int guide){
  assert(str[index] == PMATH_CHAR_BOX);
  
  ensure_boxes_valid();
  if(guide < 0)
    guide = 0;
  
  for(int box = guide;box < boxes.length();++box){
    if(boxes[box]->index() == index)
      return box;
  }
  
  
  if(guide >= boxes.length())
    guide = boxes.length();

  for(int box = 0;box < guide;++box){
    if(boxes[box]->index() == index)
      return box;
  }
  
  assert(0 && "no box found at index.");
  return -1;
}

float MathSequence::indention_width(int i){
  float f = i * em / 2; 
  
  if(f <= _extents.width / 2)
    return f;
    
  return floor(_extents.width / em) * em / 2; 
}

//} ... class MathSequence
