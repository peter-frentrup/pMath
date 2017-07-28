#ifndef __GUI__DOCUMENT_H__
#define __GUI__DOCUMENT_H__

#include <boxes/sectionlist.h>
#include <eval/application.h>
#include <util/autocompletion.h>


namespace richmath {
  class BoxRepaintEvent;
  class MouseEvent;
  class NativeWidget;
  class SpecialKeyEvent;
  
  extern bool DebugFollowMouse;
  extern bool DebugSelectionBounds;
  
  typedef enum {
    DragStatusIdle,
    DragStatusMayDrag,
    DragStatusCurrentlyDragging
  } DragStatus;
  
  class Document final: public SectionList {
      friend class NativeWidget;
      friend class DocumentImpl;
    public:
      Document();
      ~Document();
      
      virtual bool try_load_from_object(Expr expr, BoxOptions options) override;
      
      virtual bool request_repaint(float x, float y, float w, float h) override;
      virtual void invalidate() override;
      virtual void invalidate_options() override;
      void invalidate_all();
      
      NativeWidget *native() { return _native; } // never nullptr
      
      virtual void scroll_to(float x, float y, float w, float h) override;
      virtual void scroll_to(Canvas *canvas, Box *child, int start, int end) override;
      
      void mouse_exit();
      void mouse_down(MouseEvent &event);
      void mouse_up(MouseEvent &event);
      void mouse_move(MouseEvent &event);
      void finish_editing(Box *except_box);
      
      void focus_set();
      void focus_killed();
      
      void key_down(SpecialKeyEvent &event);
      void key_up(SpecialKeyEvent &event);
      void key_press(uint16_t unicode);
      
      virtual Box *mouse_sensitive() override { return this; }
      virtual void on_mouse_down(MouseEvent &event) override;
      virtual void on_mouse_move(MouseEvent &event) override;
      virtual void on_mouse_up(MouseEvent &event) override;
      virtual void on_mouse_cancel() override;
      
      virtual void on_key_down(SpecialKeyEvent &event) override;
      virtual void on_key_up(SpecialKeyEvent &event) override;
      virtual void on_key_press(uint32_t unichar) override;
      
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
      
      void select_prev(bool operands_only);
      
      virtual void insert_pmath(int *pos, Expr boxes, int overwrite_until_index = 0) override;
      virtual void insert(int pos, Section *section) override;
      virtual Section *swap(int pos, Section *section) override;
      
      Box *prepare_copy(int *start, int *end);
      bool can_copy();
      String copy_to_text(String mimetype);
      void copy_to_binary(String mimetype, Expr file);
      void copy_to_image(cairo_surface_t *target, bool calc_size_only, double *device_width, double *device_height);
      void copy_to_clipboard(String mimetype);
      void copy_to_clipboard();
      void cut_to_clipboard();
      
      void paste_from_boxes(Expr boxes);
      void paste_from_text(String mimetype, String data);
      void paste_from_binary(String mimetype, Expr file);
      void paste_from_clipboard();
      
      void set_selection_style(Expr options);
      
      MenuCommandStatus can_do_scoped(Expr cmd, Expr scope);
      bool                  do_scoped(Expr cmd, Expr scope);
      
      bool split_section( bool do_it = true);
      bool merge_sections(bool do_it = true);
      
      void graphics_original_size();
      void graphics_original_size(Box *box);
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
      
      const Array<SelectionReference> &current_word_references() { return _current_word_references; }
      
      int clicked_box_id() {   return context.clicked_box_id; }
      int mouseover_box_id() { return context.mouseover_box_id; }
      void reset_mouse();
      bool is_mouse_down() { return mouse_down_counter > 0; }
      
      virtual SharedPtr<Stylesheet> stylesheet() override { return context.stylesheet; }
      virtual void reset_style() override;
      
      void paint_resize(Canvas *canvas, bool resize_only);
      
      Expr section_list_to_pmath(BoxFlags flags, int start, int end) {
        return SectionList::to_pmath(flags, start, end);
      }
      virtual Expr to_pmath(BoxFlags flags) override;
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_DOCUMENT); }
      
    public:
      Document *main_document; // not owned
      
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
      
      /* When the selection changes, these ranges need to be repainted, too. */
      Array<SelectionReference> additional_selection;
      
      Array<SelectionReference> _current_word_references;
      
      SelectionReference        last_paint_sel;
      
      AutoCompletion auto_completion;
  };
  
  extern Hashtable<String, Expr, object_hash> global_immediate_macros;
  extern Hashtable<String, Expr, object_hash> global_macros;
  
  extern Box *expand_selection(Box *box, int *start, int *end);
  extern int box_depth(Box *box);
  extern int box_order(Box *b1, int i1, Box *b2, int i2);
}

#endif // __GUI__DOCUMENT_H__
