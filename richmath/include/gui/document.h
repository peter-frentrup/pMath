#ifndef RICHMATH__GUI__DOCUMENT_H__INCLUDED
#define RICHMATH__GUI__DOCUMENT_H__INCLUDED

#include <boxes/sectionlist.h>
#include <eval/application.h>
#include <graphics/rectangle.h>
#include <gui/menus.h>
#include <syntax/autocompletion.h>


namespace richmath {
  class BoxRepaintEvent;
  class Clipboard;
  class MouseEvent;
  class NativeWidget;
  class SpecialKeyEvent;
  
  extern bool DebugFollowMouse;
  extern bool DebugSelectionBounds;
  
  enum class DragStatus : uint8_t {
    Idle,
    MayDrag,
    CurrentlyDragging
  };
  
  struct BoxAttchmentPopup {
    SelectionReference anchor;
    FrontEndReference  popup_id;
    
    Document *popup_document() const { return FrontEndObject::find_cast<Document>(popup_id); }
  
    friend void swap(BoxAttchmentPopup &left, BoxAttchmentPopup &right) {
      using std::swap;
      swap(left.anchor,   right.anchor);
      swap(left.popup_id, right.popup_id);
    }
    
  };
  
  class Document final : public SectionList {
      friend class NativeWidget;
      class Impl;
    public:
      Document();
      ~Document();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags options) override;
      
      virtual bool request_repaint(const RectangleF &rect) override;
      virtual bool visible_rect(RectangleF &rect, Box *top_most) override;
      virtual void invalidate() override;
      virtual void invalidate_options() override;
      void invalidate_all();
      
      virtual bool changes_children_style() override { return true; }
      
      NativeWidget *native() { return _native; } // never nullptr
      
      virtual void scroll_to(const RectangleF &rect) override;
      virtual void scroll_to(Canvas &canvas, const VolatileSelection &child_sel) override;
      
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
      
      void select(const VolatileSelection &sel) { select(sel.box, sel.start, sel.end); }
      void select(Box *box, int start, int end);
      void select_to(const VolatileSelection &sel);
      void select_range(const VolatileSelection &sel1, const VolatileSelection &sel2);
        
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
      virtual Section *swap(int pos, Section *sect) override;
      
      VolatileSelection prepare_copy();
      bool can_copy();
      String copy_to_text(String mimetype);
      void copy_to_binary(String mimetype, Expr file);
      
      RectangleF prepare_copy_to_image(cairo_surface_t *target_surface);
      RectangleF prepare_copy_to_image(cairo_t         *target_cr);
      void finish_copy_to_image(cairo_surface_t *target_surface, const RectangleF &pix_rect);
      void finish_copy_to_image(cairo_t         *target_cr,      const RectangleF &pix_rect);
      
      void copy_to_clipboard(Clipboard *clipboard, String mimetype);
      void copy_to_clipboard(Clipboard *clipboard);
      void cut_to_clipboard(Clipboard *clipboard);
      
      void paste_from_boxes(Expr boxes);
      void paste_from_text(String mimetype, String data);
      void paste_from_binary(String mimetype, Expr file);
      void paste_from_filenames(Expr list_of_files, bool import_contents);
      void paste_from_clipboard(Clipboard *clipboard);
      
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
      
      bool remove_selection(SelectionReference &sel, bool insert_default = true);
      bool remove_selection(bool insert_default = true);
      
      void toggle_open_close_current_group();
      
      void complete_box();
      
      Box *selection_box() {   return context.selection.get();   }
      int selection_start() {  return context.selection.start; }
      int selection_end() {    return context.selection.end;   }
      int selection_length() { return context.selection.end - context.selection.start; }
      VolatileSelection selection_now() { return context.selection.get_all(); }
      const SelectionReference &selection() { return context.selection; }

      const Array<SelectionReference> &current_word_references() { return _current_word_references; }
      
      FrontEndReference clicked_box_id() {   return context.clicked_box_id; }
      FrontEndReference mouseover_box_id() { return context.mouseover_box_id; }
      static Expr get_current_value_of_MouseOverBox(FrontEndObject *obj, Expr item);
      void reset_mouse();
      bool is_mouse_down();
      
      virtual SharedPtr<Stylesheet> stylesheet() override { return context.stylesheet; }
      void stylesheet(SharedPtr<Stylesheet> new_stylesheet);
      bool load_stylesheet();
      virtual void reset_style() override;
      
      void paint_resize(Canvas &canvas, bool resize_only);
      
      Expr section_list_to_pmath(BoxOutputFlags flags, int start, int end) {
        return SectionList::to_pmath(flags, start, end);
      }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      virtual Expr to_pmath_symbol() override { return Symbol(PMATH_SYMBOL_DOCUMENT); }
      
      virtual Expr to_pmath_id() override;
      
      bool attach_popup_window(const SelectionReference &anchor, Document *popup_window);
      void popup_window_closed(Document *popup_window) { if(popup_window) popup_window_closed(popup_window->id()); };
      void popup_window_closed(FrontEndReference popup_window_id);
      void invalidate_popup_window_positions();
      
    public:
      Document *main_document; // not owned
      
    protected:
      Context            context;
      float              best_index_rel_x;
      int                prev_sel_line;
      FrontEndReference  prev_sel_box_id;
      int                must_resize_min;
      DragStatus         drag_status;
      bool               auto_scroll;
      
      SharedPtr<BoxRepaintEvent> flashing_cursor_circle;
      
      SelectionReference sel_first;
      SelectionReference sel_last;
      
      SelectionReference drag_source;
      
    private:
      NativeWidget *_native;
      
      /* When the selection changes, these ranges need to be repainted, too. */
      Array<SelectionReference> additional_selection;
      
      Array<SelectionReference> _current_word_references;
      Array<BoxAttchmentPopup>  _attached_popup_windows;
      
      SelectionReference        last_paint_sel;
      
      AutoCompletion auto_completion;
  };
  
  class AutoResetSelection {
    public:
      explicit AutoResetSelection(Document *_doc) : doc(_doc), old_sel(doc->selection()) {}
      AutoResetSelection(const AutoResetSelection &src) = delete;
      
      ~AutoResetSelection() {
        Box *b = old_sel.get();
        doc->select(b, old_sel.start, old_sel.end);
      }
      
    private:
      Document           *doc;
      SelectionReference  old_sel;
  };
  
  extern Hashtable<String, Expr> global_immediate_macros;
  extern Hashtable<String, Expr> global_macros;
}

#endif // RICHMATH__GUI__DOCUMENT_H__INCLUDED
