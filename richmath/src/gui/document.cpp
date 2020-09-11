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

#include <syntax/spanexpr.h>

#include <util/autovaluereset.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_DocumentObject;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;

bool richmath::DebugFollowMouse     = false;
bool richmath::DebugSelectionBounds = false;

static const Color DebugFollowMouseInStringColor = Color::from_rgb(0.5, 0, 1);
static const Color DebugFollowMouseColor         = Color::from_rgb(1, 0, 0);
static const Color DebugSelectionBoundsColor     = Color::from_rgb(0.5, 0.5, 0.5);

static const Color OccurenceBackgroundColor = Color::from_rgb24(0xFF9933);

static const Color  MatchingBracketHighlightColor = Color::from_rgb(1, 1, 0);
static const double MatchingBracketHighlightAlpha = 0.5;

static const Color  AutoCompleteHighlightColor = Color::from_rgb(1, 1, 0);
static const double AutoCompleteHighlightAlpha = 0.5;

static const Color DocumentCursorLineColor = Color::from_rgb24(0xC0C0C0);


static const double MaxFlashingCursorRadius = 9;  /* points */
static const double MaxFlashingCursorTime = 0.15; /* seconds */

namespace {
  static class MouseHistory {
    public:
      MouseHistory() 
      : down_time(0.0), // -HUGE_VAL
        down_pos(HUGE_VAL, HUGE_VAL),
        click_repeat_count(0),
        _document(nullptr),
        _num_buttons_pressed(0)
      {
      }
      
      int press_button(Document *document) {
        if(_document == document) {
          ++_num_buttons_pressed;
        }
        else {
          _document = document;
          _num_buttons_pressed = 1;
        }
        return _num_buttons_pressed;
      }
      
      int release_button() {
        if(--_num_buttons_pressed <= 0)
          _num_buttons_pressed = 0;
        
        return _num_buttons_pressed;
      }
      
      int is_mouse_down(Document *document) {
        return _document == document && _num_buttons_pressed > 0;
      }
      
      void reset() {
        _document = nullptr;
        _num_buttons_pressed = 0;
        click_repeat_count = 0;
      }
    
    public:
      double             down_time; // left button only
      Point              down_pos;
      SelectionReference down_sel;
      SelectionReference debug_move_sel;
      int                click_repeat_count;
      
      ObservableValue<FrontEndReference> observable_mouseover_box_id;
      
    private:
      Document *_document;
      int _num_buttons_pressed;
  } mouse_history;
}

Hashtable<String, Expr> richmath::global_immediate_macros;
Hashtable<String, Expr> richmath::global_macros;

static int index_of_replacement(const String &s) {
  const uint16_t *buf = s.buffer();
  int             len = s.length();
  
  if(len <= 1)
    return -1;
    
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_SELECTIONPLACEHOLDER)
      return i;
      
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_PLACEHOLDER)
      return i;
      
  return -1;
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
  class Document::Impl {
    private:
      Document &self;
      
    public:
      Impl(Document &self) : self(self) {}
      
    public:
      void raw_select(Box *box, int start, int end);
      void after_resize_section(int i);
      
      //{ selection highlights
    private:
      void add_fill(PaintHookManager &hooks, Box *box, int start, int end, Color color, float alpha = 1.0f);
      void add_pre_fill(Box *box, int start, int end, Color color, float alpha = 1.0f);
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
      bool is_inside_selection(const VolatileSelection &sub);
      bool is_inside_selection(const VolatileSelection &sub, bool was_inside_start);
      
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
      bool handle_immediate_macros(const Hashtable<String, Expr> &table);
      bool handle_macros(const Hashtable<String, Expr> &table);
      
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
    prev_sel_box_id(FrontEndReference::None),
    must_resize_min(0),
    drag_status(DragStatus::Idle),
    auto_scroll(false),
    _native(NativeWidget::dummy),
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
    
  Expr options_expr { pmath_options_extract_ex(expr.get(), 1, PMATH_OPTIONS_EXTRACT_UNKNOWN_WARNONLY) };
  if(!options_expr.is_valid())
    return false;
    
  reset_style();
  style->add_pmath(std::move(options_expr));
  load_stylesheet();
  
  sections_expr = Call(Symbol(richmath_System_SectionGroup), std::move(sections_expr), Symbol(PMATH_SYMBOL_ALL));
  
  int pos = 0;
  insert_pmath(&pos, std::move(sections_expr), count());
  
  finish_load_from_object(std::move(expr));
  return true;
}

bool Document::request_repaint(float x, float y, float w, float h) {
  RectangleF window_rect { native()->scroll_pos(), native()->window_size() };
  
  if(window_rect.overlaps({x,y,w,h})) {
    native()->invalidate_rect({x,y,w,h});
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

void Document::scroll_to(const RectangleF &rect) {
  if(!native()->is_scrollable())
    return;
    
  RectangleF window_rect{ native()->scroll_pos(), native()->window_size() };
  
  if(rect.top() < window_rect.top() && rect.top() < window_rect.top()) {
    window_rect.y = rect.y - window_rect.height / 6.f;
    if(rect.bottom() > window_rect.bottom())
      window_rect.y = rect.bottom() - window_rect.height;
  }
  else if(rect.bottom() >= window_rect.bottom() && rect.top() > window_rect.top()) {
    window_rect.y = rect.bottom() - window_rect.height * 5 / 6.f;
    if(rect.top() < window_rect.top())
      window_rect.y = rect.y;
  }
  
  if(rect.left() < window_rect.left() && rect.right() < window_rect.right()) {
    window_rect.x = rect.x - window_rect.width / 6.f;
    if(rect.right() > window_rect.right())
      window_rect.x = rect.right() - window_rect.width;
  }
  else if(rect.right() >= window_rect.right() && rect.left() > window_rect.left()) {
    window_rect.x = rect.right() - window_rect.width * 5 / 6.f;
    if(rect.left() < window_rect.left())
      window_rect.x = rect.x;
  }
  
  native()->scroll_to(window_rect.top_left());
}

void Document::scroll_to(Canvas &canvas, const VolatileSelection &child_sel) {
  default_scroll_to(canvas, this, child_sel);
}

//{ event invokers ...

void Document::mouse_exit() {
  if(context.mouseover_box_id) {
    Box *over = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
    
    while(over && over != this) {
      over->on_mouse_exit();
      over = over->parent();
    }
    
    context.mouseover_box_id = FrontEndReference::None;
    mouse_history.observable_mouseover_box_id = FrontEndReference::None;
  }
  
  if(DebugFollowMouse) {
    mouse_history.debug_move_sel.reset();
    invalidate();
  }
}

void Document::mouse_down(MouseEvent &event) {
  Box *receiver = nullptr;
  
  //Application::update_control_active(native()->is_mouse_down());
  
  Application::delay_dynamic_updates(true);
  if(mouse_history.press_button(this) == 1) {
    event.set_origin(this);
    
    bool was_inside_start;
    receiver = mouse_selection(event.x, event.y, &was_inside_start).box;
                 
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
  auto next_clicked_box_id = context.clicked_box_id;
  Box *receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id);
  
  if(mouse_history.release_button() <= 0) {
    Application::delay_dynamic_updates(false);
    next_clicked_box_id = FrontEndReference::None;
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
  if(Box *receiver = FrontEndObject::find_cast<Box>(context.clicked_box_id)) {
    native()->set_cursor(CursorType::Current);
    
    receiver->on_mouse_move(event);
  }
  else {
    event.set_origin(this);
    
    bool was_inside_start;
    VolatileSelection receiver_sel = mouse_selection(event.x, event.y, &was_inside_start);
    
    if(DebugFollowMouse && !mouse_history.debug_move_sel.equals(receiver_sel)) {
      mouse_history.debug_move_sel.set(receiver_sel);
      invalidate();
    }
    
    //Box *new_over = receiver_sel.box ? receiver_sel.box->mouse_sensitive() : nullptr;
    Box *old_over = FrontEndObject::find_cast<Box>(context.mouseover_box_id);
    
    if(receiver_sel.box) {
      Box *base = Box::common_parent(receiver_sel.box, old_over);
      Box *box = old_over;
      while(box != base) {
        box->on_mouse_exit();
        box = box->parent();
      }
      
      reverse_mouse_enter(base, receiver_sel.box);
      
      box = receiver_sel.box->mouse_sensitive();
      if(box)
        box->on_mouse_move(event);
        
      context.mouseover_box_id = receiver_sel.box->id();
    }
    else
      context.mouseover_box_id = FrontEndReference::None;
    
    mouse_history.observable_mouseover_box_id = context.mouseover_box_id;
  }
}

void Document::focus_set() {
  context.active = true;
  
  if(Box *box = selection_box()) {
    if(selection_length() > 0)
      box->request_repaint_range(selection_start(), selection_end());
    
    while(box) {
      box->on_enter();
      box = box->parent();
    }
  }
}

void Document::focus_killed() {
  context.active = false;
  reset_mouse();
  
  if(Box *box = selection_box()) {
    if(!box->selectable())
      select(nullptr, 0, 0);
    else if(selection_length() > 0)
      box->request_repaint_range(selection_start(), selection_end());
    
    while(box) {
      box->on_exit();
      box = box->parent();
    }
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
  
  drag_status = DragStatus::Idle;
  if(event.left) {
    Vector2F dd = native()->double_click_dist();
    
    bool double_click =
      abs(mouse_history.down_time - native()->message_time()) <= native()->double_click_time() &&
      fabs(event.x - mouse_history.down_pos.x) <= dd.x &&
      fabs(event.y - mouse_history.down_pos.y) <= dd.y;
      
    mouse_history.down_time = native()->message_time();
    
    bool was_inside_start;
    VolatileSelection mouse_sel = mouse_selection(event.x, event.y, &was_inside_start);
                 
    if(double_click) {
      ++mouse_history.click_repeat_count;
      
      VolatileSelection sel = context.selection.get_all();
      if(sel.box == this) {
        if(sel.start < sel.end) {
          toggle_open_close_group(sel.start);
          
          // prevent selection from changing in mouse_move():
          context.clicked_box_id = FrontEndReference::None;
          
          // prevent "tripple-click"
          mouse_history.down_time = 0;
        }
      }
      else if(sel.selectable()) {
        bool should_expand = true;
        
        if(sel.start == sel.end) {
          if(was_inside_start) {
            if(sel.end + 1 <= sel.box->length()) {
              ++sel.end;
              should_expand = false;
            }
          }
          else if(sel.start > 0) {
            --sel.start;
            should_expand = false;
          }
        }
        
        if(!should_expand) {
          should_expand = true;
          
          if(auto seq = dynamic_cast<MathSequence *>(sel.box)) {
            should_expand = false;
            while(sel.start > 0 && !seq->span_array().is_token_end(sel.start - 1))
              --sel.start;
              
            while(sel.end < seq->length() && !seq->span_array().is_token_end(sel.end - 1))
              ++sel.end;
          }
        }
        
        if(should_expand)
          sel.expand();
          
        select(sel);
      }
    }
    else {
      mouse_history.click_repeat_count = 1;
      
      if(Impl(*this).is_inside_selection(mouse_sel, was_inside_start)) {
        // maybe drag & drop
        drag_status = DragStatus::MayDrag;
      }
      else if(mouse_sel.selectable())
        select(mouse_sel);
    }
    
    mouse_history.down_pos = { event.x, event.y };
    mouse_history.down_sel = sel_first;
  }
}

void Document::on_mouse_move(MouseEvent &event) {
  event.set_origin(this);
  
  bool was_inside_start;
  VolatileSelection mouse_sel = mouse_selection(event.x, event.y, &was_inside_start);
  
  if(event.left && drag_status == DragStatus::MayDrag) {
    Vector2F dd = native()->double_click_dist();
    
    if( fabs(event.x - mouse_history.down_pos.x) > dd.x ||
        fabs(event.y - mouse_history.down_pos.y) > dd.y)
    {
      drag_status = DragStatus::CurrentlyDragging;
      mouse_history.down_pos = Point(HUGE_VAL, HUGE_VAL);
      native()->do_drag_drop(selection_now(), event);
    }
    
    return;
  }
  
  if(drag_status == DragStatus::CurrentlyDragging) {
    native()->set_cursor(CursorType::Current);
    return;
  }
  
  if(!mouse_sel)
    return;
  
  if(!event.left && Impl(*this).is_inside_selection(mouse_sel, was_inside_start)) {
    native()->set_cursor(CursorType::Default);
  }
  else if(mouse_sel.selectable()) {
    if(mouse_sel.box == this) {
      if(length() == 0)
        native()->set_cursor(CursorType::TextN);
      else if(mouse_sel.is_empty())
        native()->set_cursor(CursorType::Document);
      else
        native()->set_cursor(CursorType::Section);
    }
    else
      native()->set_cursor(NativeWidget::text_cursor(mouse_sel.box, mouse_sel.start));
  }
  else if(dynamic_cast<Section *>(mouse_sel.box) && selectable()) {
    native()->set_cursor(CursorType::NoSelect);
  }
  else
    native()->set_cursor(CursorType::Default);
    
  if(event.left && context.clicked_box_id) {
    if(VolatileSelection mouse_down_sel = mouse_history.down_sel.get_all()) {
      Section *sec1 = mouse_down_sel.box->find_parent<Section>(true);
      Section *sec2 = mouse_sel.box ? mouse_sel.box->find_parent<Section>(true) : nullptr;
      
      if(sec1 && sec1 != sec2) {
        event.set_origin(sec1);
        mouse_sel = sec1->mouse_selection(event.x, event.y, &was_inside_start);
      }
      
      VolatileSelection down_sel = mouse_down_sel;
      if(mouse_history.click_repeat_count >= 2) {
        bool in_text = Impl::is_inside_string(down_sel.box, down_sel.start);
        
        if(!in_text) {
          VolatileSelection new_down_sel = down_sel.expanded_up_to_sibling(mouse_sel);
          if(!new_down_sel.same_box_but_disjoint(down_sel))
            down_sel = new_down_sel;
        }
        
        VolatileSelection new_mouse_sel = mouse_sel.expanded_up_to_sibling(
                                            mouse_down_sel, 
                                            in_text ? mouse_history.click_repeat_count - 1 : INT_MAX);
        if(!new_mouse_sel.same_box_but_disjoint(mouse_sel))
          mouse_sel = new_mouse_sel;
      }
      
      select_range(down_sel, mouse_sel);
    }
  }
}

void Document::on_mouse_up(MouseEvent &event) {
  event.set_origin(this);
  
  if(event.left && drag_status != DragStatus::Idle) {
    bool was_inside_start;
    VolatileSelection mouse_sel = mouse_selection(event.x, event.y, &was_inside_start);
                 
    if(Impl(*this).is_inside_selection(mouse_sel, was_inside_start) && mouse_sel.selectable()) {
      select(mouse_sel);
    }
  }
  
  drag_status = DragStatus::Idle;
}

void Document::on_mouse_cancel() {
  drag_status = DragStatus::Idle;
}

void Document::on_key_down(SpecialKeyEvent &event) {
  switch(event.key) {
    case SpecialKey::Left:
      Impl(*this).handle_key_left_right(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::Right:
      Impl(*this).handle_key_left_right(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Home:
      Impl(*this).handle_key_home_end(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::End:
      Impl(*this).handle_key_home_end(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Up:
      Impl(*this).handle_key_up_down(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::Down:
      Impl(*this).handle_key_up_down(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::PageUp:
      Impl(*this).handle_key_pageup_pagedown(event, LogicalDirection::Backward);
      return;
      
    case SpecialKey::PageDown:
      Impl(*this).handle_key_pageup_pagedown(event, LogicalDirection::Forward);
      return;
      
    case SpecialKey::Tab:
      Impl(*this).handle_key_tab(event);
      return;
      
    case SpecialKey::Backspace:
      Impl(*this).handle_key_backspace(event);
      return;
      
    case SpecialKey::Delete:
      Impl(*this).handle_key_delete(event);
      return;
      
    case SpecialKey::Escape:
      Impl(*this).handle_key_escape(event);
      return;
      
    default: return;
  }
}

void Document::on_key_up(SpecialKeyEvent &event) {
}

void Document::on_key_press(uint32_t unichar) {
  AbstractSequence *initial_seq = dynamic_cast<AbstractSequence *>(selection_box());
  
  if(!Impl(*this).prepare_insert()) {
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
          
          Expr new_style_expr = sect->get_style(DefaultReturnCreatedSectionStyle, Symbol(PMATH_SYMBOL_AUTOMATIC));
          if(new_style_expr == PMATH_SYMBOL_AUTOMATIC) 
            new_style_expr = get_group_style(sect->index(), DefaultNewSectionStyle, Symbol(PMATH_SYMBOL_AUTOMATIC));
          if(new_style_expr == PMATH_SYMBOL_AUTOMATIC)
            new_style_expr = sect->get_own_style(BaseStyleName);

          new_style->add_pmath(std::move(new_style_expr));
            
          String lang;
          if(context.stylesheet)
            lang = context.stylesheet->get_with_base(new_style, LanguageCategory);
          else
            new_style->get(LanguageCategory, &lang);
            
          if(lang.equals("NaturalLanguage"))
            new_sect = new TextSection(new_style);
          else
            new_sect = new MathSection(new_style);
            
          insert(sect->index() + 1, new_sect);
          move_to(new_sect->abstract_content(), 0);
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
    
    bool was_inside_string = Impl(*this).is_inside_string();
    bool was_inside_alias  = Impl(*this).is_inside_alias();
    
    if(!was_inside_string && !was_inside_alias) {
      Impl(*this).handle_immediate_macros();
    }
    
    int oldpos = context.selection.start;
    int newpos = seq->insert(oldpos, unichar);
    move_to(seq, newpos);
    
    if(mseq && !was_inside_string && !was_inside_alias) {
      // handle "\alias" macros:
      if(unichar == ' '/* && !was_inside_string*/) {
        context.selection.start--;
        context.selection.end--;
        
        bool ok = Impl(*this).handle_macros();
        
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
        
        if(!Impl(*this).handle_macros())
          move_to(mseq, start);
      }
      
      if(unichar == '\n') {
        int line_start = oldpos;
        while(line_start > 0 && buf[line_start-1] != '\n')
          --line_start;
        
        int indent_end = line_start;
        while(indent_end < context.selection.start && (buf[indent_end] == ' ' || buf[indent_end] == '\t'))
          ++indent_end;
        
        newpos = seq->insert(newpos, mseq, line_start, indent_end);
        move_to(seq, newpos);
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
  auto_scroll = !mouse_history.is_mouse_down(this);
  
  Impl(*this).raw_select(box, start, end);
}

void Document::select_to(const VolatileSelection &sel) {
  if(!sel.null_or_selectable())
    return;
  
  select_range(sel_first.get_all(), sel);
}

void Document::select_range(const VolatileSelection &sel1, const VolatileSelection &sel2) {
  if(!sel1.null_or_selectable() || !sel2.null_or_selectable())
    return;
    
  sel_first.set(sel1);
  sel_last.set(sel2);
  auto_scroll = !is_mouse_down();
  
  int start1 = sel1.start;
  int end1   = sel1.end;
  int start2 = sel2.start;
  int end2   = sel2.end;
  
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
  
  Box *b1 = sel1.box;
  Box *b2 = sel2.box;
  int s1  = start1;
  int s2  = start2;
  int e1  = end1;
  int e2  = end2;
  int d1 = box_depth(b1);
  int d2 = box_depth(b2);
  
  while(d1 > d2) {
    if(b1->parent() && !b1->parent()->selection_exitable()) {
      if(b1->selectable()) {
        int o1 = box_order(b1, s1, b2, e2);
        int o2 = box_order(b1, e1, b2, s2);
        
        if(o1 > 0)
          Impl(*this).raw_select(b1, 0, e1);
        else if(o2 < 0)
          Impl(*this).raw_select(b1, s1, b1->length());
        else
          Impl(*this).raw_select(b1, 0, b1->length());
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
    if(b1->parent() && !b1->parent()->selection_exitable()) {
      if(b1->selectable()) {
        int o = box_order(b1, s1, b2, s2);
        
        if(o < 0)
          Impl(*this).raw_select(b1, s1, b1->length());
        else
          Impl(*this).raw_select(b1, 0, e1);
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
    if(!b1->selection_exitable())
      return;
      
    s1 = b1->index();
    e1 = s1 + 1;
    b1 = b1->parent();
  }
  if(b1)
    Impl(*this).raw_select(b1, s1, e1);
}

void Document::move_to(Box *box, int index, bool selecting) {
  if(selecting)
    select_to(VolatileSelection(box, index));
  else
    select(VolatileSelection(box, index));
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
    select_to(VolatileSelection(box, i));
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
  VolatileSelection new_sel = sel_last.get_all();
  if(!new_sel) {
    new_sel = context.selection.get_all();
    
    sel_last = context.selection;
    if(!new_sel)
      return;
  }
  
  if(new_sel.box == this && !new_sel.is_empty()) {
    if(selecting) {
      if(direction == LogicalDirection::Forward && new_sel.end < length())
        ++new_sel.end;
      else if(direction == LogicalDirection::Backward && new_sel.start > 0)
        --new_sel.start;
        
      select(new_sel);
      return;
    }
    
    if(direction == LogicalDirection::Forward)
      move_to(this, new_sel.end);
    else
      move_to(this, new_sel.start);
      
    return;
  }
  
  if(direction == LogicalDirection::Forward && new_sel.length() > 1) {
    new_sel.start = new_sel.end;
    if(!dynamic_cast<MathSequence *>(new_sel.box))
      --new_sel.start;
  }
  
  new_sel.box = new_sel.box->move_vertical(direction, &best_index_rel_x, &new_sel.start, false);
  new_sel.end = new_sel.start;
  if(auto seq = dynamic_cast<MathSequence *>(new_sel.box)) {
    if(seq->is_placeholder(new_sel.start - 1)) {
      --new_sel.start;
      best_index_rel_x += seq->glyph_array()[new_sel.start].right;
      if(new_sel.start > 0)
        best_index_rel_x -= seq->glyph_array()[new_sel.start - 1].right;
    }
    else if(seq->is_placeholder(new_sel.start))
      ++new_sel.end;
  }
  
  float tmp = best_index_rel_x;
  if(selecting)
    select_to(new_sel);
  else
    select(new_sel);
    
  if(context.selection.get() == new_sel.box)
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
    select_to(VolatileSelection(box, index));
  else
    select(VolatileSelection(box, index));
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

Section *Document::swap(int pos, Section *sect) {
  invalidate();
  if(nullptr == dynamic_cast<EditSection*>(sect) && nullptr == dynamic_cast<EditSection*>(section(pos)))
    native()->on_editing();
  
  return SectionList::swap(pos, sect);
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

VolatileSelection Document::prepare_copy() {
  if(selection_length() > 0) 
    return selection_now();
  
  Box *box = selection_box();
  if(box && !dynamic_cast<AbstractSequence *>(box)) {
    if(auto parent = dynamic_cast<AbstractSequence *>(box->parent())) {
      int i = box->index();
      return VolatileSelection(parent, i, i+1).normalized();
    }
  }
  
  return VolatileSelection(nullptr, -1, -1);
}

bool Document::can_copy() {
  return !prepare_copy().is_empty();
}

String Document::copy_to_text(String mimetype) {
  VolatileSelection sel = prepare_copy();
  if(!sel) {
    native()->beep();
    return String();
  }
  
  BoxOutputFlags flags = BoxOutputFlags::Default;
  bool is_plain_text = mimetype.equals(Clipboard::PlainText) || mimetype.equals("PlainText");
  if(is_plain_text)
    flags |= BoxOutputFlags::Literal | BoxOutputFlags::ShortNumbers;
    
  Expr boxes = sel.to_pmath(flags);
  if(mimetype.equals(Clipboard::BoxesText))
    return boxes.to_string(PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM);
    
  if( is_plain_text || mimetype.equals("InputText")) {
    Expr text = Application::interrupt_wait(
                  Parse("FE`BoxesToText(`1`, `2`)", boxes, mimetype),
                  Application::edit_interrupt_timeout);
                  
    return text.to_string();
  }
  
  native()->beep();
  return String();
}

void Document::copy_to_binary(String mimetype, Expr file) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    int start, end;
    
    VolatileSelection sel = prepare_copy();
    if(!sel) {
      native()->beep();
      return;
    }
    
    Expr boxes = sel.to_pmath(BoxOutputFlags::Default);
    file = Expr(pmath_file_create_compressor(file.release(), nullptr));
    pmath_serialize(file.get(), boxes.release(), 0);
    pmath_file_close(file.release());
    return;
  }
  
  String text = copy_to_text(mimetype);
  // pmath_file_write(file.get(), text.buffer(), 2 * (size_t)text.length());
  pmath_file_writetext(file.get(), text.buffer(), text.length());
}

void Document::prepare_copy_to_image(cairo_surface_t *target_surface, richmath::RectangleF *out_pix_rect) {
  cairo_t *cr = cairo_create(target_surface);
  
  cairo_set_line_width(cr, 1);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
  
  cairo_font_options_t *opt = cairo_font_options_create();
  cairo_font_options_set_antialias(opt, CAIRO_ANTIALIAS_GRAY);
  cairo_set_font_options(cr, opt);
  cairo_font_options_destroy(opt);

  prepare_copy_to_image(cr, out_pix_rect);
  
  cairo_destroy(cr);
  cairo_surface_flush(target_surface);
}

void Document::finish_copy_to_image(cairo_surface_t *target_surface, const richmath::RectangleF &pix_rect) {
  cairo_t *cr = cairo_create(target_surface);
  
  cairo_set_line_width(cr, 1);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_SQUARE);
  
  cairo_font_options_t *opt = cairo_font_options_create();
  cairo_font_options_set_antialias(opt, CAIRO_ANTIALIAS_GRAY);
  cairo_set_font_options(cr, opt);
  cairo_font_options_destroy(opt);

  finish_copy_to_image(cr, pix_rect);
  
  cairo_destroy(cr);
  cairo_surface_flush(target_surface);
}

void Document::prepare_copy_to_image(cairo_t *target_cr, richmath::RectangleF *out_pix_rect) {
  *out_pix_rect = RectangleF{0, 0, 0, 0};
  
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
  
  DragStatus old_drag_status = drag_status;
  drag_status = DragStatus::Idle;
  
  SelectionReference oldsel = context.selection;
  context.selection = SelectionReference();
  
  //selbox->invalidate();
  {
    Canvas canvas(target_cr);
    
    float sf = native()->scale_factor();
    canvas.scale(sf, sf);
    
    switch(cairo_surface_get_type(cairo_get_target(target_cr))) {
      case CAIRO_SURFACE_TYPE_IMAGE:
      case CAIRO_SURFACE_TYPE_WIN32:
        canvas.pixel_device = true;
        break;
        
      default:
        canvas.pixel_device = false;
        break;
    }
    
    canvas.set_font_size(10);// 10 * 4/3.
    
    paint_resize(canvas, true);
    
    copysel.get_all().add_path(canvas);
    
    cairo_matrix_t mat = canvas.get_matrix();
    canvas.reset_matrix();
    canvas.path_extents(out_pix_rect);
    canvas.set_matrix(mat);
    
    canvas.new_path();
  }
  
  context.selection = oldsel;
  drag_status = old_drag_status;
}

void Document::finish_copy_to_image(cairo_t *target_cr, const richmath::RectangleF &pix_rect) {
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
  
  DragStatus old_drag_status = drag_status;
  drag_status = DragStatus::Idle;
  
  SelectionReference oldsel = context.selection;
  context.selection = SelectionReference();
  
  //selbox->invalidate();
  {
    Canvas canvas(target_cr);
    
    float sf = native()->scale_factor();
    canvas.scale(sf, sf);
    
    switch(cairo_surface_get_type(cairo_get_target(target_cr))) {
      case CAIRO_SURFACE_TYPE_IMAGE:
      case CAIRO_SURFACE_TYPE_WIN32:
        canvas.pixel_device = true;
        break;
        
      default:
        canvas.pixel_device = false;
        break;
    }
    
    canvas.set_font_size(10);// 10 * 4/3.
    
    //paint_resize(&canvas, true);
    
    canvas.translate(-pix_rect.x / sf, -pix_rect.y / sf);
    
    if(0 == (CAIRO_CONTENT_ALPHA & cairo_surface_get_content(cairo_get_target(target_cr)))) {
      if(Color color = get_style(Background)) {
        canvas.set_color(color);
        canvas.paint();
      }
      else {
        canvas.set_color(Color::White);
        canvas.paint();
      }
    }
    
    Point scroll_pos = native()->scroll_pos();
    canvas.translate(-scroll_pos.x, -scroll_pos.y);
    copysel.get_all().add_path(canvas);
    canvas.clip();
    canvas.translate(scroll_pos.x, scroll_pos.y);
    
    canvas.set_color(get_style(FontColor, Color::Black));
    
    paint_resize(canvas, false);
  }
  
  context.selection = oldsel;
  drag_status = old_drag_status;
}

void Document::copy_to_clipboard(Clipboard *clipboard, String mimetype) {
  if(mimetype.equals(Clipboard::BoxesBinary)) {
    SharedPtr<OpenedClipboard> cb = clipboard->open_write();
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
    SharedPtr<OpenedClipboard> cb = clipboard->open_write();
    if(!cb) {
      native()->beep();
      return;
    }
    
    cb->add_text(Clipboard::PlainText, copy_to_text(mimetype));
    return;
  }
  
  if(cairo_surface_t *image = clipboard->create_image(mimetype, 1, 1)) {
    SharedPtr<OpenedClipboard> cb = clipboard->open_write();
    if(!cb) {
      native()->beep();
      return;
    }
    
    RectangleF rect;
    prepare_copy_to_image(image, &rect);
    cairo_surface_destroy(image);
    image = clipboard->create_image(mimetype, rect.width, rect.height);
    if(image) {
      finish_copy_to_image(image, rect);
      cb->add_image(mimetype, image);
      cairo_surface_destroy(image);
    }
  }
}

void Document::copy_to_clipboard(Clipboard *clipboard) {
  SharedPtr<OpenedClipboard> cb = clipboard->open_write();
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

void Document::cut_to_clipboard(Clipboard *clipboard) {
  copy_to_clipboard(clipboard);
  
  if(VolatileSelection sel = prepare_copy()) {
    select(sel);
    remove_selection(false);
  }
}

void Document::paste_from_boxes(Expr boxes) {
  if( context.selection.get() == this &&
      get_style(Editable, true))
  {
    if( boxes[0] == richmath_System_Section ||
        boxes[0] == richmath_System_SectionGroup)
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
    auto rect = grid->get_enclosing_range(context.selection.start, context.selection.end - 1);
    
    BoxInputFlags options = BoxInputFlags::Default;
    if(grid->get_style(AutoNumberFormating))
      options |= BoxInputFlags::FormatNumbers;
      
    MathSequence *tmp = new MathSequence;
    tmp->load_from_object(boxes, options);
    
    if(tmp->length() == 1 && tmp->count() == 1) {
      GridBox *tmpgrid = dynamic_cast<GridBox *>(tmp->item(0));
      
      if( tmpgrid && 
          tmpgrid->rows() <= rect.rows() && 
          tmpgrid->cols() <= rect.cols()) 
      {
        for(int col = 0; col < rect.cols(); ++col) {
          for(int row = 0; row < rect.rows(); ++row) {
            if( col < tmpgrid->cols() && row < tmpgrid->rows()) {
              grid->item(
                rect.y.start.primary_value() + row, 
                rect.x.start.primary_value() + col
              )->load_from_object(
                Expr(tmpgrid->item(row, col)->to_pmath(BoxOutputFlags::Default)),
                BoxInputFlags::FormatNumbers);
            }
            else {
              grid->item(
                rect.y.start.primary_value() + row, 
                rect.x.start.primary_value() + col
              )->load_from_object(
                String::FromChar(PMATH_CHAR_BOX),
                BoxInputFlags::Default);
            }
          }
        }
        
        MathSequence *sel = grid->item(
                              rect.y.start.primary_value() + tmpgrid->rows() - 1,
                              rect.x.start.primary_value() + tmpgrid->cols() - 1)->content();
                              
        move_to(sel, sel->length());
        grid->invalidate();
        
        tmp->safe_destroy();
        return;
      }
    }
    
    for(int col = 0; col < rect.cols(); ++col) {
      for(int row = 0; row < rect.rows(); ++row) {
        grid->item(
          rect.y.start.primary_value() + row, 
          rect.x.start.primary_value() + col
        )->load_from_object(
          String::FromChar(PMATH_CHAR_BOX),
          BoxInputFlags::Default);
      }
    }
    
    MathSequence *sel = grid->item(rect.y.start, rect.x.start)->content();
    sel->remove(0, 1);
    sel->insert(0, tmp); tmp = nullptr;
    move_to(sel, sel->length());
    
    grid->invalidate();
    grid->after_insertion(); // TODO: only call on new grid items.
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
    graphics->after_insertion();
  }
  
  remove_selection(false);
  
  if(Impl(*this).prepare_insert()) {
    if(auto seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
    
      BoxInputFlags options = BoxInputFlags::Default;
      if(seq->get_style(AutoNumberFormating))
        options |= BoxInputFlags::FormatNumbers;
        
      AbstractSequence *tmp = seq->create_similar();
      tmp->load_from_object(boxes, options);
      
      int ins_start = context.selection.end;
      int ins_end = seq->insert(ins_start, tmp);
      seq->after_insertion(ins_start, ins_end);
      select(seq, ins_end, ins_end);
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
    
    if(Impl(*this).prepare_insert()) {
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

void Document::paste_from_filenames(Expr list_of_files, bool import_contents) {
  if(list_of_files[0] != PMATH_SYMBOL_LIST) {
    native()->beep();
    return;
  }
  
  if(!import_contents) {
    String s = Evaluate(Parse("Map(`1`, InputForm).Row(\",\").ToString", list_of_files));
    paste_from_text(Clipboard::PlainText, s);
    return;
  }
  
  size_t len = list_of_files.expr_length();
  for(size_t i = 1; i <= len; ++i) {
    Expr content_boxes = Application::interrupt_wait(
      Parse("FE`Import`PasteFileNameContentBoxes(`1`)", list_of_files[i]),
      Application::edit_interrupt_timeout);
    
    if(content_boxes.is_null())
      continue;
    
    if(content_boxes == PMATH_SYMBOL_FAILED)       content_boxes = String("$Failed");
    else if(content_boxes == PMATH_SYMBOL_ABORTED) content_boxes = String("$Aborted");
    
    paste_from_boxes(content_boxes);
  }
}

void Document::paste_from_clipboard(Clipboard *clipboard) {
  if(clipboard->has_format(Clipboard::BoxesBinary)) {
    paste_from_binary(
      Clipboard::BoxesBinary,
      clipboard->read_as_binary_file(Clipboard::BoxesBinary));
    return;
  }
  
  if(clipboard->has_format(Clipboard::BoxesText)) {
    paste_from_text(
      Clipboard::BoxesText,
      clipboard->read_as_text(Clipboard::BoxesText));
    return;
  }
  
  if(clipboard->has_format(Clipboard::PlainText)) {
    paste_from_text(
      Clipboard::PlainText,
      clipboard->read_as_text(Clipboard::PlainText));
    return;
  }
  
  if(clipboard->has_format(Clipboard::PlatformFilesOrUris)) {
    paste_from_filenames(clipboard->read_as_filenames(), false);
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
        sect = Impl(*this).auto_make_text_or_math(sect);
        
      sect->invalidate();
    }
    
    return;
  }
  
  AbstractSequence *seq = dynamic_cast<AbstractSequence *>(sel);
  if(seq && start < end) {
  
    if(!seq->edit_selection(context.selection))
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
    Impl(*this).set_prev_sel_line();
    
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
  else if(scope == richmath_System_Section) {
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
  else if(scope == richmath_System_Section) {
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
  
  if(!Impl(*this).prepare_insert()) {
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
    if(Impl(*this).is_inside_string()) {
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
    
    if(autoformat && !Impl(*this).is_inside_string()) { // replace tokens from global_immediate_macros ...
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

static AbstractSequence *find_selection_placeholder(
  Box *current, 
  int *index, 
  Box *root, 
  int root_end,
  bool stop_early = false
) {
  assert(index != nullptr);
  
  AbstractSequence *placeholder_seq = nullptr;
  int placeholder_pos = -1;
  
  while(current && (current != root || *index < root_end)) {
    AbstractSequence *current_seq = dynamic_cast<AbstractSequence *>(current);
    
    if(current_seq && current_seq->is_placeholder(*index)) {
      if(stop_early || current_seq->char_at(*index) == PMATH_CHAR_SELECTIONPLACEHOLDER) {
        placeholder_seq = current_seq;
        placeholder_pos = *index;
        break;
      }
      
      if(!placeholder_seq) {
        placeholder_seq = current_seq;
        placeholder_pos = *index;
      }
    }
    
    current = current->move_logical(LogicalDirection::Forward, false, index);
  }
  
  *index = placeholder_pos;
  return placeholder_seq;
}

void Document::insert_box(Box *box, bool handle_placeholder) {
  if(!box || !Impl(*this).prepare_insert()) {
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
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
  }
  
  if(auto seq = dynamic_cast<AbstractSequence *>(context.selection.get())) {
    int rem_length = context.selection.length();
    int ins_start = context.selection.start;
    
    if(!handle_placeholder) {
      seq->remove(ins_start, ins_start + rem_length);
      rem_length = 0;
    }
    
    int ins_end = seq->insert(ins_start, box);
    box = nullptr;
    
    move_to(seq, ins_end);
    seq->after_insertion(ins_start, ins_end);
    
    if(handle_placeholder) {
      int pl_start = ins_start;
      AbstractSequence *pl_seq = find_selection_placeholder(seq, &pl_start, seq, ins_end);
      
      int pl_end = pl_start + 1;
      if(pl_seq && pl_seq == pl_seq->normalize_selection(&pl_start, &pl_end)) {
        int old_pl_len = pl_end - pl_start;
        
        pl_seq->remove(pl_start, pl_end);
        pl_end = pl_start;
        
        if(pl_seq == seq)
          ins_end-= old_pl_len;
        
        if(rem_length == 0) 
          pl_end = pl_seq->insert(pl_start, PMATH_CHAR_PLACEHOLDER);
        else 
          pl_end = pl_seq->insert(pl_start, seq, ins_end, ins_end + rem_length);
        
        if(pl_seq == seq)
          ins_end+= pl_end - pl_start;
        
        pl_start = ins_start;
        pl_seq = find_selection_placeholder(seq, &pl_start, seq, ins_end, true);
        if(pl_seq)
          select(pl_seq, pl_start, pl_start + 1);
        else
          move_to(seq, ins_end);
      }
      
      seq->remove(ins_end, ins_end + rem_length);
    }
    return;
  }
  
  box->safe_destroy();
}

void Document::insert_fraction() {
  if(!Impl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_fraction();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
  }
  
  select_prev(true);
  
  if(auto seq = dynamic_cast<MathSequence *>(context.selection.get())) {
    auto num = new MathSequence;
    auto den = new MathSequence;
    auto frac = new FractionBox(num, den);
    
    seq->insert(context.selection.end, frac);
    frac->after_insertion();
    
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
  if(!Impl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_column();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
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
      GridYIndex row;
      GridXIndex col;
      grid->index_to_yx(i, &row, &col);
      
      int col_val = col.primary_value();
      
      if( context.selection.end > 0 ||
          selection_box() != grid->item(0, col_val)->content())
      {
        ++col_val;
      }
      
      grid->insert_cols(col_val, 1);
      select(grid->item(0, col_val)->content(), 0, 1);
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
  if(!Impl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_matrix_row();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
  }
  
  GridBox *grid = nullptr;
  
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
      GridYIndex row;
      GridXIndex col;
      grid->index_to_yx(i, &row, &col);
      
      int row_val = row.primary_value();
      
      if( context.selection.end > 0 ||
          context.selection.get() != grid->item(row_val, 0)->content())
      {
        ++row_val;
      }
      
      grid->insert_rows(row_val, 1);
      select(grid->item(row_val, 0)->content(), 0, 1);
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
  if(!Impl(*this).prepare_insert_math(false)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_sqrt();
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
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
  if(!Impl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_subsuperscript(sub);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
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
  if(!Impl(*this).prepare_insert_math(true)) {
    Document *cur = get_current_document();
    
    if(cur && cur != this) {
      cur->insert_underoverscript(under);
    }
    else {
      native()->beep();
    }
    
    return;
  }
  
  if( !Impl(*this).is_inside_string() &&
      !Impl(*this).is_inside_alias() &&
      !Impl(*this).handle_immediate_macros())
  {
    Impl(*this).handle_macros();
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
    
  if(selection_box() && !selection_box()->edit_selection(context.selection))
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
    native()->on_editing();
    int start = context.selection.start;
    Box *box = grid->remove_range(&start, context.selection.end);
    select(box, start, start);
    return true;
  }
  
  if(context.selection.id == this->id()) {
    native()->on_editing();
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

Expr Document::get_current_value_of_MouseOverBox(FrontEndObject *obj, Expr item) {
  FrontEndReference ref = mouse_history.observable_mouseover_box_id;
  Box *box = FrontEndObject::find_cast<Box>(ref);
  if(!box)
    return Symbol(PMATH_SYMBOL_NONE);
  
  if(dynamic_cast<AbstractSequence*>(box) && box->parent())
    box = box->parent();
  
  return box->to_pmath_id();
}

void Document::reset_mouse() {
  drag_status = DragStatus::Idle;
  
  mouse_history.reset();
  Application::delay_dynamic_updates(false);
  
  Application::deactivated_all_controls();
  //Application::update_control_active(native()->is_mouse_down());
}

bool Document::is_mouse_down() {
  return mouse_history.is_mouse_down(this);
}

void Document::stylesheet(SharedPtr<Stylesheet> new_stylesheet) {
  assert(new_stylesheet);
  if(new_stylesheet != context.stylesheet) {
    new_stylesheet->add_user(this);
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
      new_stylesheet->add_user(this);
      context.stylesheet = new_stylesheet;
      style->set(InternalLastStyleDefinitions, styledef);
      invalidate_all();
      return true;
    }
  }
  return false;
}

void Document::reset_style() {
  Style::reset(style, "Document");
}

void Document::paint_resize(Canvas &canvas, bool resize_only) {
  context.with_canvas(canvas, [&]() {
    if(update_dynamic_styles(context)) {
      if(get_own_style(InternalHasModifiedWindowOption)) {
        style->set(InternalHasModifiedWindowOption, false);
        native()->invalidate_options();
      }
    }

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
    
    ContextState cc(context);
    cc.begin(style);
    
    RectangleF page_rect{ native()->scroll_pos(), native()->page_size() };
    _page_width   = page_rect.width;
    _scrollx      = page_rect.x;
    _window_width = native()->window_size().x;
    
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
    
    canvas.translate(-page_rect.x, -page_rect.y);
    
    init_section_bracket_sizes(context);
    
    int sel_sect = -1;
    Box *b = context.selection.get();
    while(b && b != this) {
      sel_sect = b->index();
      b = b->parent();
    }
    
    int i = 0;
    while(i < length() && _extents.descent <= page_rect.top()) {
      if(section(i)->must_resize) // || i == sel_sect)
        resize_section(context, i);
        
      Impl(*this).after_resize_section(i);
      ++i;
    }
    
    int first_visible_section = i - 1;
    if(first_visible_section < 0)
      first_visible_section = 0;
      
    while(i < length() && _extents.descent <= page_rect.bottom()) {
      if(section(i)->must_resize) // || i == sel_sect)
        resize_section(context, i);
        
      Impl(*this).after_resize_section(i);
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
          resize_section(context, i);
        }
      }
      
      Impl(*this).after_resize_section(i);
      ++i;
    }
    
    finish_resize(context);
    
    if(!resize_only) {
      Impl(*this).add_selection_highlights(0, length());
  //    Impl(*this).add_selection_highlights(first_visible_section, last_visible_section);
      
      {
        float y = 0;
        if(first_visible_section < length())
          y += section(first_visible_section)->y_offset;
        canvas.move_to(0, y);
      }
      
      if(Color bg = get_style(Background)) {
        context.cursor_color = bg.is_dark() ? Color::White : Color::Black;
      }
      
      for(i = first_visible_section; i <= last_visible_section; ++i) {
        paint_section(context, i);
      }
      
      context.pre_paint_hooks.clear();
      context.post_paint_hooks.clear();
      
      Impl(*this).paint_cursor_and_flash();
      
      if(drag_source != context.selection && drag_status == DragStatus::CurrentlyDragging) {
        if(VolatileSelection drag_src = drag_source.get_all()) {
          drag_src.add_path(canvas);
          context.draw_selection_path();
        }
      }
      
      if(DebugFollowMouse) {
        if(VolatileSelection ms = mouse_history.debug_move_sel.get_all()) {
          ms.add_path(canvas);
          if(Impl::is_inside_string(ms.box, ms.start))
            canvas.set_color(DebugFollowMouseInStringColor);
          else
            canvas.set_color(DebugFollowMouseColor);
          canvas.hair_stroke();
        }
      }
      
      if(DebugSelectionBounds) {
        if(VolatileSelection sel = selection_now()) {
          canvas.save();
          {
            sel.add_path(canvas);
            
            static const double dashes[] = {1.0, 2.0};
            
            double x1, y1, x2, y2;
            canvas.path_extents(&x1, &y1, &x2, &y2);
            canvas.new_path();
            
            if(canvas.pixel_device) {
              canvas.user_to_device(&x1, &y1);
              canvas.user_to_device(&x2, &y2);
              
              x2 = floor(x2 + 0.5) - 0.5;
              y2 = floor(y2 + 0.5) - 0.5;
              x1 = ceil(x1 - 0.5) + 0.5;
              y1 = ceil(y1 - 0.5) + 0.5;
              
              canvas.device_to_user(&x1, &y1);
              canvas.device_to_user(&x2, &y2);
            }
            
            canvas.move_to(x1, page_rect.top());
            canvas.line_to(x1, page_rect.bottom());
            
            canvas.move_to(x2, page_rect.top());
            canvas.line_to(x2, page_rect.bottom());
            
            canvas.move_to(page_rect.left(),  y1);
            canvas.line_to(page_rect.right(), y1);
            
            canvas.move_to(page_rect.left(),  y2);
            canvas.line_to(page_rect.right(), y2);
            canvas.close_path();
            
            canvas.set_color(DebugSelectionBoundsColor);
            cairo_set_dash(canvas.cairo(), dashes, sizeof(dashes) / sizeof(double), 0.5);
            canvas.hair_stroke();
          }
          canvas.restore();
        }
      }
      
      if(auto_scroll) {
        auto_scroll = false;
        if(VolatileSelection box_range = sel_last.get_all())
          box_range.box->scroll_to(canvas, box_range);
      }
      
      if(selection_length() == 1 && best_index_rel_x == 0) {
        if(auto seq = dynamic_cast<MathSequence *>(selection_box())) {
          best_index_rel_x = seq->glyph_array()[selection_end() - 1].right;
          if(selection_start() > 0)
            best_index_rel_x -= seq->glyph_array()[selection_start() - 1].right;
            
          best_index_rel_x /= 2;
        }
      }
      
      canvas.translate(page_rect.x, page_rect.y);
      
      if(last_paint_sel != context.selection) {
        last_paint_sel = context.selection;
        
        for(auto sel : additional_selection) {
          if(Box *b = sel.get())
            b->request_repaint_range(sel.start, sel.end);
        }
        additional_selection.length(0);
      }
    }
    
    cc.end();
    must_resize_min = 0;
  });
}

Expr Document::to_pmath(BoxOutputFlags flags) {
  Gather g;
  
  Expr content = SectionList::to_pmath(flags);
  if(content[0] == richmath_System_SectionGroup) {
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

Expr Document::to_pmath_id() {
  return Call(Symbol(richmath_System_DocumentObject), id().to_pmath());
}

//} ... class Document

//{ class Document::Impl ...

void Document::Impl::raw_select(Box *box, int start, int end) {
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
  }
  
  self.best_index_rel_x = 0;
}

void Document::Impl::after_resize_section(int i) {
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
void Document::Impl::add_fill(PaintHookManager &hooks, Box *box, int start, int end, Color color, float alpha) {
  SelectionReference ref;
  ref.set(box, start, end);
  
  hooks.add(ref.get(), new SelectionFillHook(ref.start, ref.end, color, alpha));
  self.additional_selection.add(ref);
}

void Document::Impl::add_pre_fill(Box *box, int start, int end, Color color, float alpha) {
  add_fill(self.context.pre_paint_hooks, box, start, end, color, alpha);
}

void Document::Impl::add_selected_word_highlight_hooks(int first_visible_section, int last_visible_section) {
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
        add_fill(temp_hooks, box, s, e, OccurenceBackgroundColor);
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

bool Document::Impl::word_occurs_outside_visible_range(String str, int first_visible_section, int last_visible_section) {
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

void Document::Impl::add_matching_bracket_hook() {
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
            
          add_pre_fill(seq, head->start(), head->end() + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
          
          // opening parenthesis, always exists
          add_pre_fill(seq, span->item_pos(1), span->item_pos(1) + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
          
          // closing parenthesis, last item, might not exist
          int clos = span->count() - 1;
          if(clos >= 2 && span->item_equals(clos, ")")) {
            add_pre_fill(seq, span->item_pos(clos), span->item_pos(clos) + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
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
          add_pre_fill(seq, head->start(), head->end() + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
          
          // dot, always exists
          add_pre_fill(seq, span->item_pos(1), span->item_pos(1) + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
          
          // opening parenthesis, might not exist
          if(span->count() > 3) {
            add_pre_fill(seq, span->item_pos(3), span->item_pos(3) + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
          }
          
          // closing parenthesis, last item, might not exist
          int clos = span->count() - 1;
          if(clos >= 2 && span->item_equals(clos, ")")) {
            add_pre_fill(seq, span->item_pos(clos), span->item_pos(clos) + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
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
      add_pre_fill(seq, pos,           pos + 1,           MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
      add_pre_fill(seq, other_bracket, other_bracket + 1, MatchingBracketHighlightColor, MatchingBracketHighlightAlpha);
    }
  }
  
  return;
}

void Document::Impl::add_autocompletion_hook() {
  Box *box = self.auto_completion.range.get();
  
  if(!box)
    return;
    
  add_pre_fill(
    box,
    self.auto_completion.range.start,
    self.auto_completion.range.end,
    AutoCompleteHighlightColor,
    AutoCompleteHighlightAlpha);
}

void Document::Impl::add_selection_highlights(int first_visible_section, int last_visible_section) {
  add_matching_bracket_hook();
  add_selected_word_highlight_hooks(first_visible_section, last_visible_section);
  add_autocompletion_hook();
}
//}

//{ cursor painting
void Document::Impl::paint_document_cursor() {
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
    
    self.context.canvas().align_point(&x1, &y1, true);
    self.context.canvas().align_point(&x2, &y2, true);
    self.context.canvas().move_to(x1, y1);
    self.context.canvas().line_to(x2, y2);
    
    self.context.canvas().set_color(DocumentCursorLineColor);
    self.context.canvas().hair_stroke();
    
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
    
    self.context.canvas().align_point(&x1, &y1, true);
    self.context.canvas().align_point(&x2, &y2, true);
    self.context.canvas().move_to(x1, y1);
    self.context.canvas().line_to(x2, y2);
    
    self.context.draw_selection_path();
  }
}

void Document::Impl::paint_flashing_cursor_if_needed() {
  if(self.prev_sel_line >= 0) {
    AbstractSequence *seq = dynamic_cast<AbstractSequence *>(self.selection_box());
    
    if(seq && seq->id() == self.prev_sel_box_id) {
      int line = seq->get_line(self.selection_end(), self.prev_sel_line);
      
      if(line != self.prev_sel_line) {
        self.flashing_cursor_circle = new BoxRepaintEvent(self.id()/*self.prev_sel_box_id*/, 0.0);
      }
    }
    
    self.prev_sel_line = -1;
    self.prev_sel_box_id = FrontEndReference::None;
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
      
      self.context.canvas().save();
      {
        self.context.canvas().user_to_device(&x1, &y1);
        self.context.canvas().user_to_device(&x2, &y2);
        
        self.context.canvas().reset_matrix();
        
        double dx = x2 - x1;
        double dy = y2 - y1;
        double h = hypot(dx, dy);
        double c = dx / h;
        double s = dy / h;
        double x = (x1 + x2) / 2;
        double y = (y1 + y2) / 2;
        cairo_matrix_t mat;
        mat.xx = c;
        mat.yx = s;
        mat.xy = -s;
        mat.yy = c;
        mat.x0 = x - c * x + s * y;
        mat.y0 = y - s * x - c * y;
        self.context.canvas().transform(mat);
        self.context.canvas().translate(x, y);
        self.context.canvas().scale(r + h / 2, r);
        
        self.context.canvas().arc(0, 0, 1, 0, 2 * M_PI, false);
        
        cairo_set_operator(self.context.canvas().cairo(), CAIRO_OPERATOR_DIFFERENCE);
        self.context.canvas().set_color(Color::White);
        self.context.canvas().fill();
      }
      self.context.canvas().restore();
    }
  }
}

void Document::Impl::paint_cursor_and_flash() {
  paint_document_cursor();
  paint_flashing_cursor_if_needed();
}
//}

//{ insertion
inline bool Document::Impl::is_inside_string() {
  return is_inside_string(self.context.selection.get(), self.context.selection.start);
}

bool Document::Impl::is_inside_string(Box *box, int index) {
  while(box) {
    if(auto seq = dynamic_cast<MathSequence *>(box)) {
      if(seq->is_inside_string(index))
        return true;
    }
    else if(dynamic_cast<TextSequence *>(box))
      return true;
    
    index = box->index();
    box = box->parent();
  }
  
  return false;
}

bool Document::Impl::is_inside_alias() {
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

// sub.start and sub.end may lie outside 0..sub.box->length()
bool Document::Impl::is_inside_selection(const VolatileSelection &sub) {
  if(self.selection_box() && self.selection_length() > 0) {
    // section selections are only at the right margin, the section content is
    // not inside the selection-frame
    if(self.selection_box() == &self && sub.box != &self)
      return false;
      
    if(sub.start == sub.end)
      return false;
      
    Box *b = sub.box;
    int substart = sub.start;
    int subend = sub.end;
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

bool Document::Impl::is_inside_selection(const VolatileSelection &sub, bool was_inside_start) {
  if(!self.selection_box())
    return false;
  
  if(sub.box && sub.box != &self && sub.start == sub.end) {
    if(was_inside_start)
      return is_inside_selection(VolatileSelection(sub.box, sub.start, sub.start + 1));
    else
      return is_inside_selection(VolatileSelection(sub.box, sub.start - 1, sub.start));
  }
  
  return is_inside_selection(sub);
}

void Document::Impl::set_prev_sel_line() {
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(self.selection_box())) {
    self.prev_sel_line = seq->get_line(self.selection_end(), self.prev_sel_line);
    self.prev_sel_box_id = seq->id();
  }
  else {
    self.prev_sel_line = -1;
    self.prev_sel_box_id = FrontEndReference::None;
  }
}

bool Document::Impl::prepare_insert() {
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
        lang = all->get_with_base(section_style, LanguageCategory);
    }
    
    Section *sect;
    if(lang.equals("NaturalLanguage"))
      sect = new TextSection(section_style);
    else
      sect = new MathSection(section_style);
      
    self.native()->on_editing();
    self.insert(self.context.selection.start, sect);
    sect->after_insertion();
    self.move_horizontal(LogicalDirection::Forward, false);
    
    return true;
  }
  else {
    if(self.selection_box() && self.selection_box()->edit_selection(self.context.selection)) {
      self.native()->on_editing();
      set_prev_sel_line();
      return true;
    }
  }
  
  return false;
}

bool Document::Impl::prepare_insert_math(bool include_previous_word) {
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

Section *Document::Impl::auto_make_text_or_math(Section *sect) {
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
Section *Document::Impl::convert_content(Section *sect) {
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
void Document::Impl::handle_key_left_right(SpecialKeyEvent &event, LogicalDirection direction) {
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

void Document::Impl::handle_key_home_end(SpecialKeyEvent &event, LogicalDirection direction) {
  self.move_start_end(direction, event.shift);
  event.key = SpecialKey::Unknown;
  self.auto_completion.stop();
}

void Document::Impl::handle_key_up_down(SpecialKeyEvent &event, LogicalDirection direction) {
  self.move_vertical(direction, event.shift);
  event.key = SpecialKey::Unknown;
  self.auto_completion.stop();
}

void Document::Impl::handle_key_pageup_pagedown(SpecialKeyEvent &event, LogicalDirection direction) {
  if(!self.native()->is_scrollable())
    return;
    
  float h = self.native()->window_size().y;
  if(direction == LogicalDirection::Backward)
    h = -h;
    
  self.native()->scroll_by(0, h);
  
  event.key = SpecialKey::Unknown;
}

void Document::Impl::handle_key_tab(SpecialKeyEvent &event) {
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

bool Document::Impl::is_tabkey_only_moving() {
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

void Document::Impl::handle_key_backspace(SpecialKeyEvent &event) {
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
    auto old_id = self.context.selection.id;
    int old_sel_end = self.selection_end();
    self.move_horizontal(LogicalDirection::Backward, event.ctrl, event.ctrl || event.shift);
    
    if(old_id == self.context.selection.id) {
      if(self.selection_end() != old_sel_end)
        self.select(selbox, self.selection_start(), old_sel_end);
      
      self.remove_selection(true);
      
      // reset sel_last:
      selbox = self.context.selection.get();
      self.select(selbox, self.context.selection.start, self.context.selection.end);
    }
    else {
      if(self.selection_length() == 0 && self.selection_start() > 0) {
        if(AbstractSequence *seq = dynamic_cast<AbstractSequence*>(self.selection_box())) {
          if(seq->is_placeholder(self.selection_start() - 1)) {
            self.select(seq, self.selection_start() - 1, self.selection_end());
          }
        }
      }
      event.key = SpecialKey::Unknown;
      return;
    }
  }
  else
    self.move_horizontal(LogicalDirection::Backward, event.ctrl);
    
  event.key = SpecialKey::Unknown;
}

void Document::Impl::handle_key_delete(SpecialKeyEvent &event) {
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

void Document::Impl::handle_key_escape(SpecialKeyEvent &event) {
  if(self.context.clicked_box_id) {
    if(auto receiver = FrontEndObject::find_cast<Box>(self.context.clicked_box_id))
      receiver->on_mouse_cancel();
      
    self.context.clicked_box_id = FrontEndReference::None;
    event.key = SpecialKey::Unknown;
    return;
  }
  
  self.key_press(PMATH_CHAR_ALIASDELIMITER);
  event.key = SpecialKey::Unknown;
}
//}

//{ macro handling
inline bool Document::Impl::handle_immediate_macros() {
  return handle_immediate_macros(global_immediate_macros);
}

bool Document::Impl::handle_immediate_macros(const Hashtable<String, Expr> &table) {
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

inline bool Document::Impl::handle_macros() {
  return handle_macros(global_macros);
}

bool Document::Impl::handle_macros(const Hashtable<String, Expr> &table) {
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

//} ... class Document::Impl
