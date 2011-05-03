#ifndef __GUI__NATIVE_WIDGET_H__
#define __GUI__NATIVE_WIDGET_H__

#include <util/base.h>

#include <gui/control-painter.h>

namespace richmath{
  class Box;
  class Context;
  class Document;
  class SelectionReference;
  class TimedEvent;
  
  typedef enum{
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
    NoSelectCursor = 111
  }CursorType;
  
  class NativeWidget: public virtual Base{
    public:
      explicit NativeWidget(Document *doc);
      virtual ~NativeWidget();
      
      virtual void window_size(float *w, float *h) = 0;
      virtual void page_size(float *w, float *h) = 0;
      
      virtual bool is_scrollable() = 0;
      virtual bool autohide_vertical_scrollbar() = 0;
      virtual void scroll_pos(float *x, float *y) = 0;
      virtual void scroll_to(float x, float y) = 0;
      virtual void scroll_by(float dx, float dy);
      
      virtual void show_tooltip(Expr boxes) = 0;
      virtual void hide_tooltip() = 0;
      
      virtual bool is_scaleable() = 0;
      void scale_by(float ds);
      void set_scale(float s);
      
      virtual double message_time() = 0;
      virtual double double_click_time() = 0;
      virtual void double_click_dist(float *dx, float *dy) = 0;
      virtual void do_drag_drop(Box *src, int start, int end) = 0;
      virtual bool cursor_position(float *x, float *y) = 0;
      virtual bool may_drop_into(Box* dst, int strt, int end, bool self_is_source);
      
      virtual void close() = 0;
      virtual void invalidate() = 0;
      virtual void force_redraw() = 0;
      
      virtual void set_cursor(CursorType type) = 0;
      static CursorType text_cursor(float dx, float dy);
      static CursorType text_cursor(Box *box, int index);
      
      virtual void running_state_changed() = 0;
      
      virtual bool is_mouse_down() = 0;
      virtual void beep() = 0;
      
      virtual bool register_timed_event(SharedPtr<TimedEvent> event) = 0;
      
      Document *document(){ return _document; }
      float scale_factor(){ return _scale_factor; }
      
    public:
      static NativeWidget *dummy;
      
    protected:
      void adopt(Document *doc);
      
      Context *document_context();
      
      SelectionReference &drag_source_reference();
    
    protected:
      float _scale_factor;
    
    private:
      Document *_document;
  };

  static const float ScaleDefault = 4/3.f;
  static const float ScaleMin     = 1/4.f;
  static const float ScaleMax     = 32.f;
}

#endif // __GUI__NATIVE_WIDGET_H__
