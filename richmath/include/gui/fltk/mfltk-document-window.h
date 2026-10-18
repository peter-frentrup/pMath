#ifndef RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED
#define RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <eval/observable.h>

#include <gui/fltk/mfltk-widget.h>
#include <gui/common-document-windows.h>

#include <util/style.h>


class Fl_Group;
class Fl_Menu_Bar;
class Fl_Window;

namespace richmath {
  class MathFltkWorkingArea;
  
  class MathFltkDocumentWindow final : public CommonDocumentWindow, public BasicFltkWidget, public ControlContext {
    protected:
      virtual void after_construction() override;
      
    public:
      MathFltkDocumentWindow();
      virtual ~MathFltkDocumentWindow();
      
      Fl_Window *window() { return (Fl_Window*)BasicFltkWidget::widget(); }
    
      MathFltkWidget *working_area() { return (MathFltkWidget*)_working_area; }
      
      Document *document() { return working_area()->document(); }
      
      virtual bool is_foreground_window() override { return _active; }
      virtual bool is_focused_widget() override { return _active; }
      virtual bool is_using_dark_mode() override { return false; }
      virtual int dpi() override;
      
      bool can_toggle_menubar();
      bool has_menubar();
      bool try_set_menubar(bool visible);
      
      bool            is_palette() {   return _window_frame == WindowFramePalette; }
      WindowFrameType window_frame() { return _window_frame; }
      
      void invalidate_options();
      void rearrange();
      virtual void reset_title() override;
      
    protected:
      virtual void finish_apply_title(String displayed_title) override;
      
      void window_frame(WindowFrameType type);
      
      virtual void on_callback(Fl_Widget *sender) override;
      virtual void on_close();
      
    private:
      Fl_Menu_Bar           *_menu;
      Fl_Group              *_working_area_group;
      MathFltkWorkingArea   *_working_area;
      ObservableValue<bool>  _active;
      WindowFrameType        _window_frame;
  };
}

#endif // RICHMATH__GUI__FLTK__MFLTK_DOCUMENT_WINDOW_H__INCLUDED
