#ifndef RICHMATH__GUI__NATIVE_WIDGET_H__INCLUDED
#define RICHMATH__GUI__NATIVE_WIDGET_H__INCLUDED

#include <eval/observable.h>

#include <gui/control-painter.h>
#include <util/selections.h>


namespace richmath {
  class Context;
  class Document;
  class MouseEvent;
  class TimedEvent;
  
  enum class CursorType {
    Finger   = -3,
    Current  = -2,
    Default  = -1,
    TextSE   = 101,
    TextE    = 102,
    TextNE   = 103,
    TextN    = 104,
    TextNW   = 105,
    TextW    = 106,
    TextSW   = 107,
    TextS    = 108,
    Section  = 109,
    Document = 110,
    NoSelect = 111,
    SizeSE   = 120,
    SizeE    = 121,
    SizeNE   = 122,
    SizeN    = 123,
    SizeNW   = 124,
    SizeW    = 125,
    SizeSW   = 126,
    SizeS    = 127,
  };
  
  template<>
  struct default_hash_impl<CursorType> {
    static unsigned int hash(CursorType t) {
      return (unsigned int)t;
    }
  };
  
  class NativeWidget: public virtual FrontEndObject, public virtual ControlContext {
      friend class NativeWidgetImpl;
    public:
      explicit NativeWidget(Document *doc);
      
      virtual void dynamic_updated() override {}
      
      virtual Vector2F window_size() = 0;
      virtual Vector2F page_size() = 0;
      
      virtual bool is_scrollable() = 0;
      virtual bool autohide_vertical_scrollbar() = 0;
      virtual Point scroll_pos() = 0;
      virtual void scroll_to(Point pos) = 0;
      void scroll_by(Vector2F delta) { scroll_to(scroll_pos() + delta); }
      void scroll_by(float dx, float dy) { scroll_by({dx, dy}); }
      
      virtual void show_tooltip(Box *source, Expr boxes) = 0;
      virtual void hide_tooltip() = 0;
      
      /* May return nullptr (no gui or floating popup windows available ...)
         The document will not be visible, call its invalidate_options() to
         recognize the "Visible" style option.
      */
      virtual Document *try_create_popup_window(const SelectionReference &anchor) { return nullptr; }
      
      // scale setting changes doc style Magnification
      virtual bool is_scaleable() = 0;
      void scale_by(float ds);
      void set_custom_scale(float s);
      
      virtual double message_time() = 0;
      virtual double double_click_time() = 0;
      virtual Vector2F double_click_dist() = 0;
      virtual void do_drag_drop(const VolatileSelection &src, MouseEvent &event) = 0;
      virtual bool may_drop_into(const VolatileSelection &dst, bool self_is_source);
      
      virtual void bring_to_front() = 0;
      virtual void close() = 0;
      virtual void invalidate() = 0;
      virtual void invalidate_options() = 0;
      virtual void invalidate_rect(const RectangleF &rect) { invalidate(); }
      virtual void force_redraw() = 0;
      
      virtual void set_cursor(CursorType type) = 0;
      static CursorType text_cursor(Vector2F dir);
      static CursorType text_cursor(Box *box, int index);
      static CursorType size_cursor(Vector2F dir, CursorType base);
      static CursorType size_cursor(Box *box,     CursorType base);
      
      virtual void running_state_changed() = 0;
      
      virtual bool is_mouse_down() = 0;
      virtual void beep() = 0;
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) = 0;
      
      virtual String directory() = 0;
      virtual void directory(String new_directory) = 0;
      
      virtual String filename() = 0;
      virtual void filename(String new_filename) = 0;
      
      virtual String full_filename() = 0;
      virtual void full_filename(String new_full_filename) = 0;
      
      virtual bool can_toggle_menubar() { return false; }
      virtual bool has_menubar() { return false; }
      virtual bool try_set_menubar(bool visible) { return false; }
      virtual String window_title() { return String(); }
      
      virtual void on_editing();
      virtual void on_idle_after_edit();
      
      virtual void on_saved() = 0;
      
      Box *source_box();
      bool source_box(Box *box);
      
      Document *owner_document();
      Document *stylesheet_document();
      bool stylesheet_document(Document *doc);
      
      virtual Document *working_area_document() { return nullptr; }
      Document *document() { return _document; }
      float custom_scale_factor() { return _custom_scale_factor; }
      float scale_factor() {        return _dpi * _custom_scale_factor / 72; }
      
      // ControlContext functions:
      virtual int dpi() override { return _dpi; }
      
    public:
      static NativeWidget *dummy;
      
    protected:
      virtual ~NativeWidget();
      void adopt(Document *doc);
      
      bool owner_document(Document *owner);
      bool owner_document(FrontEndReference owner);
      
      Context *document_context();
      
      SelectionReference &drag_source_reference();
    
    protected:
      float _custom_scale_factor;
      ObservableValue<int> _dpi;
      
    private:
      Document                           *_document;
      ObservableValue<FrontEndReference>  _source_box;
      FrontEndReference                   _owner_document;
      FrontEndReference                   _stylesheet_document;
      SharedPtr<TimedEvent>               _idle_after_edit;
  };
  
  static const float ScaleDefault = 1.f;
  static const float ScaleMin     = 1 / 4.f;
  static const float ScaleMax     = 32.f;
}

#endif // RICHMATH__GUI__NATIVE_WIDGET_H__INCLUDED
