#include <boxes/sectionlist.h>

#include <cmath>

#include <boxes/section.h>
#include <graphics/context.h>

using namespace richmath;

//{ class SectionList ...

SectionList::SectionList()
: Box(),
  border_visible(true),
  section_bracket_width(8),
  section_bracket_right_margin(2),
  _scrollx(0)
{
}

SectionList::~SectionList(){
  for(int i = 0;i < _sections.length();++i)
    delete _sections[i];
}

Box *SectionList::item(int i){
  return _sections[i];
}

void SectionList::resize(Context *context){
  unfilled_width = 0;
  _extents.ascent = 0;
  _extents.descent = 0;
  _extents.width = _page_width = context->width;
  init_section_bracket_sizes(context);
  
  for(int i = 0;i < _sections.length();++i){
    resize_section(context, i);
    
    _sections[i]->y_offset = _extents.descent;
    if(_sections[i]->visible)
      _extents.descent+= _sections[i]->extents().height();
    
    float w  = _sections[i]->extents().width;
    float uw = _sections[i]->unfilled_width;
    if(border_visible){
      w+=  section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
      uw+= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
    }
    
    if(_extents.width < w)
       _extents.width = w;
    
    if(unfilled_width < uw)
       unfilled_width = uw;
  }
  
//  if(border_visible){
//    _extents.width+= BorderWidth;
//    unfilled_width+= BorderWidth;
//  }
}

void SectionList::paint(Context *context){
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->save();
  cairo_translate(context->canvas->cairo(), x, y);
  
  _scrollx = 0;
  for(int i = 0;i < _sections.length();++i){
    context->canvas->move_to(0, _sections[i]->y_offset);
    paint_section(context, i, _scrollx);
  }
  
  if(context->selection.get() == this
  && context->selection.start == _sections.length()){
    float x1 = 0;
    float y1 = _extents.descent;
    float x2 = _extents.width;
    float y2 = _extents.descent;
    
    context->canvas->align_point(&x1, &y1, true);
    context->canvas->align_point(&x2, &y2, true);
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y2);
    context->draw_selection_path();
//    context->canvas->paint_selection(
//      0,
//      _extents.descent,
//      _extents.width,
//      _extents.descent,
//      false);
  }

  context->canvas->restore();
}

pmath_t SectionList::to_pmath(bool parseable){
  return to_pmath(parseable, 0, length());
}

pmath_t SectionList::to_pmath(bool parseable, int start, int end){
  pmath_gather_begin(NULL);
  
  emit_pmath(parseable, start, end);
  
  pmath_t group = pmath_gather_end();
  
  if(pmath_expr_length(group) == 1){
    pmath_t section = pmath_expr_get_item(group, 1);
    pmath_unref(group);
    return section;
  }
  
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_SECTIONGROUP), 2,
    group,
    pmath_ref(PMATH_SYMBOL_ALL));
}

void SectionList::emit_pmath(bool parseable, int start, int end){
  while(start < end){
    if(_group_info[start].end == start){
      pmath_emit(_sections[start]->to_pmath(parseable), NULL);
      ++start;
    }
    else{
      int e = _group_info[start].end+1;
      if(e > end+1)
         e = end+1;
      
      pmath_gather_begin(NULL);
      
      pmath_emit(_sections[start]->to_pmath(parseable), NULL);
      
      emit_pmath(parseable, start+1, e);
      
      pmath_t group = pmath_gather_end();
      pmath_t open;
      if(_group_info[start].close_rel >= 0
      && _group_info[start].close_rel <= e)
        open = pmath_integer_new_si(_group_info[start].close_rel + 1);
      else
        open = pmath_ref(PMATH_SYMBOL_ALL);
      
      group = pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_SECTIONGROUP), 2,
        group,
        open);
      pmath_emit(group, NULL);
      
      start = e;
    }
  }
}

Box *SectionList::move_logical(
  LogicalDirection  direction, 
  bool              jumping, 
  int              *index
){
  if(direction == Forward){
    if(*index >= count()){
      if(_parent){
        *index = _index;
        return _parent->move_logical(Forward, true, index);
      }
      return this;
    }
      
    if(jumping){
      ++*index;
      return this;
    }
    
    int b = *index;
    *index = -1;
    return _sections[b]->move_logical(Forward, true, index);
  }
  
  if(*index <= 0){
    if(_parent){
      *index = _index + 1;
      return _parent->move_logical(Backward, true, index);
    }
    return this;
  }

  if(jumping){
    --*index;
    return this;
  }
  
  int b = *index - 1;
  *index = _sections[b]->length() + 1;
  return _sections[b]->move_logical(Backward, true, index);
}

Box *SectionList::move_vertical(
  LogicalDirection  direction, 
  float            *index_rel_x,
  int              *index
){
  int s = *index;
  
  if(direction == Backward){
    do{
      --s;
    }while(s >= 0 && s < _sections.length() && !_sections[s]->visible);
  }
  else{
    while(s >= 0 && s < _sections.length() && !_sections[s]->visible){
      ++s;
    }
  }
  
  if(s < 0 || s >= _sections.length())
    return this;
    
  *index = -1;
  return _sections[s]->move_vertical(direction, index_rel_x, index);
}
  
Box *SectionList::mouse_selection(
  float x,
  float y,
  int   *start,
  int   *end,
  bool  *eol
){
  float right = _scrollx + _window_width - section_bracket_right_margin;
  int border_level = -1;
  
  if(border_visible
  && x <= right 
  && section_bracket_width > 0){
    border_level = (int)ceil((right - x)/section_bracket_width + 0.2);
  } 
  
  *start = 0;
  while(*start < _sections.length()){
    if(_sections[*start]->visible){
      if(y <= _sections[*start]->y_offset + _sections[*start]->top_margin){
        int lastvis = *start-1;
        while(lastvis >= 0 && !_sections[lastvis]->visible)
          --lastvis;
        
        *end = *start = lastvis+1;
        
        if(border_level >= 0 && border_level < _group_info[*start].nesting){
          int d = _group_info[*start].nesting - border_level;
          
          if(_group_info[*start].end > *start)
            d-= 1;
          while(d-- > 0 && *start >= 0)
            *start = _group_info[*start].first;
            
          if(*start >= 0){
            *end = _group_info[*start].end + 1;
          }
          else{
            *start = 0;
            *end = _sections.length();
          }
        }
        
        return this;
      }
      
      if(y < _sections[*start]->y_offset + _sections[*start]->extents().height() - _sections[*start]->bottom_margin){
        if(border_level >= 0 && border_level <= _group_info[*start].nesting){
          int d = _group_info[*start].nesting - border_level;
          
          if(_group_info[*start].end > *start){
            if(d == 0){
              *end = *start + 1;
              return this;
            }
            
            d-= 1;
          }
            
          while(d-- > 0 && *start >= 0)
            *start = _group_info[*start].first;
            
          if(*start >= 0){
            *end = _group_info[*start].end + 1;
          }
          else{
            *start = 0;
            *end = _sections.length();
          }
          
          return this;
        }
        
        y-= _sections[*start]->y_offset;
        return _sections[*start]->mouse_selection(x, y, start, end, eol);
      }
    }
    
    ++*start;
  }
  
  *end = *start;
  return this;
}

void SectionList::child_transformation(
  int             index,
  cairo_matrix_t *matrix
){
  if(index < _sections.length())
    cairo_matrix_translate(matrix, 0, _sections[index]->y_offset);
  else
    cairo_matrix_translate(matrix, 0, _extents.height());
}

void SectionList::set_open_close_group(int i, bool open){
  if(_group_info[i].end > i){
    _group_info[i].close_rel = open ? -1 : 0;
    update_section_visibility();
  }
  else if(_group_info[i].first >= 0){
    if(open)
      _group_info[_group_info[i].first].close_rel = -1;
    else
      _group_info[_group_info[i].first].close_rel = i - _group_info[i].first;
      
    update_section_visibility();
  }
}

void SectionList::toggle_open_close_group(int i){
  if(_group_info[i].end > i){
    set_open_close_group(i, _group_info[i].close_rel >= 0);
  }
  else if(_group_info[i].first >= 0){
    set_open_close_group(i, _group_info[_group_info[i].first].close_rel >= 0);
  }
}

void SectionList::insert_pmath(int *pos, Expr boxes){
  if(boxes[0] == PMATH_SYMBOL_SECTIONGROUP
  && boxes[1][0] == PMATH_SYMBOL_LIST){
    Expr sect = boxes[1];
    Expr open = boxes[2];
    
    int start = *pos;
    int opened = 0;
    int close_rel = -1;
    if(open.instance_of(PMATH_TYPE_INTEGER)){
      opened = pmath_integer_get_si(open.get());
    }
    
    size_t i;
    for(i = 1;i <= sect.expr_length();++i){
      if(--opened == 0){
        close_rel = *pos - start;
      }
      
      insert_pmath(pos, sect[i]);
    }
    
    if(start < _sections.length() && close_rel >= 0){
      _group_info[start].close_rel = close_rel;
      update_section_visibility();
    }
  }
  else{
    Section *s = Section::create_from_object(boxes);
    
    if(s){
      insert(*pos, s);
      ++*pos;
    }
  }
}

void SectionList::insert(int pos, Section *section){
  _sections.insert(pos, 1, &section);
  
  for(int i = pos;i < _sections.length();++i)
    adopt(_sections[i], i);
  
  SectionGroupInfo sgi;
  sgi.precedence = section->get_own_style(SectionGroupPrecedence, 0.0);
  _group_info.insert(pos, 1, &sgi);
  
  recalc_group_info();
  update_group_nesting();
  set_open_close_group(pos, true);
  invalidate();
}

Section *SectionList::swap(int pos, Section *section){
  Section *old = _sections[pos];
  
  assert(section != old);
  
  _sections[pos] = section;
  adopt(section, pos);
  abandon(old);
  
  _group_info[pos].precedence = section->get_own_style(SectionGroupPrecedence, 0.0);
  recalc_group_info();
  update_group_nesting();
  set_open_close_group(pos, true);
  invalidate();
  return old;
}

void SectionList::remove(int start, int end){
  set_open_close_group(start, true);
    
  for(int i = start;i < end;++i)
    delete _sections[i];
  
  if(end < _sections.length()){
    double y = 0;
    if(start > 0)
      y = _sections[start - 1]->y_offset;
    
    y = _sections[end]->y_offset - y;
    for(int i = end;i < _sections.length();++i)
      _sections[i]->y_offset-= y;
  }
  
  _sections.remove(  start, end - start);
  _group_info.remove(start, end - start);
  for(int i = start;i < _sections.length();++i)
    adopt(_sections[i], i);
  
  recalc_group_info();
  update_group_nesting();
  invalidate();
}

Box *SectionList::remove(int *index){
  remove(*index, *index + 1);
  return this;
}

void SectionList::recalc_group_info(){
  int pos = 0;
  while(pos < _group_info.length()){
    _group_info[pos].first = -1;
    recalc_group_info_part(&pos);
  }
}

void SectionList::recalc_group_info_part(int *pos){
  int start = *pos;
  ++*pos;
  
  while(*pos < _group_info.length()
  && _group_info[start].precedence < _group_info[*pos].precedence){
    _group_info[*pos].first = start;
    recalc_group_info_part(pos);
  }
  
  _group_info[start].end = *pos - 1;
}

void SectionList::update_group_nesting(){
  int pos = 0;
  while(pos < _group_info.length())
    update_group_nesting_part(&pos, 1);
}

void SectionList::update_group_nesting_part(int *pos, int current_nesting){
  int end = _group_info[*pos].end;
  
  if(*pos < end)
    ++current_nesting;
  
  if(_group_info[*pos].nesting != current_nesting){
    if(_sections[*pos]->get_own_style(LineBreakWithin, true)){
      _sections[*pos]->invalidate();
    }
  }
  _group_info[*pos].nesting = current_nesting;
    
  ++*pos;
  while(*pos <= end)
    update_group_nesting_part(pos, current_nesting);
}

void SectionList::update_section_visibility(){
  int pos = 0;
  while(pos < _sections.length()){
    if(_group_info[pos].end != pos){
      int start = pos;
      if(_group_info[start].close_rel >= 0
      && _group_info[start].close_rel <= _group_info[start].end - start){
        while(pos < start + _group_info[start].close_rel){
          if(_sections[pos]->visible){
            invalidate();
            _sections[pos]->visible = false;
          }
          ++pos;
        }
        
        if(!_sections[pos]->visible){
          invalidate();
          _sections[pos]->visible = true;
        }
        
        ++pos;
        while(pos <= _group_info[start].end){
          if(_sections[pos]->visible){
            invalidate();
            _sections[pos]->visible = false;
          }
          ++pos;
        }
        
        continue;
      }
      
      _group_info[start].close_rel = -1;
    }
    
    if(!_sections[pos]->visible){
      invalidate();
      _sections[pos]->visible = true;
    }
    ++pos;
  }
}

void SectionList::init_section_bracket_sizes(Context *context){
  float dx = 1;
  float dy = 0;
  
  context->canvas->user_to_device_dist(&dx, &dy);
  float pix = 1/sqrt(dx * dx + dy * dy);
  
  section_bracket_width        = 8 * pix;
  section_bracket_right_margin = 2 * pix;
}

void SectionList::resize_section(Context *context, int i){
  float old_w = context->width;
  
  if(border_visible){
    context->width-= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
  }
  
  _sections[i]->resize(context);
  
  context->width = old_w;
}

void SectionList::paint_section(Context *context, int i, float scrollx){
  float old_w = context->width;
  _scrollx = scrollx;
  
  if(border_visible)
    context->width-= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
  
  if(_sections[i]->must_resize)
    _sections[i]->resize(context);
    
  context->width = old_w;
  
  if(!_sections[i]->visible)
    return;
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  float w = context->width;
  if(w == HUGE_VAL){
    w = _window_width;
  }
  
  context->canvas->save();
  {
    context->canvas->pixrect(
      x + _scrollx,
      y - _sections[i]->extents().ascent,
      x + _scrollx + w,
      y + _sections[i]->extents().descent,
      false);
    context->canvas->clip();
      
    context->canvas->move_to(x, y);
    
    Expr expr;
    context->stylesheet->get(style, TextShadow, &expr);
    context->draw_with_text_shadows(_sections[i], expr);
    
  }
  context->canvas->restore();
  paint_section_brackets(context, i, x + _scrollx + _window_width, y);
  
  context->canvas->reset_font_cache();
  context->canvas->move_to(x, y + _sections[i]->extents().height());
}

void SectionList::paint_section_brackets(Context *context, int i, float right, float top){
  if(border_visible){
    int style = BorderDefault;
    
    if(_sections[i]->evaluating)
      style = style + BorderEval;
    
    if(_sections[i]->dialog_start)
      style = style + BorderSession;
    
    float x1 = right
      - section_bracket_right_margin 
      - section_bracket_width * _group_info[i].nesting;
    
    float x2 = x1 + section_bracket_width;
    float y1 = top + _sections[i]->top_margin;
    float y2 = top + _sections[i]->extents().height() - _sections[i]->bottom_margin;
    
    float sel_y1 = y1 - 1.5;
    float sel_y2 = y2 + 1.5;
    
    int sel_depth = -1;
    if(context->selection.get() == this
    && context->selection.start <= i
    && context->selection.end > i){
      int start = i;
      if(_group_info[i].end > i)
        ++sel_depth;
        
      while(start >= context->selection.start
      && _group_info[start].end < context->selection.end){
        ++sel_depth;
        start = _group_info[start].first;
      }
    }
    
    if(_sections[i]->get_style(ShowSectionBracket))
      paint_single_section_bracket(context, x1, y1, x2, y2, style);
    
    if(sel_depth-- == 0){
      context->canvas->pixrect(x1, sel_y1, x2, sel_y2, false);
      context->draw_selection_path();
    }
    
    int s = i;
    int e = i;
    
    style = BorderDefault;
    int start = i;
    while(start >= 0){
      int end = _group_info[start].end;
      
      if(start < end){
        x1 = x2;
        x2 = x1 + section_bracket_width;
        
        if(_group_info[start].close_rel + start == s){
          if(start < s)
            style |= BorderTopArrow;
          
          if(end > e)
            style |= BorderBottomArrow;
          
          s = start;
          e = end;
        }
        
        if(start < s && !(style & BorderTopArrow)){
          y1 = sel_y1 = top;
          style |= BorderNoTop;
        }
        
        if(end > e && !(style & BorderBottomArrow)){
          y2 = sel_y2 = top + _sections[i]->extents().height();
          style |= BorderNoBottom;
        }
        
        if(_sections[start]->get_style(ShowSectionBracket))
          paint_single_section_bracket(context, x1, y1, x2, y2, style);
        
        style = style & ~BorderTopArrow;
        style = style & ~BorderBottomArrow;
        
        if(sel_depth-- == 0){
          context->canvas->pixrect(x1, sel_y1, x2, sel_y2, false);
          context->draw_selection_path();
        }
      }
      
      start = _group_info[start].first;
    }
  }
}

void SectionList::paint_single_section_bracket(
  Context *context,
  float    x1,
  float    y1,
  float    x2,
  float    y2,
  int      style  // int BorderXXX constants
){
  float px1 = x1 + 0.3 * (x2-x1);
  float py1 = y1;
  float px2 = x1 + 0.8 * (x2-x1);
  float py2 = y1;
  float px3 = px2;
  float py3 = y2 - 0.75;
  float px4 = px1;
  float py4 = py3;
  
  context->canvas->save();
  int c = context->canvas->get_color();
  {
    cairo_set_line_width(context->canvas->cairo(), 1);
    cairo_set_line_cap( context->canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_join(context->canvas->cairo(), CAIRO_LINE_JOIN_MITER);
    
    context->canvas->user_to_device(&px1, &py1);
    context->canvas->user_to_device(&px2, &py2);
    context->canvas->user_to_device(&px3, &py3);
    context->canvas->user_to_device(&px4, &py4);
    
    px1 = ceilf(px1) - 0.5f;   py1 = ceilf(py1) - 0.5f;
    px2 = ceilf(px2) - 0.5f;   py2 = ceilf(py2) - 0.5f;
    px3 = ceilf(px3) - 0.5f;   py3 = ceilf(py3) - 0.5f;
    px4 = ceilf(px4) - 0.5f;   py4 = ceilf(py4) - 0.5f;
//    context->canvas->align_point(&px1, &py1, true);
//    context->canvas->align_point(&px2, &py2, true);
//    context->canvas->align_point(&px3, &py3, true);
//    context->canvas->align_point(&px4, &py4, true);
    
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    cairo_set_matrix(context->canvas->cairo(), &mat);
    
    int col = 0x999999; // 0x454E99
    context->canvas->set_color(col);
    if(style & BorderTopArrow){
      context->canvas->move_to(px2, py2);
      context->canvas->line_to(px1, py1 + (px2-px1));
      context->canvas->line_to(px2, py2 + (px2-px1));
      context->canvas->close_path();
      context->canvas->fill_preserve();
      context->canvas->stroke();
      style|= BorderNoTop;
    }
    
    if(style & BorderBottomArrow){
      context->canvas->move_to(px3, py3);
      context->canvas->line_to(px4, py4 - (px2-px1));
      context->canvas->line_to(px3, py3 - (px2-px1));
      context->canvas->close_path();
      context->canvas->fill_preserve();
      context->canvas->stroke();
      style|= BorderNoBottom;
    }
    
    if(style & BorderNoTop){
      context->canvas->move_to(px2, py2);
    }
    else{
      context->canvas->move_to(px1, py1);
      context->canvas->line_to(px2, py2);
    }
    
    context->canvas->line_to(px3, py3);
    
    if(!(style & BorderNoBottom)){
      context->canvas->line_to(px4, py4);
    }
    
    if(style & BorderSession){
      double dashes[2] = {
       1.0,  /* ink */
       1.0   /* skip*/
      };
      
      cairo_set_dash(      context->canvas->cairo(), dashes, 2, 0.5);
      cairo_set_line_cap(  context->canvas->cairo(), CAIRO_LINE_CAP_BUTT);
      cairo_set_line_width(context->canvas->cairo(), 3.0);
      
      context->canvas->set_color(0x000000);
      context->canvas->stroke_preserve();
      
      cairo_set_dash(context->canvas->cairo(), NULL, 0, 0.0);
    }
    else if(style & BorderEval){
      cairo_set_line_width(context->canvas->cairo(), 3.0);
      
      context->canvas->set_color(0x000000);
      context->canvas->stroke_preserve();
      
      //col = 0xffffff & ~col;
    }
    
    cairo_set_line_width(context->canvas->cairo(), 1.0);
    context->canvas->set_color(col);
    context->canvas->stroke();
  }
  context->canvas->set_color(c);
  context->canvas->restore();
}

//} ... class SectionList
