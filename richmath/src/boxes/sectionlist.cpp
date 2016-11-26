#include <boxes/sectionlist.h>

#include <cmath>

#include <boxes/section.h>
#include <graphics/context.h>

using namespace richmath;

//{ class SectionList ...

Expr SectionList::group(Expr sections) {
  if(!sections.is_valid()) {
    return Call(
             Symbol(PMATH_SYMBOL_SECTIONGROUP),
             List(),
             Symbol(PMATH_SYMBOL_ALL));
  }
  
  if(sections[0] == PMATH_SYMBOL_LIST) {
    return Call(
             Symbol(PMATH_SYMBOL_SECTIONGROUP),
             sections,
             Symbol(PMATH_SYMBOL_ALL));
  }
  
  return sections;
}

SectionList::SectionList()
  : Box(),
    section_bracket_width(8),
    section_bracket_right_margin(2),
    _scrollx(0)
{
}

SectionList::~SectionList() {
  for(int i = 0; i < _sections.length(); ++i)
    delete _sections[i];
}

Box *SectionList::item(int i) {
  return _sections[i];
}

void SectionList::resize(Context *context) {
  unfilled_width = 0;
  _extents.ascent = 0;
  _extents.descent = 0;
  _extents.width = _page_width = context->width;
  init_section_bracket_sizes(context);
  
  for(int i = 0; i < _sections.length(); ++i) {
    resize_section(context, i);
    
    _sections[i]->y_offset = _extents.descent;
    if(_sections[i]->visible) {
      _extents.descent += _sections[i]->extents().height();
      
      float w  = _sections[i]->extents().width;
      float uw = _sections[i]->unfilled_width;
      if(get_own_style(ShowSectionBracket, true)) {
        w +=  section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
        uw += section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
      }
      
      if(_extents.width < w)
        _extents.width = w;
        
      if(unfilled_width < uw)
        unfilled_width = uw;
    }
  }
  
//  if(get_own_style(ShowSectionBracket, true)){
//    _extents.width+= BorderWidth;
//    unfilled_width+= BorderWidth;
//  }
}

void SectionList::paint(Context *context) {
  if(style)
    style->update_dynamic(this);
    
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  context->canvas->save();
  cairo_translate(context->canvas->cairo(), x, y);
  
  //_scrollx = 0;
  for(int i = 0; i < _sections.length(); ++i) {
    context->canvas->move_to(0, _sections[i]->y_offset);
    paint_section(context, i);
  }
  
  if( context->selection.get() == this &&
      context->selection.start == _sections.length())
  {
    float x1 = 0;
    float y1 = _extents.descent;
    float x2 = _extents.width;
    float y2 = _extents.descent;
    
    context->canvas->align_point(&x1, &y1, true);
    context->canvas->align_point(&x2, &y2, true);
    context->canvas->move_to(x1, y1);
    context->canvas->line_to(x2, y2);
    
    context->draw_selection_path();
  }
  
  context->canvas->restore();
}

void SectionList::selection_path(Canvas *canvas, int start, int end) {
  float yend;
  if(end < length())
    yend = _sections[end]->y_offset;
  else
    yend = _extents.height();
    
  float x, y;
  canvas->current_pos(&x, &y);
  
  if(start == end) {
    float x1 = x;
    float y1 = y + yend;
    float x2 = x + _extents.width;
    float y2 = y + yend;
    
    canvas->align_point(&x1, &y1, true);
    canvas->align_point(&x2, &y2, true);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x2, y2);
    canvas->close_path();
  }
  else {
    //float left   = x;
    float top    = y + _sections[start]->y_offset;
    //float right  = x + _extents.width;
    float bottom = y + yend;
    
    float left  = _extents.width;
    float right = 0;
    for(int i = start; i < end; ++i) {
      Section *sect = section(i);
      
      float r = sect->unfilled_width;
      if(right < r)
        right = r;
        
      float l = sect->get_style(SectionMarginLeft);
      l -= sect->label_width();
      l -= 3; // label distance
      if(left > l)
        left = l;
    }
    
    left += x;
    right += x;
    
    top   += section(start)->top_margin;
    bottom -= section(end - 1)->bottom_margin;
    
    float x1 = left;
    float y1 = top;
    float x2 = right;
    float y3 = bottom;
    float y2 = top;
    float x3 = right;
    float x4 = left;
    float y4 = bottom;
    
    canvas->align_point(&x1, &y1, false);
    canvas->align_point(&x2, &y2, false);
    canvas->align_point(&x3, &y3, false);
    canvas->align_point(&x4, &y4, false);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x2, y2);
    canvas->line_to(x3, y3);
    canvas->line_to(x4, y4);
    canvas->close_path();
  }
}

Expr SectionList::to_pmath(int flags) {
  return to_pmath(flags, 0, length());
}

Expr SectionList::to_pmath(int flags, int start, int end) {
  Gather g;
  
  emit_pmath(flags, start, end);
  
  Expr e = g.end();
  if(e.expr_length() == 1)
    return e[1];
    
  return Call(Symbol(PMATH_SYMBOL_SECTIONGROUP), e, Symbol(PMATH_SYMBOL_ALL));
}

void SectionList::emit_pmath(int flags, int start, int end) {
  while(start < end) {
    if(_group_info[start].end == start) {
      Gather::emit(_sections[start]->to_pmath(flags));
      ++start;
    }
    else {
      int e = _group_info[start].end + 1;
      if(e > end)
        e = end;
        
      Gather g;
      
      g.emit(_sections[start]->to_pmath(flags));
      
      emit_pmath(flags, start + 1, e);
      
      Expr group = g.end();
      Expr open;
      if( _group_info[start].close_rel >= 0 &&
          _group_info[start].close_rel <= e)
      {
        open = Expr(_group_info[start].close_rel + 1);
      }
      else
        open = Symbol(PMATH_SYMBOL_ALL);
        
      group = Call(
                Symbol(PMATH_SYMBOL_SECTIONGROUP),
                group,
                open);
      Gather::emit(group);
      
      start = e;
    }
  }
}

Box *SectionList::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  if(direction == LogicalDirection::Forward) {
    if(*index >= count()) {
      if(_parent) {
        *index = _index;
        return _parent->move_logical(LogicalDirection::Forward, true, index);
      }
      return this;
    }
    
    if(jumping) {
      ++*index;
      return this;
    }
    
    int b = *index;
    *index = -1;
    return _sections[b]->move_logical(LogicalDirection::Forward, true, index);
  }
  
  if(*index <= 0) {
    if(_parent) {
      *index = _index + 1;
      return _parent->move_logical(LogicalDirection::Backward, true, index);
    }
    return this;
  }
  
  if(jumping) {
    --*index;
    return this;
  }
  
  int b = *index - 1;
  *index = _sections[b]->length() + 1;
  return _sections[b]->move_logical(LogicalDirection::Backward, true, index);
}

Box *SectionList::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  int s = *index;
  
  if(s < 0) {
    if(direction == LogicalDirection::Backward)
      *index = _sections.length();
    else
      *index = 0;
    return this;
  }
  
  if(direction == LogicalDirection::Backward) {
    if(s == 0) {
      *index = s;
      return this;
    }
    
    if(!called_from_child)
      --s;
      
    int next_group = s;
    while(next_group >= 0) {
      int group_start = next_group;
      next_group =  _group_info[group_start].first;
      
      assert(next_group < group_start);
      
      if(_group_info[group_start].close_rel >= 0) { // group is closed
        if(called_from_child) // step out of group
          s = group_start;
        else
          s = group_start + _group_info[group_start].close_rel;
      }
    }
  }
  else {
    if(s == _sections.length()) {
      *index = s;
      return this;
    }
    
    int next_group = s;
    while(next_group >= 0) {
      int group_start = next_group;
      next_group =  _group_info[group_start].first;
      
      assert(next_group < group_start);
      
      if(_group_info[group_start].close_rel >= 0) { // group is closed
        if(called_from_child) // step out of group
          s = _group_info[group_start].end;
        else // step into group
          s = group_start + _group_info[group_start].close_rel;
      }
    }
    
    if(called_from_child)
      ++s;
  }
  
  if(called_from_child || s == _sections.length()) {
    *index = s;
    return this;
  }
  
  assert(s >= 0);
  assert(s < _sections.length());
  
  *index = -1;
  return _sections[s]->move_vertical(direction, index_rel_x, index, false);
}

Box *SectionList::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool  *was_inside_start
) {
  *was_inside_start = true;
  
  float right = _scrollx + _window_width - section_bracket_right_margin;
  int border_level = -1;
  
  if(get_own_style(ShowSectionBracket, true) && section_bracket_width > 0) {
    border_level = (int)ceil((right - x) / section_bracket_width + 0.2);
    if(border_level < 0)
      border_level = 0;
  }
  
  *start = 0;
  while(*start < _sections.length()) {
    if(_sections[*start]->visible) {
      if(y <= _sections[*start]->y_offset + _sections[*start]->top_margin) {
        int group_start = 0;
        if(_group_info[*start].end > *start) { // this starts a group
          group_start = *start;
        }
        else if(_group_info[*start].first >= 0) {
          group_start = _group_info[*start].first;
        }
        
        int lastvis = *start - 1;
        while(lastvis >= group_start && !_sections[lastvis]->visible)
          --lastvis;
          
        *end = *start = lastvis + 1;
        
        if(border_level >= 0 && border_level < _group_info[*start].nesting) {
          int d = _group_info[*start].nesting - border_level;
          
          if(_group_info[*start].end > *start)
            d -= 1;
          while(d-- > 0 && *start >= 0)
            *start = _group_info[*start].first;
            
          if(*start >= 0) {
            *end = _group_info[*start].end + 1;
          }
          else {
            *start = 0;
            *end = _sections.length();
          }
        }
        
        return this;
      }
      
      if(y < _sections[*start]->y_offset + _sections[*start]->extents().height() - _sections[*start]->bottom_margin) {
        if(border_level >= 0 && border_level <= _group_info[*start].nesting) {
          int d = _group_info[*start].nesting - border_level;
          
          if(_group_info[*start].end > *start) {
            if(d == 0) {
              *end = *start + 1;
              return this;
            }
            
            d -= 1;
          }
          
          while(d-- > 0 && *start >= 0)
            *start = _group_info[*start].first;
            
          if(*start >= 0) {
            *end = _group_info[*start].end + 1;
          }
          else {
            *start = 0;
            *end = _sections.length();
          }
          
          return this;
        }
        
        y -= _sections[*start]->y_offset;
        x += get_content_scroll_correction_x(*start);
        return _sections[*start]->mouse_selection(x, y, start, end, was_inside_start);
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
) {
  if(index < _sections.length()) {
    cairo_matrix_translate(
      matrix,
      - get_content_scroll_correction_x(index),
      _sections[index]->y_offset);
  }
  else {
    cairo_matrix_translate(
      matrix,
      0,
      _extents.height());
  }
}

Box *SectionList::normalize_selection(int *start, int *end) {
//  bool equal_start_end = *start == *end;
//
//  if(*end < 0)
//    *end = 0;
//
//  while(*end < length() && !_sections[*end]->visible)
//    ++*end;
//
//  if(equal_start_end) {
//    *start = *end;
//    return this;
//  }
//
//  while(*start > 0 && !_sections[*start]->visible)
//    --*start;

  return this;
}

void SectionList::set_open_close_group(int i, bool open) {
  if(open) {
    while(i >= 0) {
      _group_info[i].close_rel = -1;
      i = _group_info[i].first;
    }
    
    update_section_visibility();
    return;
  }
  
  if(_group_info[i].end > i) {
    _group_info[i].close_rel = 0;
      
    update_section_visibility();
  }
  else if(_group_info[i].first >= 0) {
    _group_info[_group_info[i].first].close_rel = i - _group_info[i].first;
      
    update_section_visibility();
  }
}

void SectionList::toggle_open_close_group(int i) {
  if(_group_info[i].end > i) {
    set_open_close_group(i, _group_info[i].close_rel >= 0);
  }
  else if(_group_info[i].first >= 0) {
    set_open_close_group(i, _group_info[_group_info[i].first].close_rel >= 0);
  }
}

Expr SectionList::get_group_style(int pos, ObjectStyleOptionName n, Expr result) {
  if(pos >= length())
    pos = length() - 1;
    
  while(pos >= 0) {
    Expr value = _sections[pos]->get_own_style(n, Symbol(PMATH_UNDEFINED));
    if(value != PMATH_UNDEFINED)
      return value;
      
    pos = group_info(pos).first;
  }
  
  return get_style(n, result); // get_own_style
}

void SectionList::internal_insert_pmath(int *pos, Expr boxes, int overwrite_until_index) {
  if(overwrite_until_index > _sections.length())
    overwrite_until_index  = _sections.length();
    
  if( boxes[0]    == PMATH_SYMBOL_SECTIONGROUP &&
      boxes[1][0] == PMATH_SYMBOL_LIST)
  {
    Expr sect = boxes[1];
    Expr open = boxes[2];
    
    int start     = *pos;
    int opened    = 0;
    int close_rel = -1;
    if(open.is_int32()) {
      opened = PMATH_AS_INT32(open.get());
    }
    
    if(*pos < overwrite_until_index) {
      int e = _group_info[*pos].end;
      
      if(e >= *pos && e < overwrite_until_index) {
        internal_remove(e + 1, overwrite_until_index);
        overwrite_until_index = e + 1;
      }
    }
    
    for(size_t i = 1; i <= sect.expr_length(); ++i) {
      if(--opened == 0) {
        close_rel = *pos - start;
      }
      
      internal_insert_pmath(pos, sect[i], overwrite_until_index);
    }
    
    if(start < _sections.length() && close_rel >= 0) {
      _group_info[start].close_rel = close_rel;
    }
  }
  else {
    Section *section;
    
    if(*pos < overwrite_until_index) {
      section = _sections[*pos];
      
      if(section->try_load_from_object(boxes, BoxOptionDefault)) {
        _group_info[*pos].precedence = section->get_own_style(SectionGroupPrecedence, 0.0);
        
        ++*pos;
        internal_remove(*pos, overwrite_until_index);
        return;
      }
    }
    
    section = Section::create_from_object(boxes);
    if(section) {
      //insert(*pos, s);
      _sections.insert(*pos, 1, &section);
      adopt(section, *pos);
      
      SectionGroupInfo sgi;
      sgi.precedence = section->get_own_style(SectionGroupPrecedence, 0.0);
      sgi.close_rel  = -1; // open
      _group_info.insert(*pos, 1, &sgi);
      
      ++*pos;
      ++overwrite_until_index;
    }
    
    internal_remove(*pos, overwrite_until_index);
  }
}

void SectionList::insert_pmath(int *pos, Expr boxes, int overwrite_until_index) {
  assert(pos != nullptr);
  assert(*pos >= 0);
  
  int start = *pos;
  
  internal_insert_pmath(pos, boxes, overwrite_until_index);
  
  for(int i = start; i < _sections.length(); ++i)
    adopt(_sections[i], i);
    
  recalc_group_info();
  update_group_nesting();
  update_section_visibility();
  invalidate();
}

void SectionList::insert(int pos, Section *section) {
  _sections.insert(pos, 1, &section);
  
  for(int i = pos; i < _sections.length(); ++i)
    adopt(_sections[i], i);
    
  SectionGroupInfo sgi;
  sgi.precedence = section->get_own_style(SectionGroupPrecedence, 0.0);
  _group_info.insert(pos, 1, &sgi);
  
  recalc_group_info();
  update_group_nesting();
  set_open_close_group(pos, true);
  invalidate();
}

Section *SectionList::swap(int pos, Section *section) {
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

void SectionList::internal_remove(int start, int end) {
  if(end <= start)
    return;
    
  for(int i = start; i < end; ++i)
    _sections[i]->safe_destroy();
    
  if(end < _sections.length()) {
    double y = 0;
    if(start > 0)
      y = _sections[start - 1]->y_offset;
      
    y = _sections[end]->y_offset - y;
    for(int i = end; i < _sections.length(); ++i)
      _sections[i]->y_offset -= y;
  }
  
  _sections.remove(start, end - start);
  _group_info.remove(start, end - start);
}


void SectionList::remove(int start, int end) {
  internal_remove(start, end);
  
  for(int i = start; i < _sections.length(); ++i)
    adopt(_sections[i], i);
    
  recalc_group_info();
  update_group_nesting();
  invalidate();
}

Box *SectionList::remove(int *index) {
  remove(*index, *index + 1);
  return this;
}

void SectionList::recalc_group_info() {
  int pos = 0;
  while(pos < _group_info.length()) {
    _group_info[pos].first = -1;
    recalc_group_info_part(&pos);
  }
}

void SectionList::recalc_group_info_part(int *pos) {
  int start = *pos;
  ++*pos;
  
  while( *pos < _group_info.length() &&
         _group_info[start].precedence < _group_info[*pos].precedence)
  {
    _group_info[*pos].first = start;
    recalc_group_info_part(pos);
  }
  
  _group_info[start].end = *pos - 1;
}

void SectionList::update_group_nesting() {
  int pos = 0;
  while(pos < _group_info.length())
    update_group_nesting_part(&pos, 1);
}

void SectionList::update_group_nesting_part(int *pos, int current_nesting) {
  int end = _group_info[*pos].end;
  
  if(*pos < end)
    ++current_nesting;
    
  if(_group_info[*pos].nesting != current_nesting) {
    if(_sections[*pos]->get_own_style(LineBreakWithin, true)) {
      _sections[*pos]->invalidate();
    }
  }
  _group_info[*pos].nesting = current_nesting;
  
  ++*pos;
  while(*pos <= end)
    update_group_nesting_part(pos, current_nesting);
}

void SectionList::update_section_visibility() {
  int pos = 0;
  while(pos < _sections.length()) {
    if(_group_info[pos].end != pos) {
      int start = pos;
      if(_group_info[start].close_rel >= 0 &&
          _group_info[start].close_rel <= _group_info[start].end - start)
      {
        while(pos < start + _group_info[start].close_rel) {
          if(_sections[pos]->visible) {
            invalidate();
            _sections[pos]->visible = false;
          }
          ++pos;
        }
        
        if(!_sections[pos]->visible) {
          invalidate();
          _sections[pos]->visible = true;
        }
        
        ++pos;
        while(pos <= _group_info[start].end) {
          if(_sections[pos]->visible) {
            invalidate();
            _sections[pos]->visible = false;
          }
          ++pos;
        }
        
        continue;
      }
      
      _group_info[start].close_rel = -1;
    }
    
    if(!_sections[pos]->visible) {
      invalidate();
      _sections[pos]->visible = true;
    }
    ++pos;
  }
}

bool SectionList::request_repaint_range(int start, int end) {
  float y1 = 0;
  
  if(start > _sections.length())
    start = _sections.length();
    
  if(end > _sections.length())
    end = _sections.length();
    
  if(start < _sections.length())
    y1 = _sections[start]->y_offset;
  else
    y1 = _extents.descent;
//  else if(start > 0)
//    y1 = _sections[start - 1]->y_offset + _sections[start - 1]->extents().height();

  float y2 = y1;
  
  if(end < _sections.length())
    y2 = _sections[end]->y_offset;
  else
    y2 = _extents.descent;
//  else if(end > 0)
//    y2 = _sections[end - 1]->y_offset + _sections[end - 1]->extents().height();

  return request_repaint(0, y1, _extents.width, y2 - y1);
  
}

void SectionList::init_section_bracket_sizes(Context *context) {
  float dx = 1;
  float dy = 0;
  
  context->canvas->user_to_device_dist(&dx, &dy);
  float pix = 1 / sqrt(dx * dx + dy * dy);
  
  section_bracket_width        = 8 * pix;
  section_bracket_right_margin = 2 * pix;
}

void SectionList::resize_section(Context *context, int i) {
  float old_w    = context->width;
  float old_scww = context->section_content_window_width;
  
  if(get_own_style(ShowSectionBracket, true)) {
    context->width                        -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
    context->section_content_window_width -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
  }
  
  _sections[i]->resize(context);
  
  context->width                        = old_w;
  context->section_content_window_width = old_scww;
}

float SectionList::get_content_scroll_correction_x(int i) {
  float ssx = _scrollx;
  
  float content_window_width = _window_width;
  
  if(get_own_style(ShowSectionBracket, true)) {
    content_window_width -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
  }
  
  if(ssx > _sections[i]->extents().width - content_window_width)
    ssx =  _sections[i]->extents().width - content_window_width;
    
  if(ssx < 0)
    ssx =  0;
    
  return ssx - _scrollx;
}

void SectionList::paint_section(Context *context, int i) {
  if(_sections[i]->must_resize)
    _sections[i]->resize(context);
    
  float old_w    = context->width;
  float old_scww = context->section_content_window_width;
  //_scrollx = scrollx;
  
  if(get_own_style(ShowSectionBracket, true)) {
    context->width                        -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
    context->section_content_window_width -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
  }
  
  context->width                        = old_w;
  context->section_content_window_width = old_scww;
  
  if(!_sections[i]->visible)
    return;
    
  float scroll_cor_x = get_content_scroll_correction_x(i);
  
  float x, y;
  context->canvas->current_pos(&x, &y);
  
  float w = context->width;
  if(w == HUGE_VAL) {
    w = _window_width;
  }
  
  if(get_own_style(ShowSectionBracket, true)) {
    w -= section_bracket_right_margin + section_bracket_width * _group_info[i].nesting;
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
    
    context->canvas->move_to(x - scroll_cor_x, y);
    
    Expr expr;
    context->stylesheet->get(style, TextShadow, &expr);
    context->draw_with_text_shadows(_sections[i], expr);
    
  }
  context->canvas->restore();
  paint_section_brackets(context, i, x + _scrollx + _window_width, y);
  
  context->canvas->move_to(x, y + _sections[i]->extents().height());
}

void SectionList::paint_section_brackets(Context *context, int i, float right, float top) {
  if(get_own_style(ShowSectionBracket, true)) {
    int style = BorderDefault;
    
    if(_sections[i]->evaluating)
      style = style + BorderEval;
      
    if(_sections[i]->dialog_start)
      style = style + BorderSession;
      
    if(!_sections[i]->get_style(Editable))
      style = style + BorderNoEditable;
      
    if(dynamic_cast<TextSection *>(_sections[i]))
      style = style + BorderText;
      
    if(_sections[i]->get_style(SectionGenerated))
      style = style + BorderOutput;
      
    if(_sections[i]->get_style(Evaluatable))
      style = style + BorderInput;
      
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
        && context->selection.end > i) {
      int start = i;
      if(_group_info[i].end > i)
        ++sel_depth;
        
      while(start >= context->selection.start
            && _group_info[start].end < context->selection.end) {
        ++sel_depth;
        start = _group_info[start].first;
      }
    }
    
    if(_sections[i]->get_style(ShowSectionBracket))
      paint_single_section_bracket(context, x1, y1, x2, y2, style);
      
    if(sel_depth-- == 0) {
      context->canvas->pixrect(x1, sel_y1, x2, sel_y2, false);
      context->draw_selection_path();
    }
    
    int s = i;
    int e = i;
    
    style = BorderDefault;
    int start = i;
    while(start >= 0) {
      int end = _group_info[start].end;
      
      if(start < end) {
        float pixel = section_bracket_width / 8;
        x1 = x2;
        x2 = x1 + section_bracket_width;
        x1 += pixel;
        
        if(_group_info[start].close_rel + start == s) {
          if(start < s)
            style |= BorderTopArrow;
            
          if(end > e)
            style |= BorderBottomArrow;
            
          s = start;
          e = end;
        }
        
        if(start < s && !(style & BorderTopArrow)) {
          y1 = sel_y1 = top;
          style |= BorderNoTop;
        }
        
        if(end > e && !(style & BorderBottomArrow)) {
          y2 = sel_y2 = top + _sections[i]->extents().height();
          style |= BorderNoBottom;
        }
        
        if(_sections[start]->get_style(ShowSectionBracket))
          paint_single_section_bracket(context, x1, y1, x2, y2, style);
          
        style = style & ~BorderTopArrow;
        style = style & ~BorderBottomArrow;
        
        if(sel_depth-- == 0) {
          context->canvas->save();
          if((style & BorderNoTop) || (style & BorderNoBottom)) {
            float clip_top    = sel_y1;
            float clip_bottom = sel_y2;
            
            if((style & BorderNoTop) == 0)
              clip_top -= pixel;
              
            if((style & BorderNoBottom) == 0)
              clip_bottom += pixel;
              
            context->canvas->pixrect(x1 - pixel, clip_top, x2 + pixel, clip_bottom, false);
            context->canvas->clip();
          }
          
          context->canvas->pixrect(x1, sel_y1, x2, sel_y2, false);
          context->draw_selection_path();
          context->canvas->restore();
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
) {
  float px1 = x1 + 0.2 * (x2 - x1);
  float py1 = y1;
  float px2 = x1 + 0.8 * (x2 - x1);
  float py2 = y1;
  float px3 = px2;
  float py3 = y2 - 0.75;
  float px4 = px1;
  float py4 = py3;
  
  float pdxx = 1;
  float pdxy = 0;
  float pdyx = 0;
  float pdyy = 1;
  
  context->canvas->save();
  int c = context->canvas->get_color();
  {
    cairo_set_line_width(context->canvas->cairo(), 1);
    cairo_set_line_cap(context->canvas->cairo(), CAIRO_LINE_CAP_SQUARE);
    cairo_set_line_join(context->canvas->cairo(), CAIRO_LINE_JOIN_MITER);
    
    context->canvas->user_to_device(&px1, &py1);
    context->canvas->user_to_device(&px2, &py2);
    context->canvas->user_to_device(&px3, &py3);
    context->canvas->user_to_device(&px4, &py4);
    
    context->canvas->user_to_device_dist(&pdxx, &pdxy);
    context->canvas->user_to_device_dist(&pdyx, &pdyy);
    
    float picy = 1;
    
    float hyp = sqrt(pdxx * pdxx + pdxy * pdxy);
    pdxx /= hyp;
    pdxy /= hyp;
    
    hyp = sqrt(pdyx * pdyx + pdyy * pdyy);
    pdyx /= hyp;
    pdyy /= hyp;
    
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
    if(style & BorderTopArrow) {
      context->canvas->move_to(px2, py2);
      context->canvas->line_to(px1, py1 + (px2 - px1));
      context->canvas->line_to(px2, py2 + (px2 - px1));
      context->canvas->close_path();
      context->canvas->fill_preserve();
      context->canvas->stroke();
      style |= BorderNoTop;
    }
    
    if(style & BorderBottomArrow) {
      context->canvas->move_to(px3, py3);
      context->canvas->line_to(px4, py4 - (px2 - px1));
      context->canvas->line_to(px3, py3 - (px2 - px1));
      context->canvas->close_path();
      context->canvas->fill_preserve();
      context->canvas->stroke();
      style |= BorderNoBottom;
    }
    
    if(style & BorderNoTop) {
      context->canvas->move_to(px2, py2);
    }
    else {
      if(style & BorderNoEditable) {
        context->canvas->move_to(px1 + pdyx, py1 + pdyy);
        context->canvas->line_to(px2 + pdyx, py2 + pdyy);
        
        picy += 1;
      }
      
      context->canvas->move_to(px1, py1);
      context->canvas->line_to(px2, py2);
    }
    
    context->canvas->line_to(px3, py3);
    
    if(!(style & BorderNoBottom)) {
      context->canvas->line_to(px4, py4);
    }
    
    if(style & BorderSession) {
      double dashes[2] = {
        1.0,  /* ink */
        1.0   /* skip*/
      };
      
      cairo_set_dash(context->canvas->cairo(), dashes, 2, 0.5);
      cairo_set_line_cap(context->canvas->cairo(), CAIRO_LINE_CAP_BUTT);
      cairo_set_line_width(context->canvas->cairo(), 3.0);
      
      context->canvas->set_color(0x000000);
      context->canvas->stroke_preserve();
      
      cairo_set_dash(context->canvas->cairo(), nullptr, 0, 0.0);
    }
    else if(style & BorderEval) {
      cairo_set_line_width(context->canvas->cairo(), 3.0);
      
      context->canvas->set_color(0x000000);
      context->canvas->stroke_preserve();
      
      //col = 0xffffff & ~col;
    }
    
    cairo_set_line_width(context->canvas->cairo(), 1.0);
    context->canvas->set_color(col);
    context->canvas->stroke();
    
    if(style & BorderInput) {
      float cx = px2 + (picy + 1) * pdyx - 3 * pdxx;
      float cy = py2 + (picy + 1) * pdyy - 3 * pdxy;
      
      context->canvas->move_to(cx, cy);
      context->canvas->line_to(cx + 3 * pdyx, cy + 3 * pdyy);
      context->canvas->stroke();
      
      picy += 5;
    }
    
    if(style & BorderText) {
      float cx = px2 + (picy + 1) * pdyx - 3 * pdxx;
      float cy = py2 + (picy + 1) * pdyy - 3 * pdxy;
      
      context->canvas->move_to(cx, cy);
      context->canvas->line_to(cx + 3 * pdyx, cy + 3 * pdyy);
      context->canvas->move_to(cx - pdxx, cy - pdxy);
      context->canvas->line_to(cx + pdxx, cy + pdxy);
      
      context->canvas->stroke();
      
      picy += 5;
    }
    
    if(style & BorderOutput) {
      float cx = px2 + (picy + 2) * pdyx - 3 * pdxx;
      float cy = py2 + (picy + 2) * pdyy - 3 * pdxy;
      
      context->canvas->arc(cx, cy, 1, 0, 2 * M_PI, false);
      context->canvas->stroke();
      
      picy += 4;
    }
  }
  context->canvas->set_color(c);
  context->canvas->restore();
}

//} ... class SectionList
