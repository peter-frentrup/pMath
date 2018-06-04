#include <gui/document.h>

#include <cmath>
#include <cstdio>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/buttonbox.h>
#include <boxes/fractionbox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/mathsequence.h>
#include <boxes/radicalbox.h>
#include <boxes/section.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/textsequence.h>
#include <boxes/underoverscriptbox.h>
#include <eval/binding.h>
#include <gui/clipboard.h>
#include <gui/native-widget.h>
#include <util/autovaluereset.h>
#include <util/spanexpr.h>


using namespace richmath;

bool richmath::DebugFollowMouse     = false;
bool richmath::DebugSelectionBounds = false;

static double MaxFlashingCursorRadius = 9;  /* pixels */
static double MaxFlashingCursorTime = 0.15; /* seconds */

Hashtable<String, Expr, object_hash> richmath::global_immediate_macros;
Hashtable<String, Expr, object_hash> richmath::global_macros;

static Box *expand_selection_default(Box *box, int *start, int *end) {
  int index = box->index();
  Box *box2 = box->parent();
  while(box2) {
    if(dynamic_cast<AbstractSequence *>(box2)) {
      if(box2->selectable()) {
        *start = index;
        *end = index + 1;
        return box2;
      }
    }
    
    index = box2->index();
    box2 = box2->parent();
  }
  
  return box;
}

Box *expand_selection_math(MathSequence *seq, int *start, int *end) {
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
    if(Span s = seq->span_array()[*start]) {
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
  
  int orig_start = *start;
  int orig_end = *end;
  const uint16_t *buf = seq->text().buffer();
  
  int a = *start;
  while(--a >= 0) {
    if(Span s = seq->span_array()[a]) {
      int e = s.end();
      while(s && s.end() + 1 >= *end) {
        e = s.end();
        s = s.next();
      }
      
      if(e + 1 >= *end) {
        *start = a;
        while(*start < orig_start && buf[*start] == '\n')
          ++*start;
        *end = e + 1;
        if(*start == orig_start && *end == orig_end && orig_end < seq->length()) {
          ++*end;
          ++a;
          continue;
        }
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
  
  return expand_selection_default(seq, start, end);
}

Box *expand_selection_text(TextSequence *seq, int *start, int *end) {
  if(*start == 0 && *end == seq->length())
    return expand_selection_default(seq, start, end);
    
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
  
  const char *word_end = nullptr;
  
  while(s && !word_end) {
    if(attrs[i].is_word_boundary && word_end != word_start)
      word_end = s;
      
    ++i;
    s = g_utf8_find_next_char(s, s_end);
  }
  
  g_free(attrs);
  attrs = nullptr;
  
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

Box *richmath::expand_selection(Box *box, int *start, int *end) {
  if(!box)
    return nullptr;
    
  if(auto seq = dynamic_cast<MathSequence *>(box)) {
    return expand_selection_math(seq, start, end);
  }
  else if(auto seq = dynamic_cast<TextSequence *>(box)) {
    return expand_selection_text(seq, start, end);
  }
  
  return expand_selection_default(box, start, end);
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

static int index_of_replacement(const String &s) {
  const uint16_t *buf = s.buffer();
  int             len = s.length();
  
  if(len <= 1)
    return -1;
    
  for(int i = 0; i < len; ++i)
    if(buf[i] == CHAR_REPLACEMENT)
      return i;
      
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_PLACEHOLDER)
      return i;
      
  return -1;
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
RESTART:
  if(auto seq = dynamic_cast<MathSequence *>(box)) {
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
        box = box->item(b);
        goto RESTART;
      }
    }
    
    for(; i < seqlen; ++i) {
      if(seqbuf[i] == PMATH_CHAR_BOX) {
        int b = 0;
        while(box->item(b)->index() < i)
          ++b;
          
        *index = 0;
        box = box->item(b);
        goto RESTART;
      }
    }
  }
  else if(box == stop_box) {
    if(*index >= stop_index || *index >= box->count())
      return 0;
      
    int i = *index;
    *index = 0;
    box = box->item(i);
    goto RESTART;
  }
  else if(*index < box->count()) {
    int i = *index;
    *index = 0;
    box = box->item(i);
    goto RESTART;
  }
  
  if(box->parent()) {
    *index = box->index() + 1;
    box = box->parent();
    goto RESTART;
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

namespace richmath {
  class DocumentImpl {
    private:
      Document &self;
      
    public:
      DocumentImpl(Document &_self);
      
    public:
      void raw_select(Box *box, int start, int end);
      void after_resize_section(int i);
      
      //{ selection highlights
    private:
      void add_fill(PaintHookManager &hooks, Box *box, int start, int end, int color, float alpha = 1.0f);
      void add_pre_fill(Box *box, int start, int end, int color, float alpha = 1.0f);
      void add_selected_word_highlight_hooks(int first_visible_section, int last_visible_section);
      bool word_occurs_outside_visible_range(String str, int first_visible_section, int last_visible_section);
      void add_matching_bracket_hook();
      void add_autocompletion_hook();
      
    public:
      void add_selection_highlights(int first_visible_section, int last_visible_section);
      //}
      
      //{ cursor painting
    private:
      void paint_document_cursor();
      void paint_flashing_cursor_if_needed();
      
    public:
      void paint_cursor_and_flash();
      //}
      
      //{ insertion
      bool is_inside_string();
      static bool is_inside_string(Box *box, int index);
      bool is_inside_alias();
      
      // substart and subend may lie outside 0..subbox->length()
      bool is_inside_selection(Box *subbox, int substart, int subend);
      bool is_inside_selection(Box *subbox, int substart, int subend, bool was_inside_start);
      
      void set_prev_sel_line();
      
      bool prepare_insert();
      bool prepare_insert_math(bool include_previous_word);
      
      Section *auto_make_text_or_math(Section *sect);
      
    private:
      template<class FromSectionType, class ToSectionType>
      Section *convert_content(Section *sect);
      
    public:
      //}
      
      //{ key events
      void handle_key_left_right(SpecialKeyEvent &event, LogicalDirection direction);
      void handle_key_home_end(SpecialKeyEvent &event, LogicalDirection direction);
      void handle_key_up_down(SpecialKeyEvent &event, LogicalDirection direction);
      void handle_key_pageup_pagedown(SpecialKeyEvent &event, LogicalDirection direction);
      void handle_key_tab(SpecialKeyEvent &event);
      
    private:
      bool is_tabkey_only_moving();
      
    public:
      void handle_key_backspace(SpecialKeyEvent &event);
      void handle_key_delete(SpecialKeyEvent &event);
      void handle_key_escape(SpecialKeyEvent &event);
      //}
      
      //{ macro handling
    private:
      bool handle_immediate_macros(const Hashtable<String, Expr, object_hash> &table);
      bool handle_macros(const Hashtable<String, Expr, object_hash> &table);
      
    public:
      bool handle_immediate_macros();
      bool handle_macros();
      //}
  };
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
    mouse_down_y(0),
    auto_completion(this)
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

bool Document::try_load_from_object(Expr expr, BoxInputFlags options) {
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
  load_stylesheet();
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
  if(get_own_style(InternalRequiresChildResize)) {
    style->set(InternalRequiresChildResize, false);
    invalidate_all();
  }
  
  if(get_own_style(InternalHasModifiedWindowOption)) {
    style->set(InternalHasModifiedWindowOption, false);
    
    native()->invalidate_options();
  }
  else
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
  
  if(y < _y && y + h < _y + _h) {
    _y = y - _h / 6.f;
    if(y + h > _y + _h)
      _y = y + h - _h;
  }
  else if(y + h >= _y + _h && y > _y) {
    _y = y + h - _h * 5 / 6.f;
    if(y < _y)
      _y = y;
  }
  
  if(x < _x && x + w < _x + _w) {
    _x = x - _w / 6.f;
    if(x + w > _x + _w)
      _x = x + w - _w;
  }
  else if(x + w >= _x + _w && x > _x) {
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
  
  //Application::update_control_active(native()->is_mouse_down());
  
  Application::delay_dynamic_updates(true);
  if(++mouse_down_counter == 1) {
    event.set_origin(this);
    
    bool was_inside_start;
    int start, end;
    receiver = mouse_selection(
                 event.x, event.y,
                 &start, &end,
                 &was_inside_start);
                 
    receiver = receiver ? receiver->mouse_sensitive() : this;
    assert(receiver != nullptr);
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
  
  //Application::update_control_active(native()->is_mouse_down());
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
  
  if(b1 == b2) // selection overlaps except_box
    return;
    
  while(b1 != b2 && b1 && b2) {
    b1 = b1->parent();
    b2 = b2->parent();
  }
  
  b1 = selection_box(); // b2 = convex hull of selection and except_box
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
    event.set_origin(this);
    
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
  
  if(Box *sel = selection_box()) {
    sel->on_exit();
    
    if(!sel->selectable())
      select(0, 0, 0);
    else if(selection_length() > 0)
      sel->request_repaint_range(selection_start(), selection_end());
  }
}

void Document::key_down(SpecialKeyEvent &event) {
  native()->hide_tooltip();
  
  if(Box *selbox = context.selection.get()) {
    selbox->on_key_down(event);
  }
  else {
    Document *cur = get_current_document();
    if(cur && cur != this)
      cur->key_down(event);
  }
}

void Document::key_up(SpecialKeyEvent &event) {
  if(Box *selbox = context.selection.get()) {
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
  
  if(Box *selbox = context.selection.get()) {
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
  
  auto_completion.stop();
  
  event.set_origin(this);
  
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
          
          if(auto seq = dynamic_cast<MathSequence *>(selbox)) {
            should_expand = false;
            while(start > 0 && !seq->span_array().is_token_end(start - 1))
              --start;
              
            while(end < seq->length() && !seq->span_array().is_token_end(end - 1))
              ++end;
          }
        }
        
        if(should_expand)
          selbox = expand_selection(selbox, &start, &end);
          
        //select_range(selbox, start, start, selbox, end, end);
        select(selbox, start, end);
      }
    }
    else if(DocumentImpl(*this).is_inside_selection(box, start, end, was_inside_start)) {
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
  event.set_origin(this);
  
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
  
  if(!event.left && DocumentImpl(*this).is_inside_selection(box, start, end, was_inside_start)) {
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
    if(Box *mouse_down_box = mouse_down_sel.get()) {
      Section *sec1 = mouse_down_box->find_parent<Section>(true);
      Section *sec2 = box ? box->find_parent<Section>(true) : 0;
      
      if(sec1 && sec1 != sec2) {
        event.set_origin(sec1);
        box = sec1->mouse_selection(event.x, event.y, &start, &end, &was_inside_start);
      }
      
      select_range(
        mouse_down_box, mouse_down_sel.start, mouse_down_sel.end,
        box, start, end);
    }
  }
}

void Document::on_mouse_up(MouseEvent &event) {
  event.set_origin(this);
  
  if(event.left && drag_status != DragStatusIdle) {
    bool was_inside_start;
    int start, end;
    Box *box = mouse_selection(
                 event.x, event.y,
                 &start, &end,
                 &was_inside_start);
                 
    if( DocumentImpl(*this).is_inside_selection(box, start, end, was_inside_start) &&
        box &&
        box->selectable())
    {
      select(box, start, end);
    }
  }
  
  drag_status = DragStatusIdle;
}

void Document::on_mouse_cancel() {
  drag_status = DragStatusIdle;
}

void Document::on_key_down(SpecialKeyEvent &event) {
  switch(event.key) {
    case SpecialKey::Left:
      DocumentImpl(*this).handle_key_left_right(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::Right:
      DocumentImpl(*this).handle_key_left_right(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Home:
      DocumentImpl(*this).handle_key_home_end(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::End:
      DocumentImpl(*this).handle_key_home_end(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Up:
      DocumentImpl(*this).handle_key_up_down(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::Down:
      DocumentImpl(*this).handle_key_up_down(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::PageUp:
      DocumentImpl(*this).handle_key_pageup_pagedown(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::PageDown:
      DocumentImpl(*this).handle_key_pageup_pagedown(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Tab:
      DocumentImpl(*this).handle_key_tab(event);
      return;
      
    case SpecialKey::Backspace:
      DocumentImpl(*this).handle_key_backspace(event);
      return;
      
    case SpecialKey::Delete:
      DocumentImpl(*this).handle_key_delete(event);
      return;
      
    case SpecialKey::Escape:
      DocumentImpl(*this).handle_key_escape(event);
      return;
      
    default: return;
  }
}

void Document::on_key_up(SpecialKeyEvent &event) {
}

void Document::on_key_press(uint32_t unichar) {
  AbstractSequence *initial_seq = dynamic_cast<AbstractSequence *>(selection_box());
  
  if(!DocumentImpl(*this).prepare_insert()) {
    Document *cur = get_current_document();
    
    if(cur && cur != this)
      cur->key_press(unichar);
    else
      native()->beep();
      
    return;
  }
  
  auto_completion.stop();
  
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
            
          if(lang.equals("NaturalLanguage"))
            new_sect = new TextSection(new_style);
          else
            new_sect = new MathSection(new_style);
            
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
    if(auto seq = dynamic_cast<TextSequence *>(context.selection.get())) {
      selstr = String::FromUtf8(
                 seq->text_buffer().buffer() + context.selection.start,
                 context.selection.end - context.selection.start);
    }
    else if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
      selstr = seq->text().part(
                 context.selection.start,
                 context.selection.end - context.selection.start);
                 
      can_surround = !seq->is_placeholder(context.selection.start);
    }
    
    if(can_surround && selstr.length() == 1) {
      can_surround = '\\' != *selstr.buffer() &&
                     !pmath_char_is_left( *selstr.buffer()) &&
                     !pmath_char_is_right(*selstr.buffer());
    }
    
    if(can_surround) {
      if(unichar == '/' && !selstr.starts_with("/*")) {
        if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
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
  
  bool had_empty_selection = (context.selection.start == context.selection.end);
  
  remove_selection(false);
  
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
  
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
              int repl_index = index_of_replacement(ins);
              
              if(repl_index >= 0) {
                int new_sel_start = seq->insert(alias_end, ins.part(0, repl_index));
                int new_sel_end   = seq->insert(new_sel_start, PMATH_CHAR_PLACEHOLDER);
                seq->insert(new_sel_end, ins.part(repl_index + 1));
                seq->remove(alias_pos, alias_end);
                
                select(seq, new_sel_start - (alias_end - alias_pos), new_sel_end - (alias_end - alias_pos));
              }
              else {
                int i = seq->insert(alias_end, ins);
                seq->remove(alias_pos, alias_end);
                move_to(seq, i - (alias_end - alias_pos));
              }
              
              return;
            }
            else {
              MathSequence *repl_seq = new MathSequence();
              repl_seq->load_from_object(repl, BoxInputFlags::Default);
              
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
    
    bool was_inside_string = DocumentImpl(*this).is_inside_string();
    bool was_inside_alias  = DocumentImpl(*this).is_inside_alias();
    
    if(!was_inside_string && !was_inside_alias) {
      DocumentImpl(*this).handle_immediate_macros();
    }
    
    int newpos = seq->insert(context.selection.start, unichar);
    move_to(seq, newpos);
    
    if(mseq && !was_inside_string && !was_inside_alias) {
      // handle "\alias" macros:
      if(unichar == ' '/* && !was_inside_string*/) {
        context.selection.start--;
        context.selection.end--;
        
        bool ok = DocumentImpl(*this).handle_macros();
        
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
        
        if(!DocumentImpl(*this).handle_macros())
          move_to(mseq, start);
      }
    }
    else if(had_empty_selection && mseq && was_inside_string && !was_inside_alias) {
      if(unichar == '\\') {
        int newpos = seq->insert(context.selection.start, '\\');
        select(seq, context.selection.start, newpos);
      }
    }
  }
  else
    native()->beep();
}

//} ... event handlers

void Document::select(Box *box, int start, int end) {
  if(box && !box->selectable())
    return;
    
  sel_last.set(box, start, end);
  sel_first = sel_last;
  auto_scroll = !native()->is_mouse_down();//(mouse_down_counter == 0);
  
  DocumentImpl(*this).raw_select(box, start, end);
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
  if((box1 && !box1->selectable()) || (box2 && !box2->selectable()))
    return;
    
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
      if(b1->selectable()) {
        int o1 = box_order(b1, s1, b2, e2);
        int o2 = box_order(b1, e1, b2, s2);
        
        if(o1 > 0)
          DocumentImpl(*this).raw_select(b1, 0, e1);
        else if(o2 < 0)
          DocumentImpl(*this).raw_select(b1, s1, b1->length());
        else
          DocumentImpl(*this).raw_select(b1, 0, b1->length());
      }
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
      if(b1->selectable()) {
        int o = box_order(b1, s1, b2, s2);
        
        if(o < 0)
          DocumentImpl(*this).raw_select(b1, s1, b1->length());
        else
          DocumentImpl(*this).raw_select(b1, 0, e1);
      }
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
    e1 = e2;
    
  while(b1 && !b1->selectable()) {
    if(!b1->exitable())
      return;
      
    s1 = b1->index();
    e1 = s1 + 1;
    b1 = b1->parent();
  }
  if(b1)
    DocumentImpl(*this).raw_select(b1, s1, e1);
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
  if(direction == LogicalDirection::Forward)
    i = sel_last.end;
    
  if(sel_last.start != sel_last.end) {
    if(box == selbox) {
      if(direction == LogicalDirection::Forward) {
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
    if(direction == LogicalDirection::Forward)
      ++i;
      
    box = box->parent();
  }
  
  if(selecting) {
    select_to(box, i, i);
  }
  else {
    int j = i;
    if(auto seq = dynamic_cast<MathSequence *>(box)) {
      if(direction == LogicalDirection::Forward) {
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
      
      if(direction == LogicalDirection::Forward && j < length())
        ++j;
      else if(direction == LogicalDirection::Backward && i > 0)
        --i;
        
      select(this, i, j);
      return;
    }
    
    if(direction == LogicalDirection::Forward)
      move_to(this, sel_last.end);
    else
      move_to(this, sel_last.start);
      
    return;
  }
  
  int i, j;
  if(direction == LogicalDirection::Forward && sel_last.start + 1 < sel_last.end) {
    i = sel_last.end;
    if( sel_last.start < sel_last.end &&
        !dynamic_cast<MathSequence *>(box))
    {
      --i;
    }
  }
  else
    i = sel_last.start;
    
  box = box->move_vertical(direction, &best_index_rel_x, &i, false);
  
  j = i;
  if(auto seq = dynamic_cast<MathSequence *>(box)) {
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
      direction == LogicalDirection::Forward)
  {
    index = context.selection.end;
  }
  
  while(box) {
    auto parent = box->parent();
    if(!parent || !parent->exitable() || !parent->selectable())
      break;
      
    index = box->index();
    box = parent;
  }
  
  if(auto seq = dynamic_cast<MathSequence *>(box)) {
    int l = seq->line_array().length() - 1;
    while(l > 0 && seq->line_array()[l - 1].end > index)
      --l;
      
    if(l >= 0) {
      if(direction == LogicalDirection::Forward) {
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
  else if(auto seq = dynamic_cast<TextSequence *>(box)) {
    GSList *lines = pango_layout_get_lines_readonly(seq->get_layout());
    
    if(direction == LogicalDirection::Backward) {
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
    if(context.selection.start < context.selection.end && direction == LogicalDirection::Forward)
      index = context.selection.end - 1;
      
    if(section(index)->length() > 0) {
      if(direction == LogicalDirection::Forward) {
        direction = LogicalDirection::Backward;
        ++index;
      }
      else {
        direction = LogicalDirection::Forward;
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
  if(direction == LogicalDirection::Forward) {
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
      
      if(direction == LogicalDirection::Forward)
        --i;
      else
        ++i;
    }
    
    AbstractSequence *seq = dynamic_cast<AbstractSequence *>(box);
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
    if(auto parent = dynamic_cast<AbstractSequence *>(box->parent())) {
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
  
  BoxOutputFlags flags = BoxOutputFlags::Default;
  if(mimetype.equals(Clipboard::PlainText))
    flags |= BoxOutputFlags::Literal | BoxOutputFlags::ShortNumbers;
    
  Expr boxes = selbox->to_pmath(flags, start, end);
  if(mimetype.equals(Clipboard::BoxesText))
    return boxes.to_string(PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR);
    
  if( mimetype.equals("InputText"/*Clipboard::PlainText*/) ||
      mimetype.equals(Clipboard::PlainText) ||
      mimetype.equals("PlainText"))
  {
    Expr text = Application::interrupt_wait(
                  Parse("FE`BoxesToText(`1`, `2`)", boxes, mimetype),
                  Application::edit_interrupt_timeout);
                  
    return text.to_string();
  }
  
//  // plaintext:
//  if( mimetype.equals(Clipboard::PlainText) ||
//      mimetype.equals("PlainText"))
//  {
//    boxes = Call(Symbol(PMATH_SYMBOL_RAWBOXES), boxes);
//    return boxes.to_string(0);
//  }

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
    
    Expr boxes = selbox->to_pmath(BoxOutputFlags::Default, start, end);
    file = Expr(pmath_file_create_compressor(file.release(), nullptr));
    pmath_serialize(file.get(), boxes.release(), 0);
    pmath_file_close(file.release());
    return;
  }
  
  String text = copy_to_text(mimetype);
  // pmath_file_write(file.get(), text.buffer(), 2 * (size_t)text.length());
  pmath_file_writetext(file.get(), text.buffer(), text.length());
}

void Document::copy_to_image(cairo_surface_t *target, bool calc_size_only, double *device_width, double *device_height) {
  *device_width  = 0;
  *device_height = 0;
  
  SelectionReference copysel = context.selection;
  Box *selbox = copysel.get();
  if(!selbox)
    return;
    
  if(dynamic_cast<GraphicsBox *>(selbox)) {
    copysel.set(selbox->parent(), selbox->index(), selbox->index() + 1);
    
    selbox = copysel.get();
    if(!selbox)
      return;
  }
  
  SelectionReference oldsel = context.selection;
  context.selection = SelectionReference();
  
  selbox->invalidate();
  cairo_t *cr = cairo_create(target);
  {
    Canvas canvas(cr);
    
    float sf = native()->scale_factor();
    
    canvas.scale(sf, sf);
    
    switch(cairo_surface_get_type(target)) {
      case CAIRO_SURFACE_TYPE_IMAGE:
      case CAIRO_SURFACE_TYPE_WIN32:
        canvas.pixel_device = true;
        break;
        
      default:
        canvas.pixel_device = false;
        break;
    }
    
    cairo_set_line_width(cr, 1);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
    canvas.set_font_size(10);// 10 * 4/3.
    
    paint_resize(&canvas, true);
    
    selbox = copysel.get();
    ::selection_path(&canvas, selbox, copysel.start, copysel.end);
    
    double x1, y1, x2, y2;
    canvas.path_extents(&x1, &y1, &x2, &y2);
    canvas.new_path();
    
    *device_width  = (x2 - x1) * sf;
    *device_height = (y2 - y1) * sf;
    
    if(!calc_size_only) {
      float sx, sy;
      native()->scroll_pos(&sx, &sy);
      canvas.translate(sx, sy);
      canvas.translate(-x1, -y1);
      
      if(0 == (CAIRO_CONTENT_ALPHA & cairo_surface_get_content(target))) {
        int color = get_style(Background, -1);
        if(color >= 0) {
          canvas.set_color(color);
          canvas.paint();
        }
        else {
          canvas.set_color(0xFFFFFF);
          canvas.paint();
        }
      }
      
      ::selection_path(&canvas, selbox, copysel.start, copysel.end);
      canvas.clip();
      
      canvas.set_color(get_style(FontColor, 0));
      
      canvas.translate(sx, sy);
      paint_resize(&canvas, false);
    }
  }
  cairo_destroy(cr);
  cairo_surface_flush(target);
  
  context.selection = oldsel;
}

void Document::copy_to_clipboard(String mimetype) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    SharedPtr<OpenedClipboard> cb = Clipboard::std->open_write();
    if(!cb) {
      native()->beep();
      return;
    }
    
    Expr file = Expr(pmath_file_create_binary_buffer(0));
    copy_to_binary(Clipboard::BoxesBinary, file);
    cb->add_binary_buffer(Clipboard::BoxesBinary, file);
    return;
  }
  
  if( mimetype.equals(Clipboard::BoxesText) ||
      mimetype.equals(Clipboard::PlainText) ||
      mimetype.equals("PlainText"))
  {
    SharedPtr<OpenedClipboard> cb = Clipboard::std->open_write();
    if(!cb) {
      native()->beep();
      return;
    }
    
    cb->add_text(Clipboard::PlainText, copy_to_text(mimetype));
    return;
  }
  
  if(cairo_surface_t *image = Clipboard::std->create_image(mimetype, 1, 1)) {
    SharedPtr<OpenedClipboard> cb = Clipboard::std->open_write();
    if(!cb) {
      native()->beep();
      return;
    }
    
    double dw, dh;
    copy_to_image(image, true, &dw, &dh);
    
    cairo_surface_destroy(image);
    image = Clipboard::std->create_image(mimetype, dw, dh);
    if(image) {
      copy_to_image(image, false, &dw, &dh);
      cb->add_image(mimetype, image);
      cairo_surface_destroy(image);
    }
  }
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
  cb->add_text(Clipboard::PlainText, copy_to_text("InputText"/*Clipboard::PlainText*/));
}

void Document::cut_to_clipboard() {
  copy_to_clipboard();
  
  int start, end;
  if(Box *box = prepare_copy(&start, &end)) {
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
  
  boxes = Application::interrupt_wait(
            Parse("FE`SectionsToBoxes(`1`)", boxes),
            Application::edit_interrupt_timeout);
            
  GridBox *grid = dynamic_cast<GridBox *>(context.selection.get());
  if(grid && grid->get_style(Editable)) {
    int row1, col1, row2, col2;
    
    grid->matrix().index_to_yx(context.selection.start, &row1, &col1);
    grid->matrix().index_to_yx(context.selection.end - 1, &row2, &col2);
    
    int w = col2 - col1 + 1;
    int h = row2 - row1 + 1;
    
    BoxInputFlags options = BoxInputFlags::Default;
    if(grid->get_style(AutoNumberFormating))
      options |= BoxInputFlags::FormatNumbers;
      
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
                Expr(tmpgrid->item(row, col)->to_pmath(BoxOutputFlags::Default)),
                BoxInputFlags::FormatNumbers);
            }
            else {
              grid->item(row1 + row, col1 + col)->load_from_object(
                String::FromChar(PMATH_CHAR_BOX),
                BoxInputFlags::Default);
            }
          }
        }
        
        MathSequence *sel = grid->item(
                              row1 + tmpgrid->rows() - 1,
                              col1 + tmpgrid->cols() - 1)->content();
                              
        move_to(sel, sel->length());
        grid->invalidate();
        
        tmp->safe_destroy();
        return;
      }
    }
    
    for(int col = 0; col < w; ++col) {
      for(int row = 0; row < h; ++row) {
        grid->item(row1 + row, col1 + col)->load_from_object(
          String::FromChar(PMATH_CHAR_BOX),
          BoxInputFlags::Default);
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
    BoxInputFlags options = BoxInputFlags::Default;
    if(graphics->get_style(AutoNumberFormating))
      options |= BoxInputFlags::FormatNumbers;
      
    if(graphics->try_load_from_object(boxes, options))
      return;
      
    //select(graphics->parent(), graphics->index(), graphics->index() + 1);
  }
  
  remove_selection(false);
  
  if(DocumentImpl(*this).prepare_insert()) {
    if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
    
      BoxInputFlags options = BoxInputFlags::Default;
      if(seq->get_style(AutoNumberFormating))
        options |= BoxInputFlags::FormatNumbers;
        
      MathSequence *tmp = new MathSequence;
      tmp->load_from_object(boxes, options);
      
      int newpos = context.selection.end + tmp->length();
      seq->insert(context.selection.end, tmp);
      
      select(seq, newpos, newpos);
      
      return;
    }
    
    if(auto seq = dynamic_cast<TextSequence *>(context.selection.get())) {
    
      BoxInputFlags options = BoxInputFlags::Default;
      if(seq->get_style(AutoNumberFormating))
        options |= BoxInputFlags::FormatNumbers;
        
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
    Expr parsed = Application::interrupt_wait(
                    Expr(
                      pmath_parse_string(data.release())),
                    Application::edit_interrupt_timeout);
                    
    paste_from_boxes(parsed);
    return;
  }
  
  if(mimetype.equals(Clipboard::PlainText)) {
    bool doc_was_selected = selection_box() == this;
    
    if(DocumentImpl(*this).prepare_insert()) {
      remove_selection(false);
      
      data = String(Evaluate(Parse("`1`.StringReplace(\"\\r\\n\"->\"\\n\")", data)));
      
      if (doc_was_selected && data.length() > 0 && data[data.length() - 1] == '\n')
        data = data.part(0, data.length() - 1);
        
      insert_string(data);
      
      return;
    }
  }
  
  native()->beep();
}

void Document::paste_from_binary(String mimetype, Expr file) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    pmath_serialize_error_t err = PMATH_SERIALIZE_OK;
    file = Expr(pmath_file_create_decompressor(file.release(), nullptr));
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

void Document::set_selection_style(Expr options) {
  Box *sel = selection_box();
  if(!sel)
    return;
    
  int start = selection_start();
  int end   = selection_end();
  
  if(sel->parent() == this) { // single section
    start = sel->index();
    end = start + 1;
    sel = sel->parent();
  }
  
  if(sel == this && start < end) {
    if(!get_style(Editable, true))
      return;
      
    native()->on_editing();
    
    for(int i = start; i < end; ++i) {
      Section *sect = section(i);
      
      if(!sect->style)
        sect->style = new Style();
        
      String old_basestyle = sect->get_own_style(BaseStyleName);
      
      sect->style->add_pmath(options);
      
      if(old_basestyle != sect->get_own_style(BaseStyleName))
        sect = DocumentImpl(*this).auto_make_text_or_math(sect);
        
      sect->invalidate();
    }
    
    return;
  }
  
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(sel);
  if(seq && start < end) {
  
    if(!seq->edit_selection(&context))
      return;
      
    StyleBox *style_box = nullptr;
    if(start == 0 && end == seq->length()) {
      style_box = dynamic_cast<StyleBox *>(seq->parent());
    }
    else {
      int i = start;
      Box *box = seq->move_logical(LogicalDirection::Forward, false, &i);
      if(box && box != seq && box->parent() == seq) {
        i = start;
        if(seq->move_logical(LogicalDirection::Forward, true, &i) == seq && i >= end) {
          // selection containt a single box
          
          style_box = dynamic_cast<StyleBox *>(box);
        }
      }
    }
    
    native()->on_editing();
    DocumentImpl(*this).set_prev_sel_line();
    
    if(!style_box) {
      style_box = new StyleBox();
      style_box->style->add_pmath(options);
      style_box->content()->insert(0, seq, start, end);
      
      seq->insert(end, style_box);
      seq->remove(start, end);
      
      move_to(style_box->content(), style_box->content()->length());
    }
    
    style_box->style->add_pmath(options);
    style_box->invalidate();
    return;
  }
}

MenuCommandStatus Document::can_do_scoped(Expr cmd, Expr scope) {
  SelectionReference old_sel = context.selection;
  SelectionReference new_sel;
  
  AutoValueReset<MenuCommandScope> auto_reset(Application::menu_command_scope);
  Application::menu_command_scope = MenuCommandScope::Selection;
  
  if(scope == PMATH_SYMBOL_DOCUMENT) {
    Application::menu_command_scope = MenuCommandScope::Document;
    new_sel.set(this, 0, 0);
  }
  else if(scope == PMATH_SYMBOL_SECTION) {
    Box *box = old_sel.get();
    if(box)
      box = box->find_parent<Section>(true);
      
    new_sel.set(box, 0, 0);
  }
  
  if(!new_sel.get())
    return false;
    
  context.selection = new_sel;
  
  MenuCommandStatus result = Application::test_menucommand_status(cmd);
  
  context.selection = old_sel;
  return result;
}

bool Document::do_scoped(Expr cmd, Expr scope) {
  SelectionReference old_sel = context.selection;
  SelectionReference new_sel;
  
  AutoValueReset<MenuCommandScope> auto_reset(Application::menu_command_scope);
  Application::menu_command_scope = MenuCommandScope::Selection;
  
  if(scope == PMATH_SYMBOL_DOCUMENT) {
    Application::menu_command_scope = MenuCommandScope::Document;
    new_sel.set(this, 0, 0);
  }
  else if(scope == PMATH_SYMBOL_SECTION) {
    Box *box = old_sel.get();
    if(box) {
      if(box == this && old_sel.start + 1 == old_sel.end)
        box = section(old_sel.start);
      else
        box = box->find_parent<Section>(true);
    }
      
    new_sel.set(box, 0, 0);
  }
  
  if(!new_sel.get())
    return false;
    
  context.selection = new_sel;
  
  bool result = Application::run_recursive_menucommand(cmd);
  
  if(is_parent_of(old_sel.get()))
    context.selection = old_sel;
  
  return result;
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
  move_horizontal(LogicalDirection::Forward, false);
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

void Document::graphics_original_size() {
  Box *selbox = selection_box();
  
  if(dynamic_cast<GraphicsBox *>(selbox)) {
    graphics_original_size(selbox);
    return;
  }
  
  if(!selbox || selection_length() == 0)
    return;
    
  for(int i = 0; i < selbox->count(); ++i) {
    Box *sub = selbox->item(i);
    
    int idx = sub->index();
    if(idx < selection_start())
      continue;
      
    if(idx >= selection_end())
      break;
      
    graphics_original_size(sub);
  }
}

void Document::graphics_original_size(Box *box) {
  if(GraphicsBox *gb = dynamic_cast<GraphicsBox *>(box)) {
    gb->reset_user_options();
    return;
  }
  
  for(int i = 0; i < box->count(); ++i)
    graphics_original_size(box->item(i));
}

void Document::insert_string(String text, bool autoformat) {
  const uint16_t *buf = text.buffer();
  int             len = text.length();
  
  if(!DocumentImpl(*this).prepare_insert()) {
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
  
  if(auto seq = dynamic_cast<TextSequence *>(selection_box())) {
    int i = seq->insert(selection_start(), text);
    move_to(seq, i);
    return;
  }
  
  if(autoformat) {
    if(DocumentImpl(*this).is_inside_string()) {
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
                pmath_token_t tok = pmath_token_analyse(&buf[i + 1], 1, nullptr);
                
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
    auto seq = new MathSequence;
    seq->insert(0, text);
    
    if(autoformat && !DocumentImpl(*this).is_inside_string()) { // replace tokens from global_immediate_macros ...
      seq->ensure_spans_valid();
      const SpanArray &spans = seq->span_array();
      
      MathSequence *seq2 = 0;
      int last = 0;
      int pos = 0;
      while(pos < len) {
        if(buf[pos] == '"') {
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
        while(next + 1 < len && !spans.is_token_end(next))
          ++next;
        ++next;
        
        if(Expr *e = global_immediate_macros.search(text.part(pos, next - pos))) {
          if(!seq2)
            seq2 = new MathSequence;
            
          seq2->insert(seq2->length(), text.part(last, pos - last));
          
          auto seq_tmp = new MathSequence;
          seq_tmp->load_from_object(*e, BoxInputFlags::Default);
          seq2->insert(seq2->length(), seq_tmp);
          
          last = next;
        }
        
        pos = next;
      }
      
      if(seq2) {
        seq2->insert(seq2->length(), text.part(last, len - last));
        seq->safe_destroy();
        seq = seq2;
      }
    }
    
    insert_box(seq, false);
  }
}

void Document::insert_box(Box *box, bool handle_placeholder) {
  if(!box || !DocumentImpl(*this).prepare_insert()) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_box(box, handle_placeholder);
    }
    else {
      native()->beep();
      box->safe_destroy();
    }
    
    return;
  }
  
  assert(box->parent() == 0);
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  if(auto seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
    Box *new_sel_box = nullptr;
    int new_sel_start = 0;
    int new_sel_end = 0;
    
    if(handle_placeholder) {
      AbstractSequence *placeholder_seq = nullptr;
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
        
        current = current->move_logical(LogicalDirection::Forward, false, &i);
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
              current = current->move_logical(LogicalDirection::Forward, false, &i);
              
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
                
                current = current->move_logical(LogicalDirection::Forward, false, &i);
              }
            }
          }
        }
      }
    }
    
    seq = dynamic_cast<AbstractSequence *>(context.selection.get());
    if(!seq) {
      box->safe_destroy();
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
  
  box->safe_destroy();
}

void Document::insert_fraction() {
  if(!DocumentImpl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_fraction();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  select_prev(true);
  
  if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
    auto num = new MathSequence;
    auto den = new MathSequence;
    
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
  if(!DocumentImpl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_column();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  GridBox *grid = 0;
  
  MathSequence *seq = dynamic_cast<MathSequence *>(context.selection.get());
  
  if( context.selection.start == context.selection.end ||
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
  if(!DocumentImpl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_row();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
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
  if(!DocumentImpl(*this).prepare_insert_math(false)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_sqrt();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
    auto content = new MathSequence;
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
  if(!DocumentImpl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_subsuperscript(sub);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
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
  if(!DocumentImpl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_underoverscript(under);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !DocumentImpl(*this).is_inside_string() &&
      !DocumentImpl(*this).is_inside_alias() &&
      !DocumentImpl(*this).handle_immediate_macros())
  {
    DocumentImpl(*this).handle_macros();
  }
  
  select_prev(false);
  
  if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
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
    
  auto_completion.stop();
  
  if(auto seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
    native()->on_editing();
    
    if(auto mseq = dynamic_cast<MathSequence *>(seq)) {
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
  
  if(auto grid = dynamic_cast<GridBox *>(context.selection.get())) {
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
  Box *b = selection_box();
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
  drag_status = DragStatusIdle;
  
  mouse_down_counter = 0;
  Application::delay_dynamic_updates(false);
  
  Application::deactivated_all_controls();
  //Application::update_control_active(native()->is_mouse_down());
}

void Document::stylesheet(SharedPtr<Stylesheet> new_stylesheet) {
  assert(new_stylesheet);
  if(new_stylesheet != context.stylesheet) {
    context.stylesheet = new_stylesheet;
    invalidate_all();
  }
}

bool Document::load_stylesheet() {
  Expr styledef = get_own_style(StyleDefinitions);
  Expr last_styledef = get_own_style(InternalLastStyleDefinitions);
  if(styledef != last_styledef) {
    SharedPtr<Stylesheet> new_stylesheet = Stylesheet::try_load(styledef);
    if(new_stylesheet) {
      context.stylesheet = new_stylesheet;
      style->set(InternalLastStyleDefinitions, styledef);
      return true;
    }
  }
  return false;
}

void Document::reset_style() {
  if(style)
    style->clear();
  else
    style = new Style;
    
  style->set(BaseStyleName, "Document");
}

void Document::paint_resize(Canvas *canvas, bool resize_only) {
  update_dynamic_styles(&context);
  if(get_own_style(InternalRequiresChildResize)) {
    style->set(InternalRequiresChildResize, false);
    if(resize_only) {
      invalidate_all();
    }
    else {
      must_resize_min = count();
      for(int i = 0;i < length();++i)
        section(i)->must_resize = true;
    }
  }
  
  additional_selection.length(0);
  
  context.canvas = canvas;
  if(style) {
    ContextState cc(&context);
    cc.begin(style);
  }
  
  float scrolly, page_height;
  native()->window_size(&_window_width, &page_height);
  native()->page_size(&_page_width, &page_height);
  native()->scroll_pos(&_scrollx, &scrolly);
  
  context.fontfeatures.clear();
  context.fontfeatures.add(context.stylesheet->get_with_base(style, FontFeatures));
  
  context.width = _page_width;
  context.section_content_window_width = _window_width;
  
  if(_page_width < HUGE_VAL)
    _extents.width = _page_width;
  else
    _extents.width = 0;
    
  unfilled_width = 0;
  _extents.ascent = _extents.descent = 0;
  
  canvas->translate(-_scrollx, -scrolly);
  
  init_section_bracket_sizes(&context);
  
  int sel_sect = -1;
  Box *b = context.selection.get();
  while(b && b != this) {
    sel_sect = b->index();
    b = b->parent();
  }
  
  int i = 0;
  while(i < length() && _extents.descent <= scrolly) {
    if(section(i)->must_resize) // || i == sel_sect)
      resize_section(&context, i);
      
    DocumentImpl(*this).after_resize_section(i);
    
    ++i;
  }
  
  int first_visible_section = i - 1;
  if(first_visible_section < 0)
    first_visible_section = 0;
    
  while(i < length() && _extents.descent <= scrolly + page_height) {
    if(section(i)->must_resize) // || i == sel_sect)
      resize_section(&context, i);
      
    DocumentImpl(*this).after_resize_section(i);
    
    ++i;
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
    
    DocumentImpl(*this).after_resize_section(i);
    
    ++i;
  }
  
  if(!resize_only) {
    DocumentImpl(*this).add_selection_highlights(0, length());
//    DocumentImpl(*this).add_selection_highlights(first_visible_section, last_visible_section);

    {
      float y = 0;
      if(first_visible_section < length())
        y += section(first_visible_section)->y_offset;
      canvas->move_to(0, y);
    }
    
    for(i = first_visible_section; i <= last_visible_section; ++i) {
      paint_section(&context, i);
    }
    
    context.pre_paint_hooks.clear();
    context.post_paint_hooks.clear();
    
    DocumentImpl(*this).paint_cursor_and_flash();
    
    if(drag_source != context.selection && drag_status == DragStatusCurrentlyDragging) {
      if(Box *drag_src = drag_source.get()) {
        ::selection_path(canvas, drag_src, drag_source.start, drag_source.end);
        context.draw_selection_path();
      }
    }
    
    if(DebugFollowMouse) {
      if(Box *b = mouse_move_sel.get()) {
        ::selection_path(canvas, b, mouse_move_sel.start, mouse_move_sel.end);
        if(DocumentImpl::is_inside_string(b, mouse_move_sel.start))
          canvas->set_color(0x8000ff);
        else
          canvas->set_color(0xff0000);
        canvas->hair_stroke();
      }
    }
    
    if(DebugSelectionBounds) {
      if(Box *b = selection_box()) {
        canvas->save();
        {
          ::selection_path(canvas, b, selection_start(), selection_end());
          
          static const double dashes[] = {1.0, 2.0};
          
          double x1, y1, x2, y2;
          canvas->path_extents(&x1, &y1, &x2, &y2);
          canvas->new_path();
          
          if(canvas->pixel_device) {
            canvas->user_to_device(&x1, &y1);
            canvas->user_to_device(&x2, &y2);
            
            x2 = floor(x2 + 0.5) - 0.5;
            y2 = floor(y2 + 0.5) - 0.5;
            x1 = ceil(x1 - 0.5) + 0.5;
            y1 = ceil(y1 - 0.5) + 0.5;
            
            canvas->device_to_user(&x1, &y1);
            canvas->device_to_user(&x2, &y2);
          }
          
          canvas->move_to(x1, scrolly);
          canvas->line_to(x1, scrolly + page_height);
          
          canvas->move_to(x2, scrolly);
          canvas->line_to(x2, scrolly + page_height);
          
          canvas->move_to(_scrollx,               y1);
          canvas->line_to(_scrollx + _page_width, y1);
          
          canvas->move_to(_scrollx,               y2);
          canvas->line_to(_scrollx + _page_width, y2);
          canvas->close_path();
          
          canvas->set_color(0x808080);
          cairo_set_dash(canvas->cairo(), dashes, sizeof(dashes) / sizeof(double), 0.5);
          canvas->hair_stroke();
        }
        canvas->restore();
      }
    }
    
    if(auto_scroll) {
      auto_scroll = false;
      if(Box *box = sel_last.get())
        box->scroll_to(canvas, box, sel_last.start, sel_last.end);
    }
    
    if(selection_length() == 1 && best_index_rel_x == 0) {
      if(auto seq = dynamic_cast<MathSequence *>(selection_box())) {
        best_index_rel_x = seq->glyph_array()[selection_end() - 1].right;
        if(selection_start() > 0)
          best_index_rel_x -= seq->glyph_array()[selection_start() - 1].right;
          
        best_index_rel_x /= 2;
      }
    }
    
    canvas->translate(_scrollx, scrolly);
    
    if(last_paint_sel != context.selection) {
      last_paint_sel = context.selection;
      
      for(auto sel : additional_selection) {
        if(Box *b = sel.get())
          b->request_repaint_range(sel.start, sel.end);
      }
      additional_selection.length(0);
    }
  }
  
  context.canvas = 0;
  must_resize_min = 0;
}

Expr Document::to_pmath(BoxOutputFlags flags) {
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

//} ... class Document

//{ class DocumentImpl ...

inline DocumentImpl::DocumentImpl(Document &_self)
  : self(_self)
{
}

void DocumentImpl::raw_select(Box *box, int start, int end) {
  if(end < start) {
    int i = start;
    start = end;
    end = i;
  }
  
  if(box)
    box = box->normalize_selection(&start, &end);
    
  Box *selbox = self.context.selection.get();
  if( selbox != box ||
      self.context.selection.start != start ||
      self.context.selection.end   != end)
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
    
    if(self.selection_box())
      self.selection_box()->request_repaint_range(
        self.selection_start(),
        self.selection_end());
        
    for(auto sel : self.additional_selection) {
      if(Box *b = sel.get())
        b->request_repaint_range(sel.start, sel.end);
    }
    self.additional_selection.length(0);
    
    if(self.auto_completion.range.id) {
      Box *ac_box = self.auto_completion.range.get();
      
      if( !box ||
          !ac_box ||
          box_order(box, start, ac_box, self.auto_completion.range.start) < 0 ||
          box_order(box, end,   ac_box, self.auto_completion.range.end) > 0)
      {
        self.auto_completion.stop();
      }
    }
    
    self.context.selection.set_raw(box, start, end);
    
    if(box) {
      box->request_repaint_range(start, end);
    }
    
    if(box == &self)
      pmath_debug_print("[select document %d .. %d]\n", start, end);
  }
  
  self.best_index_rel_x = 0;
}

void DocumentImpl::after_resize_section(int i) {
  Section *sect = self.section(i);
  sect->y_offset = self._extents.descent;
  if(sect->visible) {
    self._extents.descent += sect->extents().descent;
    
    float w  = sect->extents().width;
    float uw = sect->unfilled_width;
    if(self.get_own_style(ShowSectionBracket, true)) {
      w += self.section_bracket_right_margin +
           self.section_bracket_width * self.group_info(i).nesting;
      uw += self.section_bracket_right_margin +
            self.section_bracket_width * self.group_info(i).nesting;
    }
    
    if(self._extents.width < w)
      self._extents.width = w;
      
    if(self.unfilled_width < uw)
      self.unfilled_width = uw;
  }
}

//{ selection highlights
void DocumentImpl::add_fill(PaintHookManager &hooks, Box *box, int start, int end, int color, float alpha) {
  SelectionReference ref;
  ref.set(box, start, end);
  
  hooks.add(ref.get(), new SelectionFillHook(ref.start, ref.end, color, alpha));
  self.additional_selection.add(ref);
}

void DocumentImpl::add_pre_fill(Box *box, int start, int end, int color, float alpha) {
  add_fill(self.context.pre_paint_hooks, box, start, end, color, alpha);
}

void DocumentImpl::add_selected_word_highlight_hooks(int first_visible_section, int last_visible_section) {
  self._current_word_references.length(0);
  
  if(self.selection_length() == 0)
    return;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(self.selection_box());
  int start = self.selection_start();
  int end   = self.selection_end();
  int len   = self.selection_length();
  
  if( seq &&
      !seq->is_placeholder(start) &&
      (start == 0 || seq->span_array().is_token_end(start - 1)) &&
      seq->span_array().is_token_end(end - 1))
  {
    if(!selection_is_name(&self))
      return;
      
    String str = seq->text().part(start, len);
    if(str.length() == 0)
      return;
      
    Box *find = &self;
    int index = first_visible_section;
    
    PaintHookManager temp_hooks;
    int num_occurencies = 0;
    int old_additional_selection_length = self.additional_selection.length();
    
    while(0 != (find = search_string(
                         find,
                         &index,
                         &self,
                         last_visible_section + 1,
                         str,
                         true)))
    {
      int s = index - len;
      int e = index;
      Box *box = find->get_highlight_child(find, &s, &e);
      
      if(box == find) {
        self._current_word_references.add(SelectionReference(box->id(), s, e));
        add_fill(temp_hooks, box, s, e, 0xFF9933);
        ++num_occurencies;
      }
    }
    
    bool do_fill = false;
    
    if(num_occurencies == 1) {
      int sel_sect = -1;
      Box *box = self.context.selection.get();
      while(box && box != &self) {
        sel_sect = box->index();
        box = box->parent();
      }
      
      if(sel_sect >= first_visible_section && sel_sect <= last_visible_section) {
        // The one found occurency is the selection. Search for more
        // occurencies outside the visible range.
        do_fill = word_occurs_outside_visible_range(str, first_visible_section, last_visible_section);
      }
      else
        do_fill = true;
    }
    else
      do_fill = (num_occurencies > 1);
      
    if(do_fill)
      temp_hooks.move_into(self.context.pre_paint_hooks);
    else
      self.additional_selection.length(old_additional_selection_length);
      
  }
}

bool DocumentImpl::word_occurs_outside_visible_range(String str, int first_visible_section, int last_visible_section) {
  Box *find = &self;
  int index = 0;
  
  while(nullptr != (find = search_string(
                             find,
                             &index,
                             &self,
                             first_visible_section,
                             str,
                             true)))
  {
    return true;
  }
  
  find = &self;
  index = last_visible_section + 1;
  while(nullptr != (find = search_string(
                             find,
                             &index,
                             &self,
                             self.length(),
                             str,
                             true)))
  {
    return true;
  }
  
  return false;
}

void DocumentImpl::add_matching_bracket_hook() {
  int start = self.selection_start();
  int end   = self.selection_end();
  auto seq = dynamic_cast<MathSequence *>(self.selection_box());
  if(seq) {
    SpanExpr *span = SpanExpr::find(seq, start, true);
    while(span && span->end() + 1 < end)
      span = span->expand(true);
      
    for(; span; span = span->expand(true)) {
      if(FunctionCallSpan::is_simple_call(span)) {
        {
          FunctionCallSpan call(span);
          
          SpanExpr *head = call.function_head();
          if( box_order(head->sequence(), head->start(),   seq, end)   <= 0 &&
              box_order(head->sequence(), head->end() + 1, seq, start) >= 0)
          {
            continue;
          }
          seq = span->sequence();
          
          // head without white space
          while(head->count() == 1)
            head = head->item(0);
            
          add_pre_fill(seq, head->start(), head->end() + 1, 0xFFFF00, 0.5);
          
          // opening parenthesis, always exists
          add_pre_fill(seq, span->item_pos(1), span->item_pos(1) + 1, 0xFFFF00, 0.5);
          
          // closing parenthesis, last item, might not exist
          int clos = span->count() - 1;
          if(clos >= 2 && span->item_equals(clos, ")")) {
            add_pre_fill(seq, span->item_pos(clos), span->item_pos(clos) + 1, 0xFFFF00, 0.5);
          }
          
        } // destroy call before deleting span
        delete span;
        return;
      }
      
      if(FunctionCallSpan::is_complex_call(span)) {
        {
          FunctionCallSpan call(span);
          
          SpanExpr *head = span->item(2);
          if( box_order(head->sequence(), head->start(),   seq, end)   <= 0 &&
              box_order(head->sequence(), head->end() + 1, seq, start) >= 0)
          {
            continue;
          }
          seq = span->sequence();
          
          // head, always exists
          add_pre_fill(seq, head->start(), head->end() + 1, 0xFFFF00, 0.5);
          
          // dot, always exists
          add_pre_fill(seq, span->item_pos(1), span->item_pos(1) + 1, 0xFFFF00, 0.5);
          
          // opening parenthesis, might not exist
          if(span->count() > 3) {
            add_pre_fill(seq, span->item_pos(3), span->item_pos(3) + 1, 0xFFFF00, 0.5);
          }
          
          // closing parenthesis, last item, might not exist
          int clos = span->count() - 1;
          if(clos >= 2 && span->item_equals(clos, ")")) {
            add_pre_fill(seq, span->item_pos(clos), span->item_pos(clos) + 1, 0xFFFF00, 0.5);
          }
        } // destroy call before deleting span
        delete span;
        return;
      }
    }
    
    delete span;
  }
  
  if(seq && end - start <= 1) {
    int pos = start;
    int other_bracket = seq->matching_fence(pos);
    
    if(other_bracket < 0 && start == end && pos > 0) {
      pos -= 1;
      other_bracket = seq->matching_fence(pos);
    }
    
    if(other_bracket >= 0) {
      add_pre_fill(seq, pos,           pos + 1,           0xFFFF00, 0.5);
      add_pre_fill(seq, other_bracket, other_bracket + 1, 0xFFFF00, 0.5);
    }
  }
  
  return;
}

void DocumentImpl::add_autocompletion_hook() {
  Box *box = self.auto_completion.range.get();
  
  if(!box)
    return;
    
  add_pre_fill(
    box,
    self.auto_completion.range.start,
    self.auto_completion.range.end,
    0x80FF80,
    0.5);
}

void DocumentImpl::add_selection_highlights(int first_visible_section, int last_visible_section) {
  add_matching_bracket_hook();
  add_selected_word_highlight_hooks(first_visible_section, last_visible_section);
  add_autocompletion_hook();
}
//}

//{ cursor painting
void DocumentImpl::paint_document_cursor() {
  // paint cursor (as a horizontal line) at end of document:
  if( self.context.selection.id    == self.id() &&
      self.context.selection.start == self.context.selection.end)
  {
    float y;
    if(self.context.selection.start < self.length())
      y = self.section(self.context.selection.start)->y_offset;
    else
      y = self._extents.descent;
      
    float x1 = 0;
    float y1 = y + 0.5;
    float x2 = self._extents.width;
    float y2 = y + 0.5;
    
    self.context.canvas->align_point(&x1, &y1, true);
    self.context.canvas->align_point(&x2, &y2, true);
    self.context.canvas->move_to(x1, y1);
    self.context.canvas->line_to(x2, y2);
    
    self.context.canvas->set_color(0xC0C0C0);
    self.context.canvas->hair_stroke();
    
    if(self.context.selection.start < self.count()) {
      x1 = self.section(self.context.selection.start)->get_style(SectionMarginLeft);
    }
    else if(self.count() > 0) {
      x1 = self.section(self.context.selection.start - 1)->get_style(SectionMarginLeft);
    }
    else
      x1 = 20 * 0.75;
      
    x1 += self._scrollx;
    x2 = x1 + 40 * 0.75;
    
    self.context.canvas->align_point(&x1, &y1, true);
    self.context.canvas->align_point(&x2, &y2, true);
    self.context.canvas->move_to(x1, y1);
    self.context.canvas->line_to(x2, y2);
    
    self.context.draw_selection_path();
  }
}

void DocumentImpl::paint_flashing_cursor_if_needed() {
  if(self.prev_sel_line >= 0) {
    AbstractSequence *seq = dynamic_cast<AbstractSequence *>(self.selection_box());
    
    if(seq && seq->id() == self.prev_sel_box_id) {
      int line = seq->get_line(self.selection_end(), self.prev_sel_line);
      
      if(line != self.prev_sel_line) {
        self.flashing_cursor_circle = new BoxRepaintEvent(self.id()/*self.prev_sel_box_id*/, 0.0);
      }
    }
    
    self.prev_sel_line = -1;
    self.prev_sel_box_id = 0;
  }
  
  if(self.flashing_cursor_circle) {
    double t = self.flashing_cursor_circle->timer();
    Box *box = self.selection_box();
    
    if( !box ||
        t >= MaxFlashingCursorTime ||
        !self.flashing_cursor_circle->register_event())
    {
      self.flashing_cursor_circle = nullptr;
    }
    
    t = t / MaxFlashingCursorTime;
    
    if(self.flashing_cursor_circle) {
      double r = MaxFlashingCursorRadius * (1 - t);
      float x1 = self.context.last_cursor_x[0];
      float y1 = self.context.last_cursor_y[0];
      float x2 = self.context.last_cursor_x[1];
      float y2 = self.context.last_cursor_y[1];
      
      r = MaxFlashingCursorRadius * (1 - t);
      
      self.context.canvas->save();
      {
        self.context.canvas->user_to_device(&x1, &y1);
        self.context.canvas->user_to_device(&x2, &y2);
        
        cairo_matrix_t mat;
        cairo_matrix_init_identity(&mat);
        cairo_set_matrix(self.context.canvas->cairo(), &mat);
        
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
        self.context.canvas->transform(mat);
        self.context.canvas->translate(x, y);
        self.context.canvas->scale(r + h / 2, r);
        
        self.context.canvas->arc(0, 0, 1, 0, 2 * M_PI, false);
        
        cairo_set_operator(self.context.canvas->cairo(), CAIRO_OPERATOR_DIFFERENCE);
        self.context.canvas->set_color(0xffffff);
        self.context.canvas->fill();
      }
      self.context.canvas->restore();
    }
  }
}

void DocumentImpl::paint_cursor_and_flash() {
  paint_document_cursor();
  paint_flashing_cursor_if_needed();
}
//}

//{ insertion
inline bool DocumentImpl::is_inside_string() {
  return is_inside_string(self.context.selection.get(), self.context.selection.start);
}

bool DocumentImpl::is_inside_string(Box *box, int index) {
  while(box) {
    if(auto seq = dynamic_cast<MathSequence *>(box)) {
      if(seq->is_inside_string((index)))
        return true;
        
    }
    index = box->index();
    box = box->parent();
  }
  
  return false;
}

bool DocumentImpl::is_inside_alias() {
  bool result = false;
  Box *box = self.context.selection.get();
  int index = self.context.selection.start;
  while(box) {
    if(auto seq = dynamic_cast<MathSequence *>(box)) {
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

// substart and subend may lie outside 0..subbox->length()
bool DocumentImpl::is_inside_selection(Box *subbox, int substart, int subend) {
  if(self.selection_box() && self.selection_length() > 0) {
    // section selections are only at the right margin, the section content is
    // not inside the selection-frame
    if(self.selection_box() == &self && subbox != &self)
      return false;
      
    if(substart == subend)
      return false;
      
    Box *b = subbox;
    while(b && b != self.selection_box()) {
      substart = b->index();
      subend   = substart + 1;
      b = b->parent();
    }
    
    if( b == self.selection_box() &&
        self.selection_start() <= substart &&
        subend <= self.selection_end())
    {
      return true;
    }
  }
  
  return false;
}

bool DocumentImpl::is_inside_selection(Box *subbox, int substart, int subend, bool was_inside_start) {
  if(subbox && subbox != &self && substart == subend) {
    if(was_inside_start)
      subend = substart + 1;
    else
      --substart;
  }
  
  return self.selection_box() && is_inside_selection(subbox, substart, subend);
}

void DocumentImpl::set_prev_sel_line() {
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(self.selection_box())) {
    self.prev_sel_line = seq->get_line(self.selection_end(), self.prev_sel_line);
    self.prev_sel_box_id = seq->id();
  }
  else {
    self.prev_sel_line = -1;
    self.prev_sel_box_id = -1;
  }
}

bool DocumentImpl::prepare_insert() {
  if(self.context.selection.id == self.id()) {
    self.prev_sel_line = -1;
    if( self.context.selection.start != self.context.selection.end ||
        !self.get_style(Editable, true))
    {
      return false;
    }
    
    Expr style_expr = self.get_group_style(
                        self.context.selection.start - 1,
                        DefaultNewSectionStyle,
                        Symbol(PMATH_SYMBOL_FAILED));
                        
    SharedPtr<Style> section_style = new Style(style_expr);
    
    String lang;
    if(!section_style->get(LanguageCategory, &lang)) {
      if(auto all = self.stylesheet())
        all->get(section_style, LanguageCategory, &lang);
    }
    
    Section *sect;
    if(lang.equals("NaturalLanguage"))
      sect = new TextSection(section_style);
    else
      sect = new MathSection(section_style);
      
    self.insert(self.context.selection.start, sect);
    self.move_horizontal(LogicalDirection::Forward, false);
    
    return true;
  }
  else {
    if(self.selection_box() && self.selection_box()->edit_selection(&self.context)) {
      self.native()->on_editing();
      set_prev_sel_line();
      return true;
    }
  }
  
  return false;
}

bool DocumentImpl::prepare_insert_math(bool include_previous_word) {
  if(!prepare_insert())
    return false;
    
  if(dynamic_cast<MathSequence *>(self.selection_box()))
    return true;
    
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(self.selection_box());
  if(!seq)
    return false;
    
  if(include_previous_word && self.selection_length() == 0) {
    if(auto txt = dynamic_cast<TextSequence *>(seq)) {
      const char *buf = txt->text_buffer().buffer();
      int i = self.selection_start();
      
      while(i > 0 && (unsigned char)buf[i] > ' ')
        --i;
        
      self.select(txt, i, self.selection_end());
    }
  }
  
  InlineSequenceBox *box = new InlineSequenceBox;
  
  int s = self.selection_start();
  int e = self.selection_end();
  box->content()->insert(0, seq, s, e);
  seq->remove(s, e);
  seq->insert(s, box);
  
  self.select(box->content(), 0, box->content()->length());
  return true;
}

Section *DocumentImpl::auto_make_text_or_math(Section *sect) {
  assert(sect != nullptr);
  assert(sect->parent() == &self);
  
  if(!dynamic_cast<AbstractSequenceSection*>(sect))
    return sect;
    
  String langcat = sect->get_style(LanguageCategory);
  if(langcat.equals("NaturalLanguage"))
    return convert_content<MathSection, TextSection>(sect);
  else
    return convert_content<TextSection, MathSection>(sect);
}

template<class FromSectionType, class ToSectionType>
Section *DocumentImpl::convert_content(Section *sect) {
  assert(sect != nullptr);
  assert(sect->parent() == &self);
  
  auto old_sect = dynamic_cast<FromSectionType*>(sect);
  if(!old_sect)
    return sect;
  
  bool contains_selection = old_sect->is_parent_of(self.selection_box());
    
  auto old_content = old_sect->content();
  auto new_sect = new ToSectionType(old_sect->style);
  new_sect->swap_id(old_sect);
  new_sect->content()->insert(0, old_content, 0, old_content->length());
  self.swap(sect->index(), new_sect)->safe_destroy();
  
  if(contains_selection && self.selection_box() == nullptr) {
    AbstractSequence *seq = new_sect->content();
    self.select(seq, seq->length(), seq->length());
  }
  
  return new_sect;
}
//}

//{ key events
void DocumentImpl::handle_key_left_right(SpecialKeyEvent &event, LogicalDirection direction) {
  int sel_forward_start;
  int sel_forward_end;
  
  if(direction == LogicalDirection::Forward) {
    sel_forward_start = self.context.selection.start;
    sel_forward_end   = self.context.selection.end;
  }
  else {
    sel_forward_start = self.context.selection.end;
    sel_forward_end   = self.context.selection.start;
  }
  
  if(event.shift) {
    self.move_horizontal(direction, event.ctrl, true);
  }
  else if(self.selection_length() > 0) {
    Box *selbox = self.context.selection.get();
    
    if(selbox == &self) {
      self.move_to(&self, sel_forward_start);
      self.move_horizontal(direction, event.ctrl);
    }
    else if( dynamic_cast<GridBox *>(selbox) ||
             (selbox &&
              dynamic_cast<GridItem *>(selbox->parent()) &&
              ((MathSequence *)selbox)->is_placeholder()))
    {
      self.move_horizontal(direction, event.ctrl);
    }
    else
      self.move_to(selbox, sel_forward_end);
  }
  else
    self.move_horizontal(direction, event.ctrl);
    
  event.key = SpecialKey::Unknown;
  self.auto_completion.stop();
}

void DocumentImpl::handle_key_home_end(SpecialKeyEvent &event, LogicalDirection direction) {
  self.move_start_end(direction, event.shift);
  event.key = SpecialKey::Unknown;
  self.auto_completion.stop();
}

void DocumentImpl::handle_key_up_down(SpecialKeyEvent &event, LogicalDirection direction) {
  self.move_vertical(direction, event.shift);
  event.key = SpecialKey::Unknown;
  self.auto_completion.stop();
}

void DocumentImpl::handle_key_pageup_pagedown(SpecialKeyEvent &event, LogicalDirection direction) {
  if(!self.native()->is_scrollable())
    return;
    
  float w, h;
  self.native()->window_size(&w, &h);
  if(direction == LogicalDirection::Backward)
    h = -h;
    
  self.native()->scroll_by(0, h);
  
  event.key = SpecialKey::Unknown;
}

void DocumentImpl::handle_key_tab(SpecialKeyEvent &event) {
  if(is_tabkey_only_moving()) {
    SelectionReference oldpos = self.context.selection;
    
    if(!event.ctrl) {
      if(self.auto_completion.next(event.shift ? LogicalDirection::Backward : LogicalDirection::Forward)) {
        event.key = SpecialKey::Unknown;
        return;
      }
    }
    
    self.move_tab(event.shift ? LogicalDirection::Backward : LogicalDirection::Forward);
    
    if(oldpos == self.context.selection) {
      self.native()->beep();
    }
  }
  else
    self.key_press('\t');
    
  event.key = SpecialKey::Unknown;
}

bool DocumentImpl::is_tabkey_only_moving() {
  Box *selbox = self.context.selection.get();
  
  if(self.context.selection.start != self.context.selection.end)
    return true;
    
  if(!selbox || selbox == &self)
    return false;
    
  if(!dynamic_cast<Section *>(selbox->parent()))
    return true;
    
  if(auto seq = dynamic_cast<MathSequence *>(selbox)) {
    const uint16_t *buf = seq->text().buffer();
    
    for(int i = self.context.selection.start - 1; i >= 0; --i) {
      if(buf[i] == '\n')
        return false;
        
      if(buf[i] != '\t' && buf[i] != ' ')
        return true;
    }
    
    return false;
  }
  
  if(auto seq = dynamic_cast<TextSequence *>(selbox)) {
    const char *buf = seq->text_buffer().buffer();
    
    for(int i = self.context.selection.start - 1; i >= 0; --i) {
      if(buf[i] == '\n')
        return false;
        
      if(buf[i] != '\t' && buf[i] != ' ')
        return true;
    }
    
    return false;
  }
  
  return true;
}

void DocumentImpl::handle_key_backspace(SpecialKeyEvent &event) {
  set_prev_sel_line();
  if(self.selection_length() > 0) {
    if(self.remove_selection(true))
      event.key = SpecialKey::Unknown;
    return;
  }
  
  Box *selbox = self.context.selection.get();
  if( self.context.selection.start == 0 &&
      selbox &&
      selbox->get_style(Editable) &&
      selbox->parent() &&
      selbox->parent()->exitable())
  {
    int index = selbox->index();
    selbox = selbox->parent()->remove(&index);
    
    self.move_to(selbox, index);
    self.auto_completion.stop();
    event.key = SpecialKey::Unknown;
    return;
  }
  
  if(event.ctrl || selbox != &self) {
    int old = self.context.selection.id;
    MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
    
    if( seq &&
        seq->text()[self.context.selection.start - 1] == PMATH_CHAR_BOX)
    {
      int boxi = 0;
      while(seq->item(boxi)->index() < self.context.selection.start - 1)
        ++boxi;
        
      if(seq->item(boxi)->length() == 0) {
        self.move_horizontal(LogicalDirection::Backward, event.ctrl, true);
        event.key = SpecialKey::Unknown;
        return;
      }
      
      int old_sel_end = self.selection_end();
      self.move_horizontal(
        LogicalDirection::Backward,
        event.ctrl,
        event.ctrl || event.shift);
        
      if( self.selection_length() == 0 &&
          old == self.context.selection.id)
      {
        self.select(seq, self.selection_start(), old_sel_end);
        event.key = SpecialKey::Unknown;
        return;
      }
    }
    else
      self.move_horizontal(LogicalDirection::Backward, event.ctrl, true);
      
    if(!event.ctrl &&
        seq &&
        seq->text()[self.context.selection.start] == PMATH_CHAR_BOX)
    {
      event.key = SpecialKey::Unknown;
      return;
    }
    
    if(old == self.context.selection.id) {
      self.remove_selection(true);
      
      // reset sel_last:
      selbox = self.context.selection.get();
      self.select(selbox, self.context.selection.start, self.context.selection.end);
    }
  }
  else
    self. move_horizontal(LogicalDirection::Backward, event.ctrl);
    
  event.key = SpecialKey::Unknown;
}

void DocumentImpl::handle_key_delete(SpecialKeyEvent &event) {
  set_prev_sel_line();
  if(self.selection_length() > 0) {
    if(self.remove_selection(true))
      event.key = SpecialKey::Unknown;
    return;
  }
  
  Box *selbox = self.context.selection.get();
  if(event.ctrl || selbox != &self) {
    int index = self.context.selection.start;
    Box *box = selbox->move_logical(LogicalDirection::Forward, event.ctrl, &index);
    
    while(box && !box->selectable()) {
      box = box->move_logical(LogicalDirection::Forward, true, &index);
    }
    
    if(box == selbox || box == selbox->parent()) {
      self.move_to(box, index, true);
      selbox = self.selection_box();
      
      if(!event.ctrl) {
        MathSequence *seq = dynamic_cast<MathSequence *>(selbox);
        
        if(seq && seq->text()[self.context.selection.start] == PMATH_CHAR_BOX) {
          event.key = SpecialKey::Unknown;
          return;
        }
      }
      
      self.remove_selection(true);
    }
    else {
      Box *p = box;
      while(p && p != selbox)
        p = p->parent();
        
      if(p) {
        MathSequence *seq = dynamic_cast<MathSequence *>(box);
        
        if(seq && seq->is_placeholder(index)) {
          self.select(seq, index, index + 1);
        }
        else {
          int  old_start = self.selection_start();
          Box *old_box   = self.selection_box();
          
          self.move_to(box, index);
          
          if( self.selection_start() == old_start &&
              self.selection_box()   == old_box)
          {
            self.select(old_box, old_start, old_start + 1);
          }
        }
      }
      else
        self.native()->beep();
    }
  }
  else
    self.move_horizontal(LogicalDirection::Forward, event.ctrl);
    
  event.key = SpecialKey::Unknown;
}

void DocumentImpl::handle_key_escape(SpecialKeyEvent &event) {
  if(self.context.clicked_box_id) {
    if(auto receiver = FrontEndObject::find_cast<Box>(self.context.clicked_box_id))
      receiver->on_mouse_cancel();
      
    self.context.clicked_box_id = 0;
    event.key = SpecialKey::Unknown;
    return;
  }
  
  self.key_press(PMATH_CHAR_ALIASDELIMITER);
  event.key = SpecialKey::Unknown;
}
//}

//{ macro handling
inline bool DocumentImpl::handle_immediate_macros() {
  return handle_immediate_macros(global_immediate_macros);
}

bool DocumentImpl::handle_immediate_macros(
  const Hashtable<String, Expr, object_hash> &table
) {
  if(self.selection_length() != 0)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(self.selection_box());
  if(seq && self.selection_start() > 0) {
    int i = self.selection_start() - 2;
    while(i >= 0 && !seq->span_array().is_token_end(i))
      --i;
    ++i;
    
    int e = self.selection_start();
    
    Expr repl = table[seq->text().part(i, e - i)];
    
    if(!repl.is_null()) {
      String s(repl);
      
      if(s.is_null()) {
        MathSequence *repl_seq = new MathSequence();
        repl_seq->load_from_object(repl, BoxInputFlags::Default);
        
        seq->remove(i, e);
        self.move_to(self.selection_box(), i);
        self.insert_box(repl_seq, true);
        return true;
      }
      else {
        int repl_index = index_of_replacement(s);
        if(repl_index >= 0) {
          int new_sel_start = seq->insert(e, s.part(0, repl_index));
          int new_sel_end   = seq->insert(new_sel_start, PMATH_CHAR_PLACEHOLDER);
          seq->insert(new_sel_end, s.part(repl_index + 1));
          seq->remove(i, e);
          
          self.select(seq, new_sel_start - (e - i), new_sel_end - (e - i));
        }
        else {
          seq->insert(e, s);
          seq->remove(i, e);
          self.move_to(self.selection_box(), i + s.length());
        }
        return true;
      }
    }
  }
  
  return false;
}

inline bool DocumentImpl::handle_macros() {
  return handle_macros(global_macros);
}

bool DocumentImpl::handle_macros(
  const Hashtable<String, Expr, object_hash> &table
) {
  if(self.selection_length() != 0)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(self.selection_box());
  if(seq && self.selection_start() > 0) {
    const uint16_t *buf = seq->text().buffer();
    
    int e = self.selection_start();
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
          int repl_index = index_of_replacement(s);
          if(repl_index >= 0) {
            int new_sel_start = seq->insert(e, s.part(0, repl_index));
            int new_sel_end   = seq->insert(new_sel_start, PMATH_CHAR_PLACEHOLDER);
            seq->insert(new_sel_end, s.part(repl_index + 1));
            seq->remove(i, e);
            
            self.select(seq, new_sel_start - (e - i), new_sel_end - (e - i));
          }
          else {
            seq->insert(e, s);
            seq->remove(i, e);
            self.move_to(self.selection_box(), i + s.length());
          }
          return true;
        }
        else {
          MathSequence *repl_seq = new MathSequence();
          repl_seq->load_from_object(repl, BoxInputFlags::Default);
          
          seq->remove(i, e);
          self.move_to(self.selection_box(), i);
          self.insert_box(repl_seq, true);
          return true;
        }
      }
    }
  }
  
  return false;
}
//}

//} ... class DocumentImpl
