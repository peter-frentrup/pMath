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
  
  typedef enum {
    FingerCursor   = -3,
    CurrentCursor  = -2,
    DefaultCursor  = -1,
    TextSECursor   = 101,
    TextECursor    = 102,
    TextNECursor   = 103,
    TextNCursor    = 104,
    TextNWCursor   = 105,
    TextWCursor    = 106,
    TextSWCursor   = 107,
    TextSCursor    = 108,
    SectionCursor  = 109,
    DocumentCursor = 110,
    NoSelectCursor = 111,
    SizeSECursor   = 120,
    SizeECursor    = 121,
    SizeNECursor   = 122,
    SizeNCursor    = 123,
    SizeNWCursor   = 124,
    SizeWCursor    = 125,
    SizeSWCursor   = 126,
    SizeSCursor    = 127,
  } CursorType;
  
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
      virtual ~NativeWidget();
      
      virtual void dynamic_updated() override {}
      
      virtual void window_size(float *w, float *h) = 0;
      virtual void page_size(float *w, float *h) = 0;
      
      virtual bool is_scrollable() = 0;
      virtual bool autohide_vertical_scrollbar() = 0;
      virtual void scroll_pos(float *x, float *y) = 0;
      virtual void scroll_to(float x, float y) = 0;
      virtual void scroll_by(float dx, float dy);
      
      virtual void show_tooltip(Expr boxes) = 0;
      virtual void hide_tooltip() = 0;
      
      // scale setting changes doc style Magnification
      virtual bool is_scaleable() = 0;
      void scale_by(float ds);
      void set_custom_scale(float s);
      
      virtual double message_time() = 0;
      virtual double double_click_time() = 0;
      virtual void double_click_dist(float *dx, float *dy) = 0;
      virtual void do_drag_drop(const VolatileSelection &src, MouseEvent &event) = 0;
      virtual bool cursor_position(float *x, float *y) = 0;
      virtual bool may_drop_into(const VolatileSelection &dst, bool self_is_source);
      
      virtual void bring_to_front() = 0;
      virtual void close() = 0;
      virtual void invalidate() = 0;
      virtual void invalidate_options() = 0;
      virtual void invalidate_rect(float x, float y, float w, float h) { invalidate(); }
      virtual void force_redraw() = 0;
      
      virtual void set_cursor(CursorType type) = 0;
      static CursorType text_cursor(float dx, float dy);
      static CursorType text_cursor(Box *box, int index);
      static CursorType size_cursor(float dx, float dy, CursorType base);
      static CursorType size_cursor(Box *box, CursorType base);
      
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
      
      virtual String window_title() { return String(); }
      
      virtual void on_editing();
      virtual void on_idle_after_edit();
      
      virtual void on_saved() = 0;
      
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
      void adopt(Document *doc);
      
      Context *document_context();
      
      SelectionReference &drag_source_reference();
    
    protected:
      float _custom_scale_factor;
      ObservableValue<int> _dpi;
      
    private:
      Document              *_document;
      FrontEndReference      _owner_document;
      FrontEndReference      _stylesheet_document;
      SharedPtr<TimedEvent>  _idle_after_edit;
  };
  
  static const float ScaleDefault = 1.f;
  static const float ScaleMin     = 1 / 4.f;
  static const float ScaleMax     = 32.f;
}

#endif // RICHMATH__GUI__NATIVE_WIDGET_H__INCLUDED
