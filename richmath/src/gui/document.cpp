#include <gui/document.h>

#include <cmath>
#include <cstdio>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/buttonbox.h>
#include <boxes/fractionbox.h>
#include <boxes/gridbox.h>
#include <boxes/radicalbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/textsequence.h>
#include <boxes/underoverscriptbox.h>
#include <eval/binding.h>
#include <eval/application.h>
#include <gui/clipboard.h>
#include <gui/native-widget.h>

using namespace richmath;

bool richmath::DebugFollowMouse = false;

static double MaxFlashingCursorRadius = 9;  /* pixels */
static double MaxFlashingCursorTime = 0.15; /* seconds */

Hashtable<String, Expr, object_hash> richmath::global_immediate_macros;
Hashtable<String, Expr, object_hash> richmath::global_macros;

Box *richmath::expand_selection(Box *box, int *start, int *end) {
  if(!box)
    return 0;
    
  if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
    for(int i = *start; i < *end; ++i) {
      if(seq->span_array().is_token_end(i))
        goto MULTIPLE_TOKENS;
    }
    
    if( *start == *end &&
        *start > 0 &&
        !seq->span_array().is_operand_start(*start))
    {
      --*start;
      --*end;
    }
    
    while(*start > 0 && !seq->span_array().is_token_end(*start - 1))
      --*start;
      
    while(*end < seq->length() && !seq->span_array().is_token_end(*end))
      ++*end;
      
    if(*end < seq->length())
      ++*end;
    return seq;
    
  MULTIPLE_TOKENS:
    if(*start < seq->length()) {
      Span s = seq->span_array()[*start];
      
      if(s) {
        int e = s.end();
        while(s && s.end() >= *end) {
          e = s.end();
          s = s.next();
        }
        
        if(e >= *end) {
          *end = e + 1;
          return seq;
        }
      }
    }
    
    int a = *start;
    while(--a >= 0) {
      Span s = seq->span_array()[a];
      
      if(s) {
        int e = s.end();
        while(s && s.end() + 1 >= *end) {
          e = s.end();
          s = s.next();
        }
        
        if(e + 1 >= *end) {
          *start = a;
          *end = e + 1;
          return seq;
        }
      }
    }
    
    if( *start > 0 ||
        *end < seq->length())
    {
      *start = 0;
      *end = seq->length();
      return seq;
    }
  }
  else if(TextSequence *seq = dynamic_cast<TextSequence *>(box)) {
    if(*start > 0 || *end < seq->length()) {
      PangoLogAttr *attrs;
      int n_attrs;
      pango_layout_get_log_attrs(seq->get_layout(), &attrs, &n_attrs);
      
      const char *buf = seq->text_buffer().buffer();
      const char *s              = buf;
      const char *s_end          = buf + seq->length();
      const char *word_start     = buf;
      
      int i = 0;
      while(s && (size_t)s - (size_t)buf <= (size_t)*start) {
        if(attrs[i].is_word_start)
          word_start = s;
          
        ++i;
        s = g_utf8_find_next_char(s, s_end);
      }
      
      const char *word_end = NULL;
      
      while(s && !word_end) {
        if(attrs[i].is_word_boundary && word_end != word_start)
          word_end = s;
          
        ++i;
        s = g_utf8_find_next_char(s, s_end);
      }
      
      g_free(attrs);
      attrs = NULL;
      
      if(!word_end)
        word_end = s_end;
        
      if( (size_t)word_end - (size_t)buf       >= (size_t)*end &&
          (size_t)word_end - (size_t)word_start > (size_t)*end - (size_t)*start)
      {
        *start = (int)((size_t)word_start - (size_t)buf);
        *end   = (int)((size_t)word_end   - (size_t)buf);
      }
      else {
        GSList *lines = pango_layout_get_lines_readonly(seq->get_layout());
        
        int prev_par_start = 0;
        int paragraph_start = 0;
        while(lines) {
          PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
          
          if(line->is_paragraph_start && line->start_index <= *start) {
            prev_par_start = paragraph_start;
            paragraph_start = line->start_index;
          }
          
          if(line->start_index + line->length >= *end) {
            if(line->start_index <= *start && *end - *start < line->length) {
              *start = line->start_index;
              *end = line->start_index + line->length;
              break;
            }
            
            int old_end = *end;
            
            lines = lines->next;
            while(lines) {
              PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
              if(line->is_paragraph_start && line->start_index >= *end) {
                *end = line->start_index;
                break;
              }
              
              lines = lines->next;
            }
            
            if(!lines)
              *end = seq->length();
              
            if(old_end - *start < *end - paragraph_start)
              *start = paragraph_start;
            else
              *start = prev_par_start;
              
            break;
          }
          
          lines = lines->next;
        }
      }
      
      return seq;
    }
  }
  
  int index = box->index();
  Box *box2 = box->parent();
  while(box2) {
    if(dynamic_cast<AbstractSequence *>(box2)) {
      *start = index;
      *end = index + 1;
      return box2;
    }
    
    index = box2->index();
    box2 = box2->parent();
  }
  
  return box;
}

int richmath::box_depth(Box *box) {
  int result = 0;
  while(box) {
    ++result;
    box = box->parent();
  }
  return result;
}

int richmath::box_order(Box *b1, int i1, Box *b2, int i2) {
  int od1, od2, d1, d2;
  od1 = d1 = box_depth(b1);
  od2 = d2 = box_depth(b2);
  
  while(d1 > d2) {
    i1 = b1->index();
    b1 = b1->parent();
    --d1;
  }
  
  while(d2 > d1) {
    i2 = b2->index();
    b2 = b2->parent();
    --d2;
  }
  
  while(b1 != b2 && b1 && b2) {
    i1 = b1->index();
    b1 = b1->parent();
    
    i2 = b2->index();
    b2 = b2->parent();
  }
  
  if(i1 == i2)
    return od1 - od2;
    
  return i1 - i2;
}

static void selection_path(
  Canvas  *canvas,
  Box     *box,
  int      start,
  int      end
) {
  if(box) {
    canvas->save();
    
    cairo_matrix_t mat;
    cairo_matrix_init_identity(&mat);
    box->transformation(0, &mat);
    
    canvas->transform(mat);
    
    canvas->move_to(0, 0);
    box->selection_path(canvas, start, end);
    
    canvas->restore();
  }
}

static MathSequence *search_string(
  Box *box,
  int *index,
  Box *stop_box,
  int  stop_index,
  const String string,
  bool complete_token
) {
  MathSequence *seq = dynamic_cast<MathSequence *>(box);
  
  if(seq) {
    const uint16_t *buf = string.buffer();
    int             len = string.length();
    
    const uint16_t *seqbuf = seq->text().buffer();
    int             seqlen = seq->text().length();
    
    if(stop_box == seq)
      seqlen = stop_index;
      
    int i = *index;
    for(; i <= seqlen - len; ++i) {
      if(complete_token) {
        if( (i == 0 || seq->span_array().is_token_end(i - 1)) &&
            seq->span_array().is_token_end(i + len - 1))
        {
          int j = 0;
          
          for(; j < len; ++j) {
            if( seqbuf[i + j] != buf[j] ||
                (seq->span_array().is_token_end(i + j) && j < len - 1))
            {
              break;
            }
          }
          
          if(j == len) {
            *index = i + j;
            return seq;
          }
        }
      }
      else {
        int j = 0;
        
        for(; j < len; ++j)
          if(seqbuf[i + j] != buf[j])
            break;
            
        if(j == len) {
          *index = i + j;
          return seq;
        }
      }
      
      if(seqbuf[i] == PMATH_CHAR_BOX) {
        int b = 0;
        while(box->item(b)->index() < i)
          ++b;
          
        *index = 0;
        return search_string(
                 box->item(b),
                 index,
                 stop_box,
                 stop_index,
                 string,
                 complete_token);
      }
    }
    
    for(; i < seqlen; ++i) {
      if(seqbuf[i] == PMATH_CHAR_BOX) {
        int b = 0;
        while(box->item(b)->index() < i)
          ++b;
          
        *index = 0;
        return search_string(
                 box->item(b),
                 index,
                 stop_box,
                 stop_index,
                 string,
                 complete_token);
      }
    }
  }
  else if(box == stop_box) {
    if(*index >= stop_index || *index >= box->count())
      return 0;
      
    int i = *index;
    *index = 0;
    return search_string(
             box->item(i),
             index,
             stop_box,
             stop_index,
             string,
             complete_token);
  }
  else if(*index < box->count()) {
    int i = *index;
    *index = 0;
    return search_string(
             box->item(i),
             index,
             stop_box,
             stop_index,
             string,
             complete_token);
  }
  
  if(box->parent()) {
    *index = box->index() + 1;
    return search_string(
             box->parent(),
             index,
             stop_box,
             stop_index,
             string,
             complete_token);
  }
  
  *index = 0;
  return 0;
}

static bool selection_is_name(Document *doc) {
  if(doc->selection_length() <= 0)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(doc->selection_box());
  if(!seq)
    return false;
    
  const uint16_t *buf = seq->text().buffer();
  for(int i = doc->selection_start(); i < doc->selection_end(); ++i) {
    if( !pmath_char_is_digit(buf[i]) &&
        !pmath_char_is_name(buf[i]))
    {
      return false;
    }
    
    if( seq->span_array().is_token_end(i) &&
        i < doc->selection_end() - 1)
    {
      return false;
    }
  }
  
  return true;
}

//{ class Document ...

Document::Document()
  : SectionList(),
    main_document(0),
    best_index_rel_x(0),
    prev_sel_line(-1),
    prev_sel_box_id(0),
    must_resize_min(0),
    drag_status(DragStatusIdle),
    auto_scroll(false),
    _native(NativeWidget::dummy),
    mouse_down_counter(0),
    mouse_down_time(0),
    mouse_down_x(0),
    mouse_down_y(0)
{
  context.selection.set(this, 0, 0);
  context.math_shaper = MathShaper::available_shapers.default_value;
  context.text_shaper = context.math_shaper;
  context.stylesheet  = Stylesheet::Default;
  
  context.set_script_size_multis(Expr());
  
  reset_style();
}

Document::~Document() {
}

bool Document::try_load_from_object(Expr expr, int options) {
  if(expr[0] != PMATH_SYMBOL_DOCUMENT)
    return false;
    
  Expr sections_expr = expr[1];
  if(sections_expr[0] != PMATH_SYMBOL_LIST)
    return false;
    
  Expr options_expr(pmath_options_extract(expr.get(), 1));
  if(!options_expr.is_valid())
    return false;
    
  sections_expr = Call(Symbol(PMATH_SYMBOL_SECTIONGROUP), sections_expr, Symbol(PMATH_SYMBOL_ALL));
  
  int pos = 0;
  insert_pmath(&pos, sections_expr, count());
  
  reset_style();
  style->add_pmath(options_expr);
  return true;
}

bool Document::request_repaint(float x, float y, float w, float h) {
  float wx, wy, ww, wh;
  native()->scroll_pos(&wx, &wy);
  native()->window_size(&ww, &wh);
  
  if( x + w >= wx && x <= wx + ww &&
      y + h >= wy && y <= wy + wh)
  {
    native()->invalidate_rect(x, y, w, h);
    return true;
  }
  
  return false;
}

void Document::invalidate() {
  native()->invalidate();
  
  Box::invalidate();
}

void Document::invalidate_options() {
  if(get_own_style(InternalHasModifiedWindowOption)) {
    style->set(InternalHasModifiedWindowOption, false);
    
    native()->invalidate_options();
  }
  
  Box::invalidate_options();
}

void Document::invalidate_all() {
  must_resize_min = count();
  for(int i = 0; i < length(); ++i)
    section(i)->invalidate();
}

void Document::scroll_to(float x, float y, float w, float h) {
  if(!native()->is_scrollable())
    return;
    
  float _w, _h, _x, _y;
  native()->window_size(&_w, &_h);
  native()->scroll_pos(&_x, &_y);
  
  if(y < _y) {
    _y = y - _h / 6.f;
  }
  else if(y + h >= _y + _h) {
    _y = y + h - _h * 5 / 6.f;
  }
  
  if(x < _x) {
    _x = x - _w / 6.f;
  }
  else if(x + w >= _x + _w) {
    _x = x + w - _w * 5 / 6.f;
    if(x < _x)
      _x = x;
  }
  
  native()->scroll_to(_x, _y);
}

void Document::scroll_to(Canvas *canvas, Box *child, int start, int end) {
  default_scroll_to(canvas, this, child, start, end);
}

//{ event invokers ...

void Document::mouse_exit() {
  if(context.mouseover_box_id) {
    Box *over = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
    
    while(over && over != this) {
      over->on_mouse_exit();
      over = over->parent();
    }
    
    context.mouseover_box_id = 0;
  }
  
  if(DebugFollowMouse) {
    mouse_move_sel.reset();
    invalidate();
  }
}

void Document::mouse_down(MouseEvent &event) {
  Box *receiver = 0;
  
  Application::delay_dynamic_updates(true);
  if(++mouse_down_counter == 1) {
    event.set_source(this);
    
    bool was_inside_start;
    int start, end;
    receiver = mouse_selection(
                 event.x, event.y,
                 &start, &end,
                 &was_inside_start);
                 
    receiver = receiver ? receiver->mouse_sensitive() : this;
    assert(receiver != 0);
    context.clicked_box_id = receiver->id();
  }
  else {
    receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id);
    
    if(!receiver) {
      context.clicked_box_id = this->id();
      receiver = this;
    }
  }
  
  finish_editing(receiver);
  receiver->on_mouse_down(event);
}

void Document::mouse_up(MouseEvent &event) {
  int next_clicked_box_id = context.clicked_box_id;
  Box *receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id);
  
  if(--mouse_down_counter <= 0) {
    Application::delay_dynamic_updates(false);
    next_clicked_box_id = 0;
    mouse_down_counter = 0;
  }
  
  if(receiver)
    receiver->on_mouse_up(event);
    
  context.clicked_box_id = next_clicked_box_id;
}

void Document::finish_editing(Box *except_box) {
  Box *b1 = selection_box();
  Box *b2 = except_box;
  int d1 = box_depth(b1);
  int d2 = box_depth(b2);
  
  while(d1 > d2) {
    b1 = b1->parent();
    --d1;
  }
  
  while(d2 > d1) {
    b2 = b2->parent();
    --d2;
  }
  
  if(b1 == b2)
    return;
    
  while(b1 != b2 && b1 && b2) {
    b1 = b1->parent();
    b2 = b2->parent();
  }
  
  b1 = selection_box();
  while(b1 && b1 != b2) {
    b1->on_finish_editing();
    b1 = b1->parent();
  }
}

static void reverse_mouse_enter(Box *base, Box *child) {
  if(child && child != base) {
    reverse_mouse_enter(base, child->parent());
    child->on_mouse_enter();
  }
}

void Document::mouse_move(MouseEvent &event) {
  Box *receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id);
  
  if(receiver) {
    native()->set_cursor(CurrentCursor);
    
    receiver->on_mouse_move(event);
  }
  else {
    event.set_source(this);
    
    int start, end;
    bool was_inside_start;
    Box *receiver = mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
    
    if(DebugFollowMouse && !mouse_move_sel.equals(receiver, start, end)) {
      mouse_move_sel.set(receiver, start, end);
      invalidate();
    }
    
    //Box *new_over = receiver ? receiver->mouse_sensitive() : 0;
    Box *old_over = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
    
    if(receiver) {
      Box *base = Box::common_parent(receiver, old_over);
      Box *box = old_over;
      while(box != base) {
        box->on_mouse_exit();
        box = box->parent();
      }
      
      reverse_mouse_enter(base, receiver);
      
      box = receiver->mouse_sensitive();
      if(box)
        box->on_mouse_move(event);
        
      context.mouseover_box_id = receiver->id();
    }
    else
      context.mouseover_box_id = 0;
      
//    if(new_over != old_over){
//      if(old_over)
//        old_over->on_mouse_exit();
//
//      if(new_over)
//        new_over->on_mouse_enter();
//    }
//
//    if(new_over){
//      context.mouseover_box_id = new_over->id();
//
//      new_over->on_mouse_move(event);
//    }
//    else
//      context.mouseover_box_id = 0;
  }
}

void Document::focus_set() {
  context.active = true;
  
  if(selection_box()) {
    selection_box()->on_enter();
    
    if(selection_length() > 0)
      selection_box()->request_repaint_range(selection_start(), selection_end());
  }
}

void Document::focus_killed() {
  context.active = false;
  reset_mouse();
  
  Box *sel = selection_box();
  
  if(sel) {
    sel->on_exit();
    
    if(!sel->selectable())
      select(0, 0, 0);
    else if(selection_length() > 0)
      sel->request_repaint_range(selection_start(), selection_end());
  }
}

void Document::key_down(SpecialKeyEvent &event) {
  native()->hide_tooltip();
  
  Box *selbox = context.selection.get();
  if(selbox) {
    selbox->on_key_down(event);
  }
  else {
    Document *cur = get_current_document();
    if(cur && cur != this)
      cur->key_down(event);
  }
}

void Document::key_up(SpecialKeyEvent &event) {
  Box *selbox = context.selection.get();
  if(selbox) {
    selbox->on_key_up(event);
  }
  else {
    Document *cur = get_current_document();
    if(cur && cur != this)
      cur->key_up(event);
  }
}

void Document::key_press(uint16_t unicode) {
  native()->hide_tooltip();
  
  if(unicode == '\r') {
    unicode = '\n';
  }
  else if( (unicode < ' ' && unicode != '\n' && unicode != '\t') ||
           unicode == 127                                        ||
           unicode == PMATH_CHAR_BOX)
  {
    return;
  }
  
  Box *selbox = context.selection.get();
  if(selbox) {
    selbox->on_key_press(unicode);
  }
  else {
    Document *cur = get_current_document();
    if(cur && cur != this)
      cur->key_press(unicode);
    else
      native()->beep();
  }
}

//} ... event invokers

//{ event handlers ...

void Document::on_mouse_down(MouseEvent &event) {
  native()->hide_tooltip();
  
  event.set_source(this);
  
  drag_status = DragStatusIdle;
  if(event.left) {
    float ddx, ddy;
    native()->double_click_dist(&ddx, &ddy);
    
    bool double_click =
      abs(mouse_down_time - native()->message_time()) <= native()->double_click_time() &&
      fabs(event.x - mouse_down_x) <= ddx &&
      fabs(event.y - mouse_down_y) <= ddy;
      
    mouse_down_time = native()->message_time();
    
    bool was_inside_start;
    int start, end;
    Box *box = mouse_selection(
                 event.x, event.y,
                 &start, &end,
                 &was_inside_start);
                 
    if(double_click) {
      Box *selbox = context.selection.get();
      if(selbox == this) {
        if(context.selection.start < context.selection.end) {
          toggle_open_close_group(context.selection.start);
          
          // prevent selection from changing in mouse_move():
          context.clicked_box_id = 0;
          
          // prevent "tripple-click"
          mouse_down_time = 0;
        }
      }
      else if(selbox && selbox->selectable()) {
        int start = context.selection.start;
        int end   = context.selection.end;
        
        bool should_expand = true;
        
        if(start == end) {
          if(was_inside_start) {
            if(end + 1 <= selbox->length()) {
              ++end;
              should_expand = false;
            }
          }
          else if(start > 0) {
            --start;
            should_expand = false;
          }
        }
        
        if(!should_expand) {
          should_expand = true;
          
          if(MathSequence *seq = dynamic_cast<MathSequence *>(selbox)) {
            should_expand = false;
            while(start > 0 && !seq->span_array().is_token_end(start - 1))
              --start;
              
            while(end < seq->length() && !seq->span_array().is_token_end(end - 1))
              ++end;
          }
        }
        
        if(should_expand)
          selbox = expand_selection(selbox, &start, &end);
          
        select(selbox, start, end);
      }
    }
    else if(is_inside_selection(box, start, end, was_inside_start)) {
      // maybe drag & drop
      drag_status = DragStatusMayDrag;
    }
    else if(box && box->selectable())
      select(box, start, end);
      
    mouse_down_x   = event.x;
    mouse_down_y   = event.y;
    mouse_down_sel = sel_first;
  }
}

void Document::on_mouse_move(MouseEvent &event) {
  event.set_source(this);
  
  int start, end;
  bool was_inside_start;
  Box *box = mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
  
  if(event.left && drag_status == DragStatusMayDrag) {
    float ddx, ddy;
    native()->double_click_dist(&ddx, &ddy);
    
    if( fabs(event.x - mouse_down_x) > ddx ||
        fabs(event.y - mouse_down_y) > ddy)
    {
      drag_status = DragStatusCurrentlyDragging;
      mouse_down_x = mouse_down_y = Infinity;
      native()->do_drag_drop(selection_box(), selection_start(), selection_end());
    }
    
    return;
  }
  
  if(drag_status == DragStatusCurrentlyDragging) {
    native()->set_cursor(CurrentCursor);
    return;
  }
  
  if(!event.left && is_inside_selection(box, start, end, was_inside_start)) {
    native()->set_cursor(DefaultCursor);
  }
  else if(box->selectable()) {
    if(box == this) {
      if(length() == 0)
        native()->set_cursor(TextNCursor);
      else if(start == end)
        native()->set_cursor(DocumentCursor);
      else
        native()->set_cursor(SectionCursor);
    }
    else
      native()->set_cursor(NativeWidget::text_cursor(box, start));
  }
  else if(dynamic_cast<Section *>(box) && selectable()) {
    native()->set_cursor(NoSelectCursor);
  }
  else
    native()->set_cursor(DefaultCursor);
    
  if(event.left && context.clicked_box_id) {
    Box *mouse_down_box = mouse_down_sel.get();
    
    if(mouse_down_box) {
      Section *sec1 = mouse_down_box->find_parent<Section>(true);
      Section *sec2 = box ? box->find_parent<Section>(true) : 0;
      
      if(sec1 && sec1 != sec2) {
        event.set_source(sec1);
        box = sec1->mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
      }
      
      select_range(
        mouse_down_box, mouse_down_sel.start, mouse_down_sel.end,
        box, start, end);
    }
  }
}

void Document::on_mouse_up(MouseEvent &event) {
  event.set_source(this);
  
  if(event.left && drag_status != DragStatusIdle) {
    bool was_inside_start;
    int start, end;
    Box *box = mouse_selection(
                 event.x, event.y,
                 &start, &end,
                 &was_inside_start);
                 
    if( is_inside_selection(box, start, end, was_inside_start) &&
        box &&
        box->selectable())
    {
      select(box, start, end);
    }
  }
  
  drag_status = DragStatusIdle;
}

void Document::on_mouse_cancel() {

}

void Document::on_key_down(SpecialKeyEvent &event) {
  if(native()->is_scrollable()) {
    switch(event.key) {
      case KeyPageUp: {
          float w, h;
          native()->window_size(&w, &h);
          native()->scroll_by(0, -h);
          
          event.key = KeyUnknown;
        } return;
        
      case KeyPageDown: {
          float w, h;
          native()->window_size(&w, &h);
          native()->scroll_by(0, h);
          
          event.key = KeyUnknown;
        } return;
        
      default: break;
    }
  }
  
  switch(event.key) {
    case KeyLeft: {
        if(event.shift) {
          move_horizontal(Backward, event.ctrl, true);
        }
        else if(selection_length() > 0) {
          Box *selbox = context.selection.get();
          
          if(selbox == this) {
            move_to(this, context.selection.end);
            move_horizontal(Backward, event.ctrl);
          }
          else if( dynamic_cast<GridBox *>(selbox) ||
                   (selbox &&
                    dynamic_cast<GridItem *>(selbox->parent()) &&
                    ((MathSequence *)selbox)->is_placeholder()))
          {
            move_horizontal(Backward, event.ctrl);
          }
          else
            move_to(selbox, context.selection.start);
        }
        else
          move_horizontal(Backward, event.ctrl);
          
        event.key = KeyUnknown;
      } return;
      
    case KeyRight: {
        if(event.shift) {
          move_horizontal(Forward, event.ctrl, true);
        }
        else if(selection_length() > 0) {
          Box *selbox = selection_box();
          
          if(selbox == this) {
            move_to(this, context.selection.start);
            move_horizontal(Forward, event.ctrl);
          }
          else if(dynamic_cast<GridBox *>(selbox) ||
                  (selbox &&
                   dynamic_cast<GridItem *>(selbox->parent()) &&
                   ((MathSequence *)selbox)->is_placeholder()))
          {
            move_horizontal(Forward, event.ctrl);
          }
          else
            move_to(selbox, context.selection.end);
        }
        else
          move_horizontal(Forward, event.ctrl);
          
        event.key = KeyUnknown;
      } return;
      
    case KeyHome: {
        move_start_end(Backward, event.shift);
        event.key = KeyUnknown;
      } return;
      
    case KeyEnd: {
        move_start_end(Forward, event.shift);
        event.key = KeyUnknown;
      } return;
      
    case KeyUp: {
        move_vertical(Backward, event.shift);
        event.key = KeyUnknown;
      } return;
      
    case KeyDown: {
        move_vertical(Forward, event.shift);
        event.key = KeyUnknown;
      } return;
      
    case KeyTab: {
        if(is_tabkey_only_moving())
          move_tab(event.shift ? Backward : Forward);
        else
          key_press('\t');
        event.key = KeyUnknown;
      } return;
      
    case KeyBackspace: {
        set_prev_sel_line();
        if(selection_length() > 0) {
          if(remove_selection(true))
            event.key = KeyUnknown;
          return;
        }
        
        Box *selbox = context.selection.get();
        if( context.selection.start == 0 &&
            selbox &&
            selbox->get_style(Editable) &&
            selbox->parent() &&
            selbox->parent()->exitable())
        {
          int index = selbox->index();
          selbox = selbox->parent()->remove(&index);
          
          move_to(selbox, index);
        }
        else if(event.ctrl || selbox != this) {
          int old = context.selection.id;
          MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
          
          if( seq &&
              seq->text()[context.selection.start - 1] == PMATH_CHAR_BOX)
          {
            int boxi = 0;
            while(seq->item(boxi)->index() < context.selection.start - 1)
              ++boxi;
              
            if(seq->item(boxi)->length() == 0) {
              move_horizontal(Backward, event.ctrl, true);
              event.key = KeyUnknown;
              return;
            }
            
            int old_sel_end = selection_end();
            move_horizontal(Backward, event.ctrl, event.ctrl || event.shift);
            
            if(selection_length() == 0 &&
                old == context.selection.id)
            {
              select(seq, selection_start(), old_sel_end);
              event.key = KeyUnknown;
              return;
            }
          }
          else
            move_horizontal(Backward, event.ctrl, true);
            
          if(!event.ctrl &&
              seq &&
              seq->text()[context.selection.start] == PMATH_CHAR_BOX)
          {
            event.key = KeyUnknown;
            return;
          }
          
          if(old == context.selection.id) {
            remove_selection(true);
            
            // reset sel_last:
            selbox = context.selection.get();
            select(selbox, context.selection.start, context.selection.end);
          }
        }
        else
          move_horizontal(Backward, event.ctrl);
          
        event.key = KeyUnknown;
      } return;
      
    case KeyDelete: {
        set_prev_sel_line();
        if(selection_length() > 0) {
          if(remove_selection(true))
            event.key = KeyUnknown;
          return;
        }
        
        Box *selbox = context.selection.get();
        if(event.ctrl || selbox != this) {
          int index = context.selection.start;
          Box *box = selbox->move_logical(Forward, event.ctrl, &index);
          
          while(box && !box->selectable()) {
            box = box->move_logical(Forward, true, &index);
          }
          
          if(box == selbox || box == selbox->parent()) {
            move_to(box, index, true);
            selbox = selection_box();
            
            if(!event.ctrl) {
              MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
              
              if(seq && seq->text()[context.selection.start] == PMATH_CHAR_BOX) {
                event.key = KeyUnknown;
                return;
              }
            }
            
            remove_selection(true);
          }
          else {
            Box *p = box;
            while(p && p != selbox)
              p = p->parent();
              
            if(p) {
              MathSequence *seq = dynamic_cast<MathSequence *>(box);
              
              if(seq && seq->is_placeholder(index)) {
                select(seq, index, index + 1);
              }
              else {
                int  old_start = selection_start();
                Box *old_box   = selection_box();
                
                move_to(box, index);
                
                if( selection_start() == old_start &&
                    selection_box()   == old_box)
                {
                  select(old_box, old_start, old_start + 1);
                }
              }
            }
            else
              native()->beep();
          }
        }
        else
          move_horizontal(Forward, event.ctrl);
          
        event.key = KeyUnknown;
      } return;
      
    case KeyEscape: {
        if(context.clicked_box_id) {
          Box *receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id);
          
          if(receiver)
            receiver->on_mouse_cancel();
            
          context.clicked_box_id = 0;
          event.key = KeyUnknown;
          return;
        }
        
        key_press(PMATH_CHAR_ALIASDELIMITER);
        event.key = KeyUnknown;
      } return;
      
    default: return;
  }
}

void Document::on_key_up(SpecialKeyEvent &event) {
}

void Document::on_key_press(uint32_t unichar) {
  AbstractSequence *initial_seq = dynamic_cast<AbstractSequence *>(selection_box());
  
  if(!prepare_insert()) {
    Document *cur = get_current_document();
    
    if(cur && cur != this)
      cur->key_press(unichar);
    else
      native()->beep();
      
    return;
  }
  
  if(unichar == '\n') {
    prev_sel_line = -1;
    
    // handle ReturnCreatesNewSection -> True ...
    if(initial_seq == selection_box()) {
      AbstractSequenceSection *sect = dynamic_cast<AbstractSequenceSection *>(initial_seq->parent());
      
      if(sect && sect->get_style(ReturnCreatesNewSection, false)) {
      
        if(selection_start() == initial_seq->length()) {
          AbstractSequenceSection *new_sect;
          SharedPtr<Style>         new_style = new Style();
          
          Expr new_style_expr = get_own_style(DefaultReturnCreatedSectionStyle, Symbol(PMATH_SYMBOL_AUTOMATIC));
          
          if(new_style_expr != PMATH_SYMBOL_AUTOMATIC)
            new_style->add_pmath(new_style_expr);
          else
            new_style->add_pmath(sect->get_own_style(BaseStyleName));
            
          String lang;
          if(context.stylesheet)
            context.stylesheet->get(new_style, LanguageCategory, &lang);
          else
            new_style->get(LanguageCategory, &lang);
            
          if(lang.equals("pMath"))
            new_sect = new MathSection(new_style);
          else
            new_sect = new TextSection(new_style);
            
          insert(sect->index() + 1, new_sect);
          move_to(sect->abstract_content(), 0);
          return;
        }
        
        split_section();
        return;
      }
    }
  }
  
  // handle parenthesis surrounding of selections:
  if( context.selection.start < context.selection.end &&
      unichar < 0xFFFF)
  {
    int prec;
    uint16_t ch = (uint16_t)unichar;
    String selstr;
    
    bool can_surround = true;
    if(TextSequence *seq = dynamic_cast<TextSequence *>(context.selection.get())) {
      selstr = String::FromUtf8(
                 seq->text_buffer().buffer() + context.selection.start,
                 context.selection.end - context.selection.start);
    }
    else if(MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get())) {
      selstr = seq->text().part(
                 context.selection.start,
                 context.selection.end - context.selection.start);
                 
      can_surround = !seq->is_placeholder(context.selection.start);
    }
    
    if(can_surround && selstr.length() == 1) {
      can_surround = !pmath_char_is_left( *selstr.buffer()) &&
                     !pmath_char_is_right(*selstr.buffer());
    }
    
    if(can_surround) {
      if(unichar == '/' && !selstr.starts_with("/*")) {
        AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get());
        if(seq) {
          seq->insert(context.selection.end,   "*/");
          seq->insert(context.selection.start, "/*");
          select(seq, context.selection.start, context.selection.end + 4);
          
          return;
        }
      }
      else if(pmath_char_is_left(unichar)) {
        uint32_t right = pmath_right_fence(unichar);
        AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get());
        
        if(seq && right && right < 0xFFFF) {
          seq->insert(context.selection.end,   right);
          seq->insert(context.selection.start, unichar);
          select(seq, context.selection.start, context.selection.end + 2);
          
          return;
        }
      }
      else if(PMATH_TOK_RIGHT == pmath_token_analyse(&ch, 1, &prec)) {
        uint32_t left = (uint32_t)((int)unichar + prec);
        AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get());
        
        if(seq && left && left < 0xFFFF) {
          seq->insert(context.selection.end,   ch);
          seq->insert(context.selection.start, left);
          select(seq, context.selection.start, context.selection.end + 2);
          
          return;
        }
      }
    }
  }
  
  remove_selection(false);
  
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get());
  if(seq) {
  
    // handle "CAPSLOCK alias CAPSLOCK" macros:
    if(unichar == PMATH_CHAR_ALIASDELIMITER) {
      int alias_end = context.selection.start;
      int alias_pos = alias_end - 1;
      
      while(alias_pos >= 0 && seq->char_at(alias_pos) != PMATH_CHAR_ALIASDELIMITER)
        --alias_pos;
        
      if(alias_pos >= 0) { // ==>  seq->char_at(alias_pos) == PMATH_CHAR_ALIASDELIMITER
        String alias = seq->raw_substring(alias_pos, alias_end - alias_pos).part(1);
        
        const uint16_t *buf = alias.buffer();
        const int       len = alias.length();
        
        if(len > 0 && buf[0] == '\\') {
          uint32_t unichar;
          const uint16_t *bufend = pmath_char_parse(buf, len, &unichar);
          
          if(unichar <= 0x10FFFF && bufend == buf + len) {
            String ins = String::FromChar(unichar);
            
            int i = seq->insert(alias_end, ins);
            seq->remove(alias_pos, alias_end);
            
            move_to(seq, i - (alias_end - alias_pos));
            return;
          }
        }
        else {
          Expr repl = String::FromChar(unicode_to_utf32(alias));
          
          if(repl.is_null())
            repl = global_immediate_macros[alias];
          if(repl.is_null())
            repl = global_macros[alias];
            
          if(!repl.is_null()) {
            String ins(repl);
            
            if(ins.is_valid()) {
              int i = seq->insert(alias_end, ins);
              seq->remove(alias_pos, alias_end);
              
              move_to(seq, i - (alias_end - alias_pos));
              return;
            }
            else {
              MathSequence *repl_seq = new MathSequence();
              repl_seq->load_from_object(repl, BoxOptionDefault);
              
              insert_box(repl_seq, true);
              int sel_start = selection_start();
              int sel_end   = selection_end();
              seq->remove(alias_pos, alias_end);
              
              if(selection_box() == seq) { // renormalizes selection_[start|end]()
                select(
                  seq,
                  sel_start - (alias_end - alias_pos),
                  sel_end   - (alias_end - alias_pos));
              }
              
              return;
            }
          }
        }
      }
    }
    
    MathSequence *mseq = dynamic_cast<MathSequence *>(seq);
    
    bool was_inside_string = is_inside_string();
    bool was_inside_alias  = is_inside_alias();
    
    if(!was_inside_string && !was_inside_alias) {
      handle_immediate_macros();
    }
    
    int newpos = seq->insert(context.selection.start, unichar);
    move_to(seq, newpos);
    
    if(mseq && !was_inside_string && !was_inside_alias) {
      // handle "\alias" macros:
      if(unichar == ' '/* && !was_inside_string*/) {
        context.selection.start--;
        context.selection.end--;
        
        bool ok = handle_macros();
        
        if( mseq == context.selection.get() &&
            context.selection.start < mseq->length() &&
            mseq->text()[context.selection.start] == ' ')
        {
          context.selection.end++;
          
          if(ok)
            remove_selection(false);
          else
            context.selection.start++;
        }
      }
      
      // handle "\[character-name]" macros:
      int i = context.selection.start - 1;
      if(i < 0)
        i = 0;
        
      const uint16_t *buf = mseq->text().buffer();
      while( i < mseq->length() &&
             buf[i] <= 0x7F &&
             buf[i] != ']' &&
             buf[i] != '[' &&
             i - context.selection.start < 64)
      {
        ++i;
      }
      
      if(i < mseq->length() && buf[i] == ']') {
        int start = context.selection.start;
        
        move_to(mseq, i + 1);
        context.selection.start = i + 1;
        context.selection.end = i + 1;
        
        if(!handle_macros())
          move_to(mseq, start);
      }
    }
  }
  else
    native()->beep();
}

//} ... event handlers

// substart and subend may lie outside 0..subbox->length()
bool Document::is_inside_selection(Box *subbox, int substart, int subend) {
  if( selection_box() &&
      selection_length() > 0)
  {
    // section selections are only at the right margin, the section content is
    // not inside the selection-frame
    if(selection_box() == this && subbox != this)
      return false;
      
    if(substart == subend)
      return false;
      
    Box *b = subbox;
    while(b && b != selection_box()) {
      substart = b->index();
      subend   = substart + 1;
      b = b->parent();
    }
    
    if( b == selection_box() &&
        selection_start() <= substart &&
        subend <= selection_end())
    {
      return true;
    }
  }
  
  return false;
}

bool Document::is_inside_selection(Box *subbox, int substart, int subend, bool was_inside_start) {
  if(subbox && subbox != this && substart == subend) {
    if(was_inside_start)
      subend = substart + 1;
    else
      --substart;
  }
  
  return selection_box() && is_inside_selection(subbox, substart, subend);
}


void Document::raw_select(Box *box, int start, int end) {
  if(end < start) {
    int i = start;
    start = end;
    end = i;
  }
  
  if(box)
    box = box->normalize_selection(&start, &end);
    
  Box *selbox = context.selection.get();
  if( selbox != box ||
      context.selection.start != start ||
      context.selection.end   != end)
  {
    Box *common_parent = Box::common_parent(selbox, box);
    
    Box *b = selbox;
    while(b != common_parent) {
      b->on_exit();
      b = b->parent();
    }
    
    b = box;
    while(b != common_parent) {
      b->on_enter();
      b = b->parent();
    }
    
    if(selection_box())
      selection_box()->request_repaint_range(selection_start(), selection_end());
      
    context.selection.set_raw(box, start, end);
    
    if(box) {
      box->request_repaint_range(start, end);
    }
  }
  
  best_index_rel_x = 0;
}

void Document::select(Box *box, int start, int end) {
  if(box && !box->selectable())
    return;
    
  sel_last.set(box, start, end);
  sel_first = sel_last;
  auto_scroll = !native()->is_mouse_down();//(mouse_down_counter == 0);
  
  raw_select(box, start, end);
}

void Document::select_to(Box *box, int start, int end) {
  if(box && !box->selectable())
    return;
    
  Box *first = sel_first.get();
  
  select_range(
    first,
    sel_first.start,
    sel_first.end,
    box,
    start,
    end);
}

void Document::select_range(
  Box *box1, int start1, int end1,
  Box *box2, int start2, int end2
) {
  if( (box1 && !box1->selectable()) ||
      (box2 && !box2->selectable()))
  {
    sel_first.set(0, 0, 0);
    sel_last = sel_first;
    raw_select(0, 0, 0);
    return;
  }
  
  sel_first.set(box1, start1, end1);
  sel_last.set( box2, start2, end2);
  auto_scroll = (mouse_down_counter == 0);
  
  if(end1 < start1) {
    int i = start1;
    start1 = end1;
    end1 = i;
  }
  
  if(end2 < start2) {
    int i = start2;
    start2 = end2;
    end2 = i;
  }
  
  Box *b1 = box1;
  Box *b2 = box2;
  int s1  = start1;
  int s2  = start2;
  int e1  = end1;
  int e2  = end2;
  int d1 = box_depth(b1);
  int d2 = box_depth(b2);
  
  while(d1 > d2) {
    if(b1->parent() && !b1->parent()->exitable()) {
      int o1 = box_order(b1, s1, b2, e2);
      int o2 = box_order(b1, e1, b2, s2);
      
      if(o1 > 0)
        raw_select(b1, 0, e1);
      else if(o2 < 0)
        raw_select(b1, s1, b1->length());
      else
        raw_select(b1, 0, b1->length());
        
      return;
    }
    s1 = b1->index();
    e1 = s1 + 1;
    b1 = b1->parent();
    --d1;
  }
  
  while(d2 > d1) {
    s2 = b2->index();
    e2 = s2 + 1;
    b2 = b2->parent();
    --d2;
  }
  
  sel_last.set(b2, s2, e2);
  
  while(b1 != b2 && b1 && b2) {
    if(b1->parent() && !b1->parent()->exitable()) {
      int o = box_order(b1, s1, b2, s2);
      
      if(o < 0)
        raw_select(b1, s1, b1->length());
      else
        raw_select(b1, 0, e1);
        
      return;
    }
    
    s1 = b1->index();
    e1 = s1 + 1;
    b1 = b1->parent();
    
    s2 = b2->index();
    e2 = s2 + 1;
    b2 = b2->parent();
  }
  
  if(s2 < s1)
    s1 = s2;
  if(e1 < e2)
    e1 =  e2;
    
  raw_select(b1, s1, e1);
}

void Document::move_to(Box *box, int index, bool selecting) {
  if(selecting)
    select_to(box, index, index);
  else
    select(box, index, index);
}

void Document::move_horizontal(
  LogicalDirection direction,
  bool             jumping,
  bool             selecting
) {
  Box *selbox = context.selection.get();
  Box *box = sel_last.get();
  if(!box) {
    sel_last = context.selection;
    
    box = selbox;
    if(!box)
      return;
  }
  
  int i = sel_last.start;
  if(direction == Forward)
    i = sel_last.end;
    
  if(sel_last.start != sel_last.end) {
    if(box == selbox) {
      if(direction == Forward) {
        if(sel_last.end == context.selection.end)
          box = box->move_logical(direction, jumping, &i);
      }
      else {
        if(sel_last.start == context.selection.start)
          box = box->move_logical(direction, jumping, &i);
      }
    }
  }
  else
    box = box->move_logical(direction, jumping, &i);
    
  while(box && !box->selectable(i)) {
    i = box->index();
    if(direction == Forward)
      ++i;
      
    box = box->parent();
  }
  
  if(selecting) {
    select_to(box, i, i);
  }
  else {
    MathSequence *seq = dynamic_cast<MathSequence *>(box);
    int j = i;
    
    if(seq) {
      if(direction == Forward) {
        if(seq->is_placeholder(i))
          ++j;
      }
      else {
        if(seq->is_placeholder(i - 1))
          --i;
      }
    }
    select(box, i, j);
  }
}

void Document::move_vertical(
  LogicalDirection direction,
  bool             selecting
) {
  Box *box = sel_last.get();
  if(!box) {
    box = context.selection.get();
    
    sel_last = context.selection;
    if(!box)
      return;
  }
  
  if(box == this && sel_last.start < sel_last.end) {
    if(selecting) {
      int i = sel_last.start;
      int j = sel_last.end;
      
      if(direction == Forward && j < length())
        ++j;
      else if(direction == Backward && i > 0)
        --i;
        
      select(this, i, j);
      return;
    }
    
    if(direction == Forward)
      move_to(this, sel_last.end);
    else
      move_to(this, sel_last.start);
      
    return;
  }
  
  int i, j;
  if(direction == Forward && sel_last.start + 1 < sel_last.end) {
    i = sel_last.end;
    if( sel_last.start < sel_last.end &&
        !dynamic_cast<MathSequence *>(box))
    {
      --i;
    }
  }
  else
    i = sel_last.start;
    
  box = box->move_vertical(direction, &best_index_rel_x, &i);
  
  j = i;
  MathSequence *seq = dynamic_cast<MathSequence *>(box);
  if(seq) {
    if(seq->is_placeholder(i - 1)) {
      --i;
      best_index_rel_x += seq->glyph_array()[i].right;
      if(i > 0)
        best_index_rel_x -= seq->glyph_array()[i - 1].right;
    }
    else if(seq->is_placeholder(i))
      ++j;
  }
  
  float tmp = best_index_rel_x;
  if(selecting)
    select_to(box, i, j);
  else
    select(box, i, j);
    
  if(context.selection.get() == box)
    best_index_rel_x = tmp;
}

void Document::move_start_end(
  LogicalDirection direction,
  bool             selecting
) {
  Box *box = selection_box();
  if(!box)
    return;
    
  int index = context.selection.start;
  if( context.selection.start < context.selection.end &&
      direction == Forward)
  {
    index = context.selection.end;
  }
  
  while(box->parent() && box->parent()->selectable()) {
    index = box->index();
    box = box->parent();
  }
  
  if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
    int l = seq->line_array().length() - 1;
    while(l > 0 && seq->line_array()[l - 1].end > index)
      --l;
      
    if(l >= 0) {
      if(direction == Forward) {
        index = seq->line_array()[l].end;
        
        if( index > 0 && seq->text()[index - 1] == '\n')
          --index;
      }
      else {
        if(l == 0)
          index = 0;
        else
          index = seq->line_array()[l - 1].end;
      }
    }
  }
  else if(TextSequence *seq = dynamic_cast<TextSequence *>(box)) {
    GSList *lines = pango_layout_get_lines_readonly(seq->get_layout());
    
    if(direction == Backward) {
      while(lines) {
        PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
        
        if(line->start_index + line->length >= index) {
          index = line->start_index;
          break;
        }
        
        lines = lines->next;
      }
    }
    else {
      const char *s = seq->text_buffer().buffer();
      while(lines) {
        PangoLayoutLine *line = (PangoLayoutLine *)lines->data;
        
        if(line->start_index + line->length >= index) {
          index = line->start_index + line->length;
          if(index > 0 && s[index - 1] == ' ')
            --index;
          break;
        }
        
        lines = lines->next;
      }
    }
  }
  else if(context.selection.start < context.selection.end && length() > 0) {
    index = context.selection.start;
    if(context.selection.start < context.selection.end && direction == Forward)
      index = context.selection.end - 1;
      
    if(section(index)->length() > 0) {
      if(direction == Forward) {
        direction = Backward;
        ++index;
      }
      else {
        direction = Forward;
      }
      box = move_logical(direction, false, &index);
    }
    else
      box = this;
  }
  
  if(selecting)
    select_to(box, index, index);
  else
    select(box, index, index);
}

void Document::move_tab(LogicalDirection direction) {
  Box *box = sel_last.get();
  if(!box) {
    box = context.selection.get();
    
    sel_last = context.selection;
    if(!box)
      return;
  }
  
  int i;
  if(direction == Forward) {
    i = sel_last.end;
  }
  else {
    i = sel_last.start;
    box = box->move_logical(direction, false, &i);
  }
  
  bool repeated = false;
  while(box) {
    if(box == this) {
      if(repeated)
        return;
        
      repeated = true;
      
      if(direction == Forward)
        --i;
      else
        ++i;
    }
    
    MathSequence *seq = dynamic_cast<MathSequence *>(box);
    if(seq && seq->is_placeholder(i)) {
      select(box, i, i + 1);
      return;
    }
    
    int old_i = i;
    Box *old_box = box;
    box = box->move_logical(direction, false, &i);
    
    if(old_i == i && old_box == box)
      return;
      
  }
}

bool Document::is_inside_string() {
  return is_inside_string(context.selection.get(), context.selection.start);
}

bool Document::is_inside_string(Box *box, int index) {
  while(box) {
    MathSequence *seq = dynamic_cast<MathSequence *>(box);
    if(seq) {
      if(seq->is_inside_string((index)))
        return true;
        
    }
    index = box->index();
    box = box->parent();
  }
  
  return false;
}

bool Document::is_inside_alias() {
  bool result = false;
  Box *box = context.selection.get();
  int index = context.selection.start;
  while(box) {
    MathSequence *seq = dynamic_cast<MathSequence *>(box);
    if(seq) {
      const uint16_t *buf = seq->text().buffer();
      for(int i = 0; i < index; ++i) {
        if(buf[i] == PMATH_CHAR_ALIASDELIMITER)
          result = !result;
      }
    }
    index = box->index();
    box = box->parent();
  }
  return result;
}

bool Document::is_tabkey_only_moving() {
  Box *selbox = context.selection.get();
  
  if(dynamic_cast<TextSequence *>(selbox))
    return false;
    
  if(context.selection.start != context.selection.end)
    return true;
    
  if(selbox == this)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
  
  if(!seq || !dynamic_cast<Section *>(seq->parent()))
    return true;
    
  const uint16_t *buf = seq->text().buffer();
  
  for(int i = context.selection.start - 1; i >= 0; ++i) {
    if(buf[i] == '\n')
      return false;
      
    if(buf[i] != '\t' && buf[i] != ' ')
      return true;
  }
  
  return false;
}

void Document::insert_pmath(int *pos, Expr boxes, int overwrite_until_index) {
  must_resize_min = *pos + 1;
  invalidate();
  SectionList::insert_pmath(pos, boxes, overwrite_until_index);
  must_resize_min = *pos;
}

void Document::insert(int pos, Section *section) {
  must_resize_min = pos + 1;
  invalidate();
  SectionList::insert(pos, section);
}

Section *Document::swap(int pos, Section *section) {
  invalidate();
  return SectionList::swap(pos, section);
}

void Document::select_prev(bool operands_only) {
  Box *selbox = context.selection.get();
  
  if(context.selection.start != context.selection.end)
    return;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
  
  int start = context.selection.start;
  
  if(seq && start > 0) {
    do {
      --start;
    } while(start > 0 && !seq->span_array().is_token_end(start - 1));
    
    if(start > 0 && seq->text()[start] == PMATH_CHAR_BOX) {
      int b = 0;
      while(seq->item(b)->index() < start)
        ++b;
        
      if(dynamic_cast<SubsuperscriptBox *>(seq->item(b))) {
        do {
          --start;
        } while(start > 0 && !seq->span_array().is_token_end(start - 1));
      }
    }
    
    if(pmath_char_is_right(seq->text()[start])) {
      int end = context.selection.start - 1;
      
      while(start >= 0) {
        Span s = seq->span_array()[start];
        while(s) {
          if(s.end() == end && seq->span_array().is_operand_start(start)) {
            move_to(seq, start, true);
            return;
          }
          
          s = s.next();
        }
        --start;
      }
    }
    else if(!pmath_char_is_left(seq->text()[start])) {
      if(!operands_only || seq->span_array().is_operand_start(start))
        move_to(seq, start, true);
      return;
    }
  }
}

Box *Document::prepare_copy(int *start, int *end) {
  if(selection_length() > 0) {
    *start = selection_start();
    *end = selection_end();
    return selection_box();
  }
  
  Box *box = selection_box();
  if(box && !dynamic_cast<AbstractSequence *>(box)) {
    AbstractSequence *parent = dynamic_cast<AbstractSequence *>(box->parent());
    
    if(parent) {
      *start = box->index();
      *end   = *start + 1;
      return parent->normalize_selection(start, end);
    }
  }
  
  *start = -1;
  *end   = -1;
  return 0;
}

bool Document::can_copy() {
  int start, end;
  Box *box = prepare_copy(&start, &end);
  
  return box && start < end;
}

String Document::copy_to_text(String mimetype) {
  int start, end;
  
  Box *selbox = prepare_copy(&start, &end);
  if(!selbox) {
    native()->beep();
    return String();
  }
  
  int flags = BoxFlagDefault;
  if(mimetype.equals(Clipboard::PlainText))
    flags |= BoxFlagLiteral | BoxFlagShortNumbers;
    
  Expr boxes = selbox->to_pmath(flags, start, end);
  if(mimetype.equals(Clipboard::BoxesText))
    return boxes.to_string(PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR);
    
  if(mimetype.equals(Clipboard::PlainText)) {
    Expr text = Application::interrupt(
                  Parse("FE`BoxesToText(`1`)", boxes),
                  Application::edit_interrupt_timeout);
                  
    return text.to_string();
  }
  
  native()->beep();
  return String();
}

void Document::copy_to_binary(String mimetype, Expr file) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    int start, end;
    
    Box *selbox = prepare_copy(&start, &end);
    if(!selbox) {
      native()->beep();
      return;
    }
    
    Expr boxes = selbox->to_pmath(BoxFlagDefault, start, end);
    file = Expr(pmath_file_create_compressor(file.release()));
    pmath_serialize(file.get(), boxes.release());
    pmath_file_close(file.release());
    return;
  }
  
  String text = copy_to_text(mimetype);
  // pmath_file_write(file.get(), text.buffer(), 2 * (size_t)text.length());
  pmath_file_writetext(file.get(), text.buffer(), text.length());
}

void Document::copy_to_clipboard() {
  SharedPtr<OpenedClipboard> cb = Clipboard::std->open_write();
  if(!cb) {
    native()->beep();
    return;
  }
  
  Expr file = Expr(pmath_file_create_binary_buffer(0));
  copy_to_binary(Clipboard::BoxesBinary, file);
  cb->add_binary_buffer(Clipboard::BoxesBinary, file);
  
  //cb->add_text(Clipboard::BoxesText, copy_to_text(Clipboard::BoxesText));
  cb->add_text(Clipboard::PlainText, copy_to_text(Clipboard::PlainText));
}

void Document::cut_to_clipboard() {
  copy_to_clipboard();
  
  int start, end;
  Box *box = prepare_copy(&start, &end);
  if(box) {
    select(box, start, end);
    
    remove_selection(false);
  }
}

void Document::paste_from_boxes(Expr boxes) {
  if( context.selection.get() == this &&
      get_style(Editable, true))
  {
    if( boxes[0] == PMATH_SYMBOL_SECTION ||
        boxes[0] == PMATH_SYMBOL_SECTIONGROUP)
    {
      remove_selection(false);
      
      int i = context.selection.start;
      insert_pmath(&i, boxes);
      
      select(this, i, i);
      
      return;
    }
  }
  
  boxes = Application::interrupt(
            Parse("FE`SectionsToBoxes(`1`)", boxes),
            Application::edit_interrupt_timeout);
            
  GridBox *grid = dynamic_cast<GridBox *>(context.selection.get());
  if(grid && grid->get_style(Editable)) {
    int row1, col1, row2, col2;
    
    grid->matrix().index_to_yx(context.selection.start, &row1, &col1);
    grid->matrix().index_to_yx(context.selection.end - 1, &row2, &col2);
    
    int w = col2 - col1 + 1;
    int h = row2 - row1 + 1;
    
    int options = BoxOptionDefault;
    if(grid->get_style(AutoNumberFormating))
      options |= BoxOptionFormatNumbers;
      
    MathSequence *tmp = new MathSequence;
    tmp->load_from_object(boxes, options);
    
    if(tmp->length() == 1 && tmp->count() == 1) {
      GridBox *tmpgrid = dynamic_cast<GridBox *>(tmp->item(0));
      
      if( tmpgrid &&
          tmpgrid->rows() <= h &&
          tmpgrid->cols() <= w)
      {
        for(int col = 0; col < w; ++col) {
          for(int row = 0; row < h; ++row) {
            if( col < tmpgrid->cols() &&
                row < tmpgrid->rows())
            {
              grid->item(row1 + row, col1 + col)->load_from_object(
                Expr(tmpgrid->item(row, col)->to_pmath(BoxFlagDefault)),
                BoxOptionFormatNumbers);
            }
            else {
              grid->item(row1 + row, col1 + col)->load_from_object(
                String::FromChar(PMATH_CHAR_BOX),
                BoxOptionDefault);
            }
          }
        }
        
        MathSequence *sel = grid->item(
                              row1 + tmpgrid->rows() - 1,
                              col1 + tmpgrid->cols() - 1)->content();
                              
        move_to(sel, sel->length());
        grid->invalidate();
        
        delete tmp;
        return;
      }
    }
    
    for(int col = 0; col < w; ++col) {
      for(int row = 0; row < h; ++row) {
        grid->item(row1 + row, col1 + col)->load_from_object(
          String::FromChar(PMATH_CHAR_BOX),
          BoxOptionDefault);
      }
    }
    
    MathSequence *sel = grid->item(row1, col1)->content();
    sel->remove(0, 1);
    sel->insert(0, tmp); tmp = 0;
    move_to(sel, sel->length());
    
    grid->invalidate();
    return;
  }
  
  GraphicsBox *graphics = dynamic_cast<GraphicsBox *>(context.selection.get());
  if(graphics && graphics->get_style(Editable)) {
    int options = BoxOptionDefault;
    if(graphics->get_style(AutoNumberFormating))
      options |= BoxOptionFormatNumbers;
        
    if(graphics->try_load_from_object(boxes, options)) 
      return;
    
    //select(graphics->parent(), graphics->index(), graphics->index() + 1);
  }
  
  remove_selection(false);
  
  if(prepare_insert()) {
    if(MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get())) {
    
      int options = BoxOptionDefault;
      if(seq->get_style(AutoNumberFormating))
        options |= BoxOptionFormatNumbers;
        
      MathSequence *tmp = new MathSequence;
      tmp->load_from_object(boxes, options);
      
      int newpos = context.selection.end + tmp->length();
      seq->insert(context.selection.end, tmp);
      
      select(seq, newpos, newpos);
      
      return;
    }
    
    if(TextSequence *seq = dynamic_cast<TextSequence *>(context.selection.get())) {
    
      int options = BoxOptionDefault;
      if(seq->get_style(AutoNumberFormating))
        options |= BoxOptionFormatNumbers;
        
      TextSequence *tmp = new TextSequence;
      tmp->load_from_object(boxes, options);
      
      int newpos = context.selection.end + tmp->length();
      seq->insert(context.selection.end, tmp);
      
      select(seq, newpos, newpos);
      
      return;
    }
  }
  
  native()->beep();
  return;
}

void Document::paste_from_text(String mimetype, String data) {
  if(mimetype.equals(Clipboard::BoxesText)) {
    Expr parsed = Application::interrupt(Expr(
                                           pmath_parse_string(data.release())),
                                         Application::edit_interrupt_timeout);
                                         
    paste_from_boxes(parsed);
    return;
  }
  
  if(mimetype.equals(Clipboard::PlainText)) {
    if(prepare_insert()) {
      remove_selection(false);
      
      // todo: replace \r\n by \n
      
      insert_string(data);
      
      return;
    }
  }
  
  native()->beep();
}

void Document::paste_from_binary(String mimetype, Expr file) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    pmath_serialize_error_t err = PMATH_SERIALIZE_OK;
    file = Expr(pmath_file_create_uncompressor(file.release()));
    Expr boxes = Expr(pmath_deserialize(file.get(), &err));
    paste_from_boxes(boxes);
    return;
  }
  
  String line;
  
  if(!pmath_file_test(file.get(), PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)) {
    native()->beep();
    return;
  }
  
  while(pmath_file_status(file.get()) == PMATH_FILE_OK) {
    line += String(pmath_file_readline(file.get()));
    line += "\n";
  }
  
  paste_from_text(mimetype, line);
}

void Document::paste_from_clipboard() {
  if(Clipboard::std->has_format(Clipboard::BoxesBinary)) {
    paste_from_binary(
      Clipboard::BoxesBinary,
      Clipboard::std->read_as_binary_file(Clipboard::BoxesBinary));
    return;
  }
  
  if(Clipboard::std->has_format(Clipboard::BoxesText)) {
    paste_from_text(
      Clipboard::BoxesText,
      Clipboard::std->read_as_text(Clipboard::BoxesText));
    return;
  }
  
  if(Clipboard::std->has_format(Clipboard::PlainText)) {
    paste_from_text(
      Clipboard::PlainText,
      Clipboard::std->read_as_text(Clipboard::PlainText));
    return;
  }
  
  native()->beep();
}

bool Document::split_section(bool do_it) {
  if(!get_own_style(Editable, false))
    return false;
    
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(selection_box());
  if(!seq)
    return false;
    
  int start = selection_start();
  int end   = selection_end();
  
  AbstractSequenceSection *sect = dynamic_cast<AbstractSequenceSection *>(seq->parent());
  if(!sect || !sect->get_own_style(Editable, false))
    return false;
    
  if(!do_it)
    return true;
    
  SharedPtr<Style> new_style = new Style();
  new_style->merge(sect->style);
  
  AbstractSequenceSection *new_sect;
  if(dynamic_cast<MathSection *>(sect))
    new_sect = new MathSection(new_style);
  else
    new_sect = new TextSection(new_style);
    
  int e = end;
  if(start == e && seq->char_at(e) == '\n')
    ++e;
    
  insert(sect->index() + 1, new_sect);
  new_sect->abstract_content()->insert(0, seq, e, seq->length());
  
  if(start < end) {
    new_style = new Style();
    new_style->merge(sect->style);
    
    if(dynamic_cast<MathSection *>(sect))
      new_sect = new MathSection(new_style);
    else
      new_sect = new TextSection(new_style);
      
    insert(sect->index() + 1, new_sect);
    new_sect->abstract_content()->insert(0, seq, start, end);
  }
  else if(seq->char_at(start - 1) == '\n')
    --start;
    
  seq->remove(start, seq->length());
  
  move_to(this, sect->index() + 1);
  move_horizontal(Forward, false);
  return true;
}

bool Document::merge_sections(bool do_it) {
  if(selection_box() != this)
    return false;
    
  if(!get_own_style(Editable, false))
    return false;
    
  int start = selection_start();
  int end   = selection_end();
  if(start + 1 >= end)
    return false;
    
  AbstractSequenceSection *first_sect = dynamic_cast<AbstractSequenceSection *>(item(start));
  if( !first_sect                                   ||
      !first_sect->selectable(0)                    ||
      !first_sect->get_own_style(Editable, false))
  {
    return false;
  }
  for(int i = start + 1; i < end; ++i) {
    AbstractSequenceSection *sect = dynamic_cast<AbstractSequenceSection *>(item(i));
    
    if(!sect || !sect->get_own_style(Editable, false))
      return false;
  }
  
  if(!do_it)
    return true;
    
  AbstractSequence *seq = first_sect->abstract_content();
  int pos = seq->length();
  
  for(int i = start + 1; i < end; ++i) {
    AbstractSequenceSection *sect = dynamic_cast<AbstractSequenceSection *>(item(i));
    AbstractSequence        *seq2 = sect->abstract_content();
    
    pos = seq->insert(pos, '\n');
    pos = seq->insert(pos, seq2, 0, seq2->length());
  }
  
  remove(start + 1, end);
  move_to(seq, 0);
  return true;
}

void Document::insert_string(String text, bool autoformat) {
  const uint16_t *buf = text.buffer();
  int             len = text.length();
  
  if(!prepare_insert()) {
    native()->beep();
    return;
  }
  
  // remove (dangerous) PMATH_CHAR_BOX from text:
  for(int i = 0; i < len; ++i) {
    if(buf[i] == PMATH_CHAR_BOX) {
      text.remove(i, 1);
      buf = text.buffer();
      len = text.length();
      --i;
    }
  }
  
  if(TextSequence *seq = dynamic_cast<TextSequence *>(selection_box())) {
    int i = seq->insert(selection_start(), text);
    move_to(seq, i);
    return;
  }
  
  if(autoformat) {
    if(is_inside_string()) {
      bool have_sth_to_escape = false;
      
      for(int i = 0; i < len; ++i) {
        if(buf[i] < ' ' || buf[i] == '"' || buf[i] == '\\') {
          have_sth_to_escape = true;
          break;
        }
      }
      
      // todo: ask user ...
      
      if(have_sth_to_escape) {
        char insbuf[5] = {'\\', 'x', '0', '0', '\0'};
        const char *ins = insbuf;
        
        for(int i = 0; i < len; ++i) {
          switch(buf[i]) {
            case '\t': ins = "\\t"; break;
            case '\n': ins = "\\n"; break;
            case '\r': ins = "\\r"; break;
            case '\\': ins = "\\\\"; break;
            case '\"': ins = "\\\""; break;
            
            default:
              if(buf[i] < ' ') {
                static const char *hex = "0123456789ABCDEF";
                
                insbuf[2] = hex[(buf[i] & 0xF0) >> 4];
                insbuf[3] = hex[ buf[i] & 0x0F];
                ins = insbuf;
              }
              else
                continue;
          }
          
          int il = strlen(ins);
          text.remove(i, 1);
          text.insert(i, ins, il);
          buf = text.buffer();
          len = text.length();
          i += il - 1;
        }
      }
    }
    else if(len > 0) { // remove space characters ...
      static Array<bool> del;
      
      del.length(len);
      del.zeromem();
      int i;
      for(i = 0; i < len; ++i) {
        if(buf[i] == ' ' || buf[i] == '\t') {
          del[i] = true;
        }
        else
          break;
      }
      
      uint16_t last_char = 0;
      for(; i < len; ++i) {
        switch(buf[i]) {
          case '\r': del[i] = true; break;
          case '\n': {
              for(int j = i - 1; j > 0; --j) {
                if(buf[j] != ' ' && buf[j] != '\t')
                  break;
                  
                del[j] = true;
              }
              
              for(int j = i + 1; j < len; ++j) {
                if(buf[j] != ' ' && buf[j] != '\t')
                  break;
                  
                del[j] = true;
              }
              
              last_char = '\n';
            } break;
            
          case '\"': {
              do {
                if(buf[i] == '\\') {
                  if(i + 1 < len)
                    i += 2;
                  else
                    ++i;
                }
                else
                  ++i;
              } while(i < len && buf[i] != '\"');
              last_char = buf[i - 1];
            } break;
            
          case ' ':
          case '\t': {
              if(i + 1 < len) {
                pmath_token_t tok = pmath_token_analyse(&buf[i + 1], 1, NULL);
                
                if(tok == PMATH_TOK_DIGIT) {
                  if( !pmath_char_is_name(last_char) &&
                      !pmath_char_is_digit(last_char))
                  {
                    del[i] = true;
                  }
                }
                else if(tok == PMATH_TOK_NAME) {
                  if(!pmath_char_is_name(last_char))
                    del[i] = true;
                }
                else if(tok == PMATH_TOK_LEFTCALL) {
                  if(i > 0 && buf[i - 1] == ' ')
                    del[i - 1] = false;
                }
                else
                  del[i] = true;
              }
              else
                del[i] = true;
            } break;
            
          default:
            last_char = buf[i];
        }
      }
      
      int ti = 0;
      int deli = 0;
      while(deli < del.length()) {
        if(del[deli]) {
          int delcnt = 0;
          do {
            ++delcnt;
            ++deli;
          } while(deli < del.length() && del[deli]);
          
          text.remove(ti, delcnt);
        }
        else {
          ++ti;
          ++deli;
        }
      }
      
      buf = text.buffer();
      len = text.length();
    }
  }
  
  if(len > 0) {
    MathSequence *seq = new MathSequence;
    seq->insert(0, text);
    
    if(autoformat && !is_inside_string()) { // replace tokens from global_immediate_macros ...
      seq->ensure_spans_valid();
      const SpanArray &spans = seq->span_array();
      
      MathSequence *seq2 = 0;
      int last = 0;
      int pos = 0;
      while(pos < len) {
        if(buf[pos] == '"'){
          do {
            if(buf[pos] == '\\') {
              if(pos + 1 < len)
                pos += 2;
              else
                ++pos;
            }
            else
              ++pos;
          } while(pos < len && buf[pos] != '"');
          
          if(pos < len && buf[pos] == '"')
            ++pos;
        }
        
        int next = pos;
        while(!spans.is_token_end(next))
          ++next;
        ++next;
        
        Expr *e = global_immediate_macros.search(text.part(pos, next - pos));
        if(e) {
          if(!seq2)
            seq2 = new MathSequence;
            
          seq2->insert(seq2->length(), text.part(last, pos - last));
          
          MathSequence *seq_tmp = new MathSequence;
          seq_tmp->load_from_object(*e, BoxOptionDefault);
          seq2->insert(seq2->length(), seq_tmp);
          
          last = next;
        }
        
        pos = next;
      }
      
      if(seq2) {
        seq2->insert(seq2->length(), text.part(last, len - last));
        delete seq;
        seq = seq2;
      }
    }
    
    insert_box(seq, false);
  }
}

void Document::insert_box(Box *box, bool handle_placeholder) {
  if(!box || !prepare_insert()) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_box(box, handle_placeholder);
    }
    else {
      native()->beep();
      delete box;
    }
    
    return;
  }
  
  assert(box->parent() == 0);
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
    handle_macros();
    
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
    Box *new_sel_box = 0;
    int new_sel_start = 0;
    int new_sel_end = 0;
    
    if(handle_placeholder) {
      AbstractSequence *placeholder_seq = 0;
      int placeholder_pos = 0;
      
      int i = 0;
      Box *current = box;
      while(current && (current != box || i < box->length())) {
        AbstractSequence *current_seq = dynamic_cast<AbstractSequence *>(current);
        
        if(current_seq && current_seq->is_placeholder(i)) {
          if(current_seq->char_at(i) == CHAR_REPLACEMENT) {
            placeholder_seq = current_seq;
            placeholder_pos = i;
            break;
          }
          
          if(!placeholder_seq) {
            placeholder_seq = current_seq;
            placeholder_pos = i;
          }
        }
        
        current = current->move_logical(Forward, false, &i);
      }
      
      if(placeholder_seq) {
        if(selection_length() == 0) {
          if(placeholder_seq->char_at(placeholder_pos) == CHAR_REPLACEMENT) {
            placeholder_seq->remove(placeholder_pos, placeholder_pos + 1);
            placeholder_seq->insert(placeholder_pos, PMATH_CHAR_PLACEHOLDER);
          }
          
          new_sel_box   = placeholder_seq;
          new_sel_start = placeholder_pos;
          new_sel_end   = new_sel_start + 1;
        }
        else {
          placeholder_seq->remove(placeholder_pos, placeholder_pos + 1);
          placeholder_seq->insert(
            placeholder_pos,
            seq,
            context.selection.start,
            context.selection.end);
            
          if(selection_length() == 1 &&
              placeholder_seq->is_placeholder(placeholder_pos))
          {
            new_sel_box   = placeholder_seq;
            new_sel_start = placeholder_pos;
            new_sel_end   = new_sel_start + 1;
          }
          else {
            current = placeholder_seq;
            i = placeholder_pos + selection_length();
            while(current) {
              MathSequence *current_seq = dynamic_cast<MathSequence *>(current);
              
              if(current_seq && current_seq->is_placeholder(i)) {
                new_sel_box = current_seq;
                new_sel_start = i;
                new_sel_end = i + 1;
                break;
              }
              
              Box *prev = current;
              int prev_index = i;
              current = current->move_logical(Forward, false, &i);
              
              if(prev == current && i == prev_index)
                break;
            }
            
            if(!new_sel_box) {
              current = box;
              i = 0;
              while(current && (current != placeholder_seq || i < placeholder_pos)) {
                MathSequence *current_seq = dynamic_cast<MathSequence *>(current);
                
                if(current_seq && current_seq->is_placeholder(i)) {
                  new_sel_box   = current_seq;
                  new_sel_start = i;
                  new_sel_end   = i + 1;
                  break;
                }
                
                current = current->move_logical(Forward, false, &i);
              }
            }
          }
        }
      }
    }
    
    seq = dynamic_cast<AbstractSequence *>(context.selection.get());
    if(!seq) {
      delete box;
      return;
    }
    
    if(context.selection.start < context.selection.end) {
      seq->remove(context.selection.start, context.selection.end);
      context.selection.end = context.selection.start;
    }
    
    if(new_sel_box) {
      if(new_sel_box == box) {
        new_sel_box    = seq;
        new_sel_start += context.selection.start;
        new_sel_end   += context.selection.start;
      }
      
      seq->insert(context.selection.start, box);
      select(new_sel_box, new_sel_start, new_sel_end);
    }
    else {
      int len = 1;
      if(dynamic_cast<MathSequence *>(box))
        len = box->length();
        
      seq->insert(context.selection.start, box);
      
      move_to(seq, context.selection.start + len);
    }
    
    return;
  }
  
  delete box;
}

void Document::insert_fraction() {
  if(!prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_fraction();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  select_prev(true);
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  if(seq) {
    MathSequence *num = new MathSequence;
    MathSequence *den = new MathSequence;
    
    seq->insert(context.selection.end, new FractionBox(num, den));
    
    den->insert(0, PMATH_CHAR_PLACEHOLDER);
    if(context.selection.start < context.selection.end) {
      num->insert(
        0, seq,
        context.selection.start,
        context.selection.end);
      seq->remove(context.selection.start, context.selection.end);
      if(num->is_placeholder())
        select(num, 0, 1);
      else
        select(den, 0, 1);
    }
    else {
      num->insert(0, PMATH_CHAR_PLACEHOLDER);
      select(num, 0, 1);
    }
  }
}

void Document::insert_matrix_column() {
  if(!prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_column();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  GridBox *grid = 0;
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  
  if(context.selection.start == context.selection.end ||
      (seq && seq->is_placeholder()))
  {
    Box *b = context.selection.get();
    int i = 0;
    while(b) {
      i = b->index();
      b = b->parent();
      grid = dynamic_cast<GridBox *>(b);
      if(grid)
        break;
    }
    
    if(grid) {
      int row, col;
      grid->matrix().index_to_yx(i, &row, &col);
      
      if( context.selection.end > 0 ||
          selection_box() != grid->item(0, col)->content())
      {
        ++col;
      }
      
      grid->insert_cols(col, 1);
      select(grid->item(0, col)->content(), 0, 1);
      return;
    }
  }
  
  if( seq &&
      context.selection.start > 0 &&
      context.selection.start == context.selection.end &&
      !seq->span_array().is_token_end(context.selection.start - 1))
  {
    while(context.selection.end < seq->length() &&
          !seq->span_array().is_token_end(context.selection.end))
      ++context.selection.end;
      
    if(context.selection.end < seq->length())
      ++context.selection.end;
      
    context.selection.start = context.selection.end;
  }
  
  select_prev(true);
  
  if(seq) {
    grid = new GridBox(1, 2);
    seq->insert(context.selection.end, grid);
    
    if(context.selection.start < context.selection.end) {
      grid->item(0, 0)->content()->remove(0, grid->item(0, 0)->content()->length());
      grid->item(0, 0)->content()->insert(
        0, seq,
        context.selection.start,
        context.selection.end);
      seq->remove(context.selection.start, context.selection.end);
      if(grid->item(0, 0)->content()->is_placeholder())
        select(grid->item(0, 0)->content(), 0, 1);
      else
        select(grid->item(0, 1)->content(), 0, 1);
    }
    else {
      select(grid->item(0, 0)->content(), 0, 1);
    }
  }
}

void Document::insert_matrix_row() {
  if(!prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_row();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  GridBox *grid = 0;
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  
  if(context.selection.start == context.selection.end ||
      (seq && seq->is_placeholder()))
  {
    Box *b = context.selection.get();
    int i = 0;
    
    while(b) {
      i = b->index();
      b = b->parent();
      grid = dynamic_cast<GridBox *>(b);
      if(grid)
        break;
    }
    
    if(grid) {
      int row, col;
      grid->matrix().index_to_yx(i, &row, &col);
      
      if( context.selection.end > 0 ||
          context.selection.get() != grid->item(row, 0)->content())
      {
        ++row;
      }
      
      grid->insert_rows(row, 1);
      select(grid->item(row, 0)->content(), 0, 1);
      return;
    }
  }
  
  if( seq &&
      context.selection.start > 0 &&
      context.selection.start == context.selection.end &&
      !seq->span_array().is_token_end(context.selection.start - 1))
  {
    while( context.selection.end < seq->length() &&
           !seq->span_array().is_token_end(context.selection.end))
    {
      ++context.selection.end;
    }
    
    if(selection_end() < seq->length())
      ++context.selection.end;
      
    context.selection.start = context.selection.end;
  }
  
  select_prev(true);
  
  if(seq) {
    grid = new GridBox(2, 1);
    seq->insert(context.selection.end, grid);
    
    if(context.selection.start < context.selection.end) {
      grid->item(0, 0)->content()->remove(0, grid->item(0, 0)->content()->length());
      grid->item(0, 0)->content()->insert(
        0, seq,
        context.selection.start,
        context.selection.end);
      seq->remove(context.selection.start, context.selection.end);
      if(grid->item(0, 0)->content()->is_placeholder())
        select(grid->item(0, 0)->content(), 0, 1);
      else
        select(grid->item(1, 0)->content(), 0, 1);
    }
    else {
      select(grid->item(0, 0)->content(), 0, 1);
    }
  }
}

void Document::insert_sqrt() {
  if(!prepare_insert_math(false)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_sqrt();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  if(seq) {
    MathSequence *content = new MathSequence;
    seq->insert(context.selection.end, new RadicalBox(content, 0));
    
    if(context.selection.start < context.selection.end) {
      content->insert(
        0, seq,
        context.selection.start,
        context.selection.end);
        
      seq->remove(context.selection.start, context.selection.end);
      
      if(content->is_placeholder())
        select(content, 0, 1);
      else
        move_to(content, content->length());
    }
    else {
      content->insert(0, PMATH_CHAR_PLACEHOLDER);
      select(content, 0, 1);
    }
  }
}

void Document::insert_subsuperscript(bool sub) {
  if(!prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_subsuperscript(sub);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  if(seq) {
    int pos = context.selection.end;
    
    if(context.selection.end == 0) {
      seq->insert(0, PMATH_CHAR_PLACEHOLDER);
      pos += 1;
    }
    else {
      bool op = false;
      
      for(int i = context.selection.start; i < context.selection.end; ++i) {
        if(seq->text()[i] == PMATH_CHAR_BOX) {
          seq->insert(context.selection.start, "(");
          seq->insert(context.selection.end + 1, ")");
          pos += 2;
          break;
        }
        if(seq->span_array().is_operand_start(i)) {
          if(op) {
            seq->insert(context.selection.start, "(");
            seq->insert(context.selection.end + 1, ")");
            pos += 2;
            break;
          }
          op = true;
        }
      }
    }
    
    MathSequence *content = new MathSequence;
    content->insert(0, PMATH_CHAR_PLACEHOLDER);
    
    seq->insert(pos, new SubsuperscriptBox(
                  sub  ? content : 0,
                  !sub ? content : 0));
                  
    select(content, 0, 1);
  }
}

void Document::insert_underoverscript(bool under) {
  if(!prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_underoverscript(under);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !is_inside_string() &&
      !is_inside_alias() &&
      !handle_immediate_macros())
  {
    handle_macros();
  }
  
  select_prev(false);
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  if(seq) {
    MathSequence *base = new MathSequence;
    MathSequence *uo = new MathSequence;
    uo->insert(0, PMATH_CHAR_PLACEHOLDER);
    
    seq->insert(
      context.selection.end,
      new UnderoverscriptBox(
        base,
        under  ? uo : 0,
        !under ? uo : 0));
        
    if(context.selection.start < context.selection.end) {
      base->insert(
        0, seq,
        context.selection.start,
        context.selection.end);
        
      seq->remove(context.selection.start, context.selection.end);
      if(base->is_placeholder())
        select(base, 0, 1);
      else
        select(uo, 0, 1);
    }
    else {
      base->insert(0, PMATH_CHAR_PLACEHOLDER);
      select(base, 0, 1);
    }
  }
}

bool Document::remove_selection(bool insert_default) {
  if(selection_length() == 0)
    return false;
    
  if(selection_box() && !selection_box()->edit_selection(&context))
    return false;
    
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get());
  if(seq) {
    if(MathSequence *mseq = dynamic_cast<MathSequence *>(seq)) {
      bool was_empty = mseq->length() == 0 ||
                       (mseq->length() == 1 &&
                        mseq->text()[0] == PMATH_CHAR_PLACEHOLDER);
                        
      mseq->remove(context.selection.start, context.selection.end);
      
      if( insert_default &&
          was_empty &&
          mseq->parent() &&
          mseq->parent()->remove_inserts_placeholder())
      {
        int index = mseq->index();
        Box *box = mseq->parent()->remove(&index);
        
        mseq = dynamic_cast<MathSequence *>(box);
        if(insert_default && mseq && mseq->length() == 0) {
          mseq->insert(0, PMATH_CHAR_PLACEHOLDER);
          select(mseq, 0, 1);
        }
        else if(mseq && mseq->is_placeholder(index)) {
          select(mseq, index, index + 1);
        }
        else
          move_to(box, index);
          
        return true;
      }
      
      if( insert_default &&
          mseq->length() == 0 &&
          mseq->parent() &&
          mseq->parent()->remove_inserts_placeholder())
      {
        mseq->insert(0, PMATH_CHAR_PLACEHOLDER);
        select(mseq, 0, 1);
        return true;
      }
    }
    else
      seq->remove(context.selection.start, context.selection.end);
      
    move_to(context.selection.get(), context.selection.start);
    return true;
  }
  
  GridBox *grid = dynamic_cast<GridBox *>(context.selection.get());
  if(grid) {
    int start = context.selection.start;
    Box *box = grid->remove_range(&start, context.selection.end);
    select(box, start, start);
    return true;
  }
  
  if(context.selection.id == this->id()) {
    remove(context.selection.start, context.selection.end);
    move_to(this, context.selection.start);
    return true;
  }
  
  return false;
}

void Document::toggle_open_close_current_group() {
  int s = selection_start();
  int e = selection_end();
  Box *box = selection_box();
  
  while(box && box != this) {
    s = box->index();
    e = s + 1;
    box = box->parent();
  }
  
  if(box && s < e) {
    for(int i = s; i < e; ++i) {
      toggle_open_close_group(i);
      i = group_info(i).end;
    }
  }
  else
    native()->beep();
}

void Document::complete_box() {
  Box *b = context.selection.get();
  while(b) {
    {
      RadicalBox *rad = dynamic_cast<RadicalBox *>(b);
      
      if(rad && rad->count() == 1) {
        rad->complete();
        rad->exponent()->insert(0, PMATH_CHAR_PLACEHOLDER);
        select(rad->exponent(), 0, 1);
        
        return;
      }
    }
    
    {
      SubsuperscriptBox *subsup = dynamic_cast<SubsuperscriptBox *>(b);
      
      if(subsup && subsup->count() == 1) {
        if(subsup->subscript()) {
          subsup->complete();
          subsup->superscript()->insert(0, PMATH_CHAR_PLACEHOLDER);
          select(subsup->superscript(), 0, 1);
        }
        else {
          subsup->complete();
          subsup->subscript()->insert(0, PMATH_CHAR_PLACEHOLDER);
          select(subsup->subscript(), 0, 1);
        }
        
        return;
      }
    }
    
    {
      UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox *>(b);
      
      if(underover && underover->count() == 2) {
        if(underover->underscript()) {
          underover->complete();
          underover->overscript()->insert(0, PMATH_CHAR_PLACEHOLDER);
          select(underover->overscript(), 0, 1);
        }
        else {
          underover->complete();
          underover->underscript()->insert(0, PMATH_CHAR_PLACEHOLDER);
          select(underover->underscript(), 0, 1);
        }
        
        return;
      }
    }
    
    b = b->parent();
  }
  
  native()->beep();
}

void Document::reset_mouse() {
  mouse_down_counter = 0;
  Application::delay_dynamic_updates(false);
}

void Document::reset_style() {
  if(style)
    style->clear();
  else
    style = new Style;
    
  style->set(BaseStyleName, "Document");
}

void Document::paint_resize(Canvas *canvas, bool resize_only) {
  style->update_dynamic(this);
  
  context.canvas = canvas;
  
  float sx, sy, h;
  native()->window_size(&_window_width, &h);
  native()->page_size(&_page_width, &h);
  native()->scroll_pos(&sx, &sy);
  
  context.width = _page_width;
  context.section_content_window_width = _window_width;
  
  if(_page_width < HUGE_VAL)
    _extents.width = _page_width;
  else
    _extents.width = 0;
    
  unfilled_width = 0;
  _extents.ascent = _extents.descent = 0;
  
  canvas->translate(-sx, -sy);
  
  init_section_bracket_sizes(&context);
  
  int sel_sect = -1;
  Box *b = context.selection.get();
  while(b && b != this) {
    sel_sect = b->index();
    b = b->parent();
  }
  
  int i = 0;
  
  while(i < length()) {
    if(section(i)->must_resize) // || i == sel_sect)
      resize_section(&context, i);
      
    if(_extents.descent + section(i)->extents().height() >= sy && !resize_only)
      break;
      
    section(i)->y_offset = _extents.descent;
    if(section(i)->visible) {
      _extents.descent += section(i)->extents().descent;
      
      float w  = section(i)->extents().width;
      float uw = section(i)->unfilled_width;
      if(get_own_style(ShowSectionBracket, true)) {
        w +=  section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
        uw += section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
      }
      
      if(_extents.width < w)
        _extents.width = w;
        
      if(unfilled_width < uw)
        unfilled_width = uw;
    }
    
    ++i;
  }
  
  {
    float y = 0;
    if(i < length())
      y += section(i)->y_offset;
    canvas->move_to(0, y);
  }
  
  int first_visible_section = i;
  
  if(!resize_only) {
    while(i < length() && _extents.descent <= sy + h) {
      paint_section(&context, i, sx);
      
      section(i)->y_offset = _extents.descent;
      if(section(i)->visible) {
        _extents.descent += section(i)->extents().descent;
        
        float w  = section(i)->extents().width;
        float uw = section(i)->unfilled_width;
        if(get_own_style(ShowSectionBracket, true)) {
          w +=  section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
          uw += section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
        }
        
        if(_extents.width < w)
          _extents.width = w;
          
        if(unfilled_width < uw)
          unfilled_width = uw;
      }
      
      ++i;
    }
  }
  
  int last_visible_section = i - 1;
  
  while(i < length()) {
    if(section(i)->must_resize) {
      bool resi = (i == sel_sect || i < must_resize_min);
      
      if(!resi && auto_scroll) {
        resi = (i <= sel_sect);
        
        if(!resi && context.selection.id == this->id())
          resi = (i < context.selection.end);
      }
      
      if(resi) {
        resize_section(&context, i);
      }
    }
    
    section(i)->y_offset = _extents.descent;
    if(section(i)->visible) {
      _extents.descent += section(i)->extents().descent;
      
      float w  = section(i)->extents().width;
      float uw = section(i)->unfilled_width;
      if(get_own_style(ShowSectionBracket, true)) {
        w +=  section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
        uw += section_bracket_right_margin + section_bracket_width * group_info(i).nesting;
      }
      
      if(_extents.width < w)
        _extents.width = w;
        
      if(unfilled_width < uw)
        unfilled_width = uw;
    }
    
    ++i;
  }
  
  if(!resize_only) {
    // paint cursor (as a horizontal line) at end of document:
    if( context.selection.id == this->id() &&
        context.selection.start == context.selection.end)
    {
      float y;
      if(context.selection.start < length())
        y = section(context.selection.start)->y_offset;
      else
        y = _extents.descent;
        
      float x1 = sx;
      float y1 = y + 0.5;
      float x2 = sx + _extents.width;
      float y2 = y + 0.5;
      
      context.canvas->align_point(&x1, &y1, true);
      context.canvas->align_point(&x2, &y2, true);
      context.canvas->move_to(x1, y1);
      context.canvas->line_to(x2, y2);
      
      context.canvas->set_color(0xC0C0C0);
      context.canvas->hair_stroke();
      
      if(context.selection.start < count()) {
        x1 = section(context.selection.start)->get_style(SectionMarginLeft);
      }
      else if(count() > 0) {
        x1 = section(context.selection.start - 1)->get_style(SectionMarginLeft);
      }
      else
        x1 = 20 * 0.75;
      x2 = x1 + 40 * 0.75;
      
      context.canvas->align_point(&x1, &y1, true);
      context.canvas->align_point(&x2, &y2, true);
      context.canvas->move_to(x1, y1);
      context.canvas->line_to(x2, y2);
      
      context.draw_selection_path();
    }
    
    // highlight the current selected word in the whole document:
    if(selection_length() > 0) {
      MathSequence *seq = dynamic_cast<MathSequence *>(selection_box());
      int start = selection_start();
      int end   = selection_end();
      int len   = selection_length();
      
      if( seq &&
          !seq->is_placeholder(start) &&
          (start == 0 || seq->span_array().is_token_end(start - 1)) &&
          seq->span_array().is_token_end(end - 1))
      {
        if(selection_is_name(this)) {
          String s = seq->text().part(start, len);
          
          if(s.length() > 0) {
            Box *find = this;
            int index = first_visible_section;
            
            int count_occurences = 0;
            
            while(0 != (find = search_string(
                                 find, &index, this, last_visible_section + 1, s, true))
                 ) {
              int s = index - len;
              int e = index;
              Box *b = find->get_highlight_child(find, &s, &e);
              
              if(b == find) {
                ::selection_path(context.canvas, b, s, e);
              }
              ++count_occurences;
            }
            
            bool do_fill = false;
            
            if(count_occurences == 1) {
              if( sel_sect >= first_visible_section &&
                  sel_sect <= last_visible_section)
              {
                // The one found occurency is the selection. Search for more
                // occurencies outside the visible range.
                find = this;
                index = 0;
                
                while(0 != (find = search_string(
                                     find, &index, this, first_visible_section, s, true))
                     ) {
                  do_fill = true;
                  break;
                }
                
                if(!do_fill) {
                  find = this;
                  index = last_visible_section + 1;
                  
                  while(0 != (find = search_string(
                                       find, &index, this, length(), s, true))
                       ) {
                    do_fill = true;
                    break;
                  }
                  
                }
              }
              else
                do_fill = true;
            }
            else
              do_fill = (count_occurences > 1);
              
            if(do_fill) {
              cairo_push_group(context.canvas->cairo());
              {
                context.canvas->save();
                {
                  cairo_matrix_t idmat;
                  cairo_matrix_init_identity(&idmat);
                  cairo_set_matrix(context.canvas->cairo(), &idmat);
                  cairo_set_line_width(context.canvas->cairo(), 2.0);
                  context.canvas->set_color(0xFF0000);
                  context.canvas->stroke_preserve();
                }
                context.canvas->restore();
                
                context.canvas->set_color(0xFF9933); //ControlPainter::std->selection_color()
                context.canvas->fill();
              }
              cairo_pop_group_to_source(canvas->cairo());
              canvas->paint_with_alpha(0.3);
            }
            else
              context.canvas->new_path();
          }
        }
      }
    }
    
    if(drag_source != context.selection && drag_status == DragStatusCurrentlyDragging) {
      if(Box *drag_src = drag_source.get()) {
        ::selection_path(canvas, drag_src, drag_source.start, drag_source.end);
        context.draw_selection_path();
      }
    }
    
    if(DebugFollowMouse) {
      b = mouse_move_sel.get();
      if(b) {
        ::selection_path(canvas, b, mouse_move_sel.start, mouse_move_sel.end);
        if(is_inside_string(b, mouse_move_sel.start))
          canvas->set_color(0x8000ff);
        else
          canvas->set_color(0xff0000);
        canvas->hair_stroke();
      }
    }
    
    if(auto_scroll) {
      auto_scroll = false;
      if(Box *box = sel_last.get())
        box->scroll_to(canvas, box, sel_last.start, sel_last.end);
    }
    
    if(prev_sel_line >= 0) {
      AbstractSequence *seq = dynamic_cast<AbstractSequence *>(selection_box());
      
      if(seq && seq->id() == prev_sel_box_id) {
        int line = seq->get_line(selection_end(), prev_sel_line);
        
        if(line != prev_sel_line) {
          flashing_cursor_circle = new BoxRepaintEvent(this->id()/*prev_sel_box_id*/, 0);
        }
      }
      
      prev_sel_line = -1;
      prev_sel_box_id = 0;
    }
    
    if(flashing_cursor_circle) {
      double t = flashing_cursor_circle->timer();
      Box *box = selection_box();
      
      if( !box ||
          t >= MaxFlashingCursorTime ||
          !flashing_cursor_circle->register_event())
      {
        flashing_cursor_circle = 0;
      }
      
      t = t / MaxFlashingCursorTime;
      
      if(flashing_cursor_circle) {
        double r = MaxFlashingCursorRadius * (1 - t);
        float x1 = context.last_cursor_x[0];
        float y1 = context.last_cursor_y[0];
        float x2 = context.last_cursor_x[1];
        float y2 = context.last_cursor_y[1];
        
        r = MaxFlashingCursorRadius * (1 - t);
        
        context.canvas->save();
        {
          context.canvas->user_to_device(&x1, &y1);
          context.canvas->user_to_device(&x2, &y2);
          
          cairo_matrix_t mat;
          cairo_matrix_init_identity(&mat);
          cairo_set_matrix(context.canvas->cairo(), &mat);
          
          double dx = x2 - x1;
          double dy = y2 - y1;
          double h = sqrt(dx * dx + dy * dy);
          double c = dx / h;
          double s = dy / h;
          double x = (x1 + x2) / 2;
          double y = (y1 + y2) / 2;
          mat.xx = c;
          mat.yx = s;
          mat.xy = -s;
          mat.yy = c;
          mat.x0 = x - c * x + s * y;
          mat.y0 = y - s * x - c * y;
          context.canvas->transform(mat);
          context.canvas->translate(x, y);
          context.canvas->scale(r + h / 2, r);
          
          context.canvas->arc(0, 0, 1, 0, 2 * M_PI, false);
          
          cairo_set_operator(context.canvas->cairo(), CAIRO_OPERATOR_DIFFERENCE);
          context.canvas->set_color(0xffffff);
          context.canvas->fill();
        }
        context.canvas->restore();
      }
    }
    
    if(selection_length() == 1 && best_index_rel_x == 0) {
      MathSequence *seq = dynamic_cast<MathSequence *>(selection_box());
      if(seq) {
        best_index_rel_x = seq->glyph_array()[selection_end() - 1].right;
        if(selection_start() > 0)
          best_index_rel_x -= seq->glyph_array()[selection_start() - 1].right;
          
        best_index_rel_x /= 2;
      }
    }
    
    canvas->translate(sx, sy);
  }
  
  context.canvas = 0;
  must_resize_min = 0;
}

Expr Document::to_pmath(int flags) {
  Gather g;
  
  Expr content = SectionList::to_pmath(flags);
  if(content[0] == PMATH_SYMBOL_SECTIONGROUP) {
    Expr inner = content[1];
    if(inner.expr_length() == 1 && inner[0] == PMATH_SYMBOL_LIST)
      content = inner[1];
  }
  
  Gather::emit(List(content));
  
  style->emit_to_pmath(false);
  
  Expr e = g.end();
  e.set(0, Symbol(PMATH_SYMBOL_DOCUMENT));
  return e;
}

void Document::set_prev_sel_line() {
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(selection_box());
  if(seq) {
    prev_sel_line = seq->get_line(selection_end(), prev_sel_line);
    prev_sel_box_id = seq->id();
  }
  else {
    prev_sel_line = -1;
    prev_sel_box_id = -1;
  }
}

bool Document::prepare_insert() {
  if(context.selection.id == this->id()) {
    prev_sel_line = -1;
    if( context.selection.start != context.selection.end ||
        !get_style(Editable, true))
    {
      return false;
    }
    
    Expr style_expr = get_group_style(
                        context.selection.start - 1,
                        DefaultNewSectionStyle,
                        Symbol(PMATH_SYMBOL_FAILED));
                        
    SharedPtr<Style> section_style = new Style(style_expr);
    
    String lang;
    if(!section_style->get(LanguageCategory, &lang)) {
      SharedPtr<Stylesheet> all = stylesheet();
      if(all)
        all->get(section_style, LanguageCategory, &lang);
    }
    
    Section *sect;
    if(lang.equals("pMath"))
      sect = new MathSection(section_style);
    else
      sect = new TextSection(section_style);
      
    insert(context.selection.start, sect);
    move_horizontal(Forward, false);
    
    return true;
  }
  else {
    if(selection_box() && selection_box()->edit_selection(&context)) {
      set_prev_sel_line();
      return true;
    }
  }
  
  return false;
}

bool Document::prepare_insert_math(bool include_previous_word) {
  if(!prepare_insert())
    return false;
    
  if(dynamic_cast<MathSequence *>(selection_box()))
    return true;
    
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(selection_box());
  if(!seq)
    return false;
    
  if(include_previous_word && selection_length() == 0) {
    TextSequence *txt = dynamic_cast<TextSequence *>(seq);
    
    if(txt) {
      const char *buf = txt->text_buffer().buffer();
      int i = selection_start();
      
      while(i > 0 && (unsigned char)buf[i] > ' ')
        --i;
        
      select(txt, i, selection_end());
    }
  }
  
  InlineSequenceBox *box = new InlineSequenceBox;
  
  int s = selection_start();
  int e = selection_end();
  box->content()->insert(0, seq, s, e);
  seq->remove(s, e);
  seq->insert(s, box);
  
  select(box->content(), 0, box->content()->length());
  return true;
}

bool Document::handle_immediate_macros(
  const Hashtable<String, Expr, object_hash> &table
) {
  if(selection_length() != 0)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(selection_box());
  if(seq && selection_start() > 0) {
    int i = selection_start() - 2;
    while(i >= 0 && !seq->span_array().is_token_end(i))
      --i;
    ++i;
    
    int e = selection_start() - 1;
    while(e < seq->length() && !seq->span_array().is_token_end(e))
      ++e;
      
    Expr repl = table[seq->text().part(i, e - i + 1)];
    
    if(!repl.is_null()) {
      String s(repl);
      
      if(s.is_null()) {
        MathSequence *repl_seq = new MathSequence();
        repl_seq->load_from_object(repl, BoxOptionDefault);
        
        seq->remove(i, e + 1);
        move_to(selection_box(), i);
        insert_box(repl_seq, true);
        return true;
      }
      else {
        seq->insert(e + 1, s);
        seq->remove(i, e + 1);
        move_to(selection_box(), i + s.length());
        return true;
      }
    }
  }
  
  return false;
}

bool Document::handle_macros(
  const Hashtable<String, Expr, object_hash> &table
) {
  if(selection_length() != 0)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(selection_box());
  if(seq && selection_start() > 0) {
    const uint16_t *buf = seq->text().buffer();
    
    int e = selection_start();
    int i = e - 1;
    
    if(seq->is_inside_string(i))
      return false;
      
    while(i >= 0 && buf[i] > ' ' && buf[i] != '\\')
      --i;
      
    int j = i;
    while(j >= 0 && buf[j] == '\\')
      --j;
      
    if(i < e - 1 && (i - j) % 2 == 1) {
      uint32_t unichar;
      const uint16_t *bufend = pmath_char_parse(buf + i, e - i, &unichar);
      
      Expr repl;// = String::FromChar(unicode_to_utf32(s));
      if(bufend == buf + e && unichar <= 0x10FFFF) {
        repl = String::FromChar(unichar);
      }
      else {
        String s = seq->text().part(i + 1, e - i - 1);
        repl = String::FromChar(unicode_to_utf32(s));
        if(repl.is_null())
          repl = table[s];
      }
      
      if(!repl.is_null()) {
        String s(repl);
        
        if(!s.is_null()) {
          seq->insert(e, s);
          seq->remove(i, e);
          move_to(selection_box(), i + s.length());
          return true;
        }
        else {
          MathSequence *repl_seq = new MathSequence();
          repl_seq->load_from_object(repl, BoxOptionDefault);
          
          seq->remove(i, e);
          move_to(selection_box(), i);
          insert_box(repl_seq, true);
          return true;
        }
      }
    }
  }
  
  return false;
}

bool Document::handle_immediate_macros() {
  return handle_immediate_macros(global_immediate_macros);
}

bool Document::handle_macros() {
  return handle_macros(global_macros);
}

//{ ... class Document
