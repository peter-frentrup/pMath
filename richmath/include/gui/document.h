#ifndef __GUI__DOCUMENT_H__
#define __GUI__DOCUMENT_H__

#include <boxes/sectionlist.h>
#include <graphics/context.h>


namespace richmath {
  class BoxRepaintEvent;
  class MouseEvent;
  class NativeWidget;
  class SpecialKeyEvent;
  
  extern bool DebugFollowMouse;
  
  typedef enum {
    DragStatusIdle,
    DragStatusMayDrag,
    DragStatusCurrentlyDragging
  } DragStatus;
  
  class Document: public SectionList {
      friend class NativeWidget;
    public:
      Document();
      ~Document();
      
      virtual bool try_load_from_object(Expr expr, int options);
      
      virtual bool request_repaint(float x, float y, float w, float h);
      virtual void invalidate();
      virtual void invalidate_options();
      void invalidate_all();
      
      NativeWidget *native() { return _native; } // never NULL
      
      virtual void scroll_to(float x, float y, float w, float h);
      virtual void scroll_to(Canvas *canvas, Box *child, int start, int end);
      
      void mouse_exit();
      void mouse_down(MouseEvent &event);
      void mouse_up(MouseEvent &event);
      void mouse_move(MouseEvent &event);
      
      void focus_set();
      void focus_killed();
      
      void key_down(SpecialKeyEvent &event);
      void key_up(SpecialKeyEvent &event);
      void key_press(uint16_t unicode);
      
      virtual Box *mouse_sensitive() { return this; }
      virtual void on_mouse_down(MouseEvent &event);
      virtual void on_mouse_move(MouseEvent &event);
      virtual void on_mouse_up(MouseEvent &event);
      virtual void on_mouse_cancel();
      
      virtual void on_key_down(SpecialKeyEvent &event);
      virtual void on_key_up(SpecialKeyEvent &event);
      virtual void on_key_press(uint32_t unichar);
      
      // substart and subend may lie outside 0..subbox->length()
      bool is_inside_selection(Box *subbox, int substart, int subend);
      bool is_inside_selection(Box *subbox, int substart, int subend, bool was_inside_start);
      
      void select(Box *box, int start, int end);
      void select_to(Box *box, int start, int end);
      void select_range(
        Box *box1, int start1, int end1,
        Box *box2, int start2, int end2);
        
      void move_to(Box *box, int index, bool selecting = false);
      
      void move_horizontal(
        LogicalDirection direction,
        bool             jumping,
        bool             selecting = false);
        
      void move_vertical(
        LogicalDirection direction,
        bool             selecting = false);
        
      void move_start_end(
        LogicalDirection direction,
        bool             selecting = false);
        
      void move_tab(LogicalDirection direction);
      
      bool is_inside_string();
      bool is_inside_string(Box *box, int index);
      bool is_inside_alias();
      bool is_tabkey_only_moving();
      
      void select_prev(bool operands_only);
      
      virtual void insert_pmath(int *pos, Expr boxes, int overwrite_until_index = 0);
      virtual void insert(int pos, Section *section);
      virtual Section *swap(int pos, Section *section);
      
      Box *prepare_copy(int *start, int *end);
      bool can_copy();
      String copy_to_text(String mimetype);
      void copy_to_binary(String mimetype, Expr file);
      void copy_to_clipboard();
      void cut_to_clipboard();
      
      void paste_from_boxes(Expr boxes);
      void paste_from_text(String mimetype, String data);
      void paste_from_binary(String mimetype, Expr file);
      void paste_from_clipboard();
      
      void insert_string(String text, bool autoformat = true);
      void insert_box(Box *box, bool handle_placeholder = false); // deletes the box
      void insert_fraction();
      void insert_matrix_column();
      void insert_matrix_row();
      void insert_sqrt();
      void insert_subsuperscript(bool sub);
      void insert_underoverscript(bool under);
      
      bool remove_selection(bool insert_default = true);
      
      void toggle_open_close_current_group();
      
      void complete_box();
      
      Box *selection_box() {   return context.selection.get();   }
      int selection_start() {  return context.selection.start; }
      int selection_end() {    return context.selection.end;   }
      int selection_length() { return context.selection.end - context.selection.start; }
      
      int clicked_box_id() {   return context.clicked_box_id; }
      int mouseover_box_id() { return context.mouseover_box_id; }
      void reset_mouse();
      bool is_mouse_down() { return mouse_down_counter > 0; }
      
      SharedPtr<Stylesheet> stylesheet() { return context.stylesheet; }
      
      void paint_resize(Canvas *canvas, bool resize_only);
      
      Expr section_list_to_pmath(int flags, int start, int end) {
        return SectionList::to_pmath(flags, start, end);
      }
      virtual Expr to_pmath(int flags);
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_DOCUMENT); }
      
    public:
      Document *main_document; // not owned
      
    protected:
      void raw_select(Box *box, int start, int end);
      
      void set_prev_sel_line();
      bool prepare_insert();
      bool prepare_insert_math(bool include_previous_word);
      
      bool handle_immediate_macros(
        const Hashtable<String, Expr, object_hash> &table);
      bool handle_macros(
        const Hashtable<String, Expr, object_hash> &table);
        
      bool handle_immediate_macros();
      bool handle_macros();
      
    protected:
      Context     context;
      float       best_index_rel_x;
      int         prev_sel_line;
      int         prev_sel_box_id;
      int         must_resize_min;
      DragStatus  drag_status;
      bool        auto_scroll;
      
      SharedPtr<BoxRepaintEvent> flashing_cursor_circle;
      
      SelectionReference sel_first;
      SelectionReference sel_last;
      
      SelectionReference drag_source;
      
    private:
      NativeWidget *_native;
      
      int mouse_down_counter;
      double mouse_down_time;
      float mouse_down_x;
      float mouse_down_y;
      SelectionReference mouse_down_sel;
      
      SelectionReference mouse_move_sel;
  };
  
  extern Hashtable<String, Expr, object_hash> global_immediate_macros;
  extern Hashtable<String, Expr, object_hash> global_macros;
  
  extern Box *expand_selection(Box *box, int *start, int *end);
  extern int box_depth(Box *box);
  extern int box_order(Box *b1, int i1, Box *b2, int i2);
}

#endif // __GUI__DOCUMENT_H__
