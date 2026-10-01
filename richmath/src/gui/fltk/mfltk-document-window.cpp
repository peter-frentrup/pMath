#include <gui/fltk/mfltk-document-window.h>

#include <FL/FL.H>
#include <FL/FL_Window.H>

#include <gui/document.h>
#include <gui/documents.h>


namespace richmath {
  class MathFltkDocumentChildWidget: public MathFltkWidget {
      using base = MathFltkWidget;
    protected:
      virtual void after_construction() override;
    public:
      explicit MathFltkDocumentChildWidget(MathFltkDocumentWindow *parent);
      virtual ~MathFltkDocumentChildWidget();
    
      MathFltkDocumentWindow *parent() { return _parent; }
    
      virtual void running_state_changed() override { _parent->reset_title(); }
      
      virtual String directory() override { return _parent->directory(); }
      virtual void directory(String new_directory) override { _parent->directory(new_directory); }
      
      virtual String filename() override { return _parent->filename(); }
      virtual void filename(String new_filename) override { _parent->filename(new_filename); }
      
      virtual String full_filename() override { return _parent->full_filename(); }
      virtual void full_filename(String new_full_filename) override { _parent->full_filename(new_full_filename); }
      
      virtual String window_title() override { return _parent->title(); }
      
      virtual bool is_foreground_window() override { return _parent->is_foreground_window(); }
      virtual bool is_focused_widget() override { return _parent->is_foreground_window() && base::is_focused_widget(); }
      virtual bool is_using_dark_mode() override { return _parent->is_using_dark_mode(); }
      
      virtual void invalidate_options() override;
      
    protected:
      virtual void do_set_selected_document() override;
      
    private:
      MathFltkDocumentWindow *_parent;
      
  }; 
  
  class MathFltkWorkingArea : public MathFltkDocumentChildWidget {
      using base = MathFltkDocumentChildWidget;
    public:
      explicit MathFltkWorkingArea(MathFltkDocumentWindow *parent);
      virtual ~MathFltkWorkingArea();
  };
}

using namespace richmath;

//{ class MathFltkDocumentWindow ...

MathFltkDocumentWindow::MathFltkDocumentWindow()
: BasicFltkWidget()
{
  SET_EXPLICIT_BASE_DEBUG_TAG(CommonDocumentWindow, typeid(*this).name());
  SET_EXPLICIT_BASE_DEBUG_TAG(BasicFltkWidget,      typeid(*this).name());
  
  _working_area = new MathFltkWorkingArea(this);
  
  _content = _working_area->document();
}

void MathFltkDocumentWindow::after_construction() {
  if(!_widget) {
    Fl_Window *win = new Fl_Window(500, 530, nullptr);
    win->end();
    _widget = win;
    win->box(FL_NO_BOX);
  }
  
  BasicFltkWidget::after_construction();
  
  working_area()->init();
  working_area()->widget()->resize(0, 0, 500, 530);
  
  window()->resizable(working_area()->widget());
  
  title(String());
  
  document()->style.set(Visible,                         true);
  document()->style.set(InternalHasModifiedWindowOption, true);
}

MathFltkDocumentWindow::~MathFltkDocumentWindow() {
}

int MathFltkDocumentWindow::dpi() {
  float h = 0, w = 0;
  
  if(Fl_Window *win = window()) {
    int screen_num = win->visible_r() ? win->screen_num() : 0;
    
    Fl::screen_dpi(h, w, screen_num);
    
    if((int)h > 0)
      return (int)h;
  }
  
  Fl::screen_dpi(h, w, 0);
  if((int)h > 0)
    return (int)h;
  
  return 96;
}

void MathFltkDocumentWindow::invalidate_options() {
  Document *doc = document();
  
  if(doc->load_stylesheet()) {
    // TODO invalidate top docked areas
  }
  
  String s = doc->get_style(WindowTitle, _default_title);
  if(!_title.unobserved_equals(s))
    title(s);
  
  if(Fl_Window *win = window()) {
    if(doc->get_style(Visible, true)) {
      if(!win->shown())
        win->show();
    }
    else {
      if(win->shown())
        win->hide();
    }
  }
}

void MathFltkDocumentWindow::reset_title() {
  title(document()->get_style(WindowTitle, _default_title));
}

void MathFltkDocumentWindow::finish_apply_title(String displayed_title) {
  if(!window())
    return;
  
  // TODO: avoid double copying and retain 'str' instead
  char *str = pmath_string_to_utf8(displayed_title.get(), nullptr);
  if(str)
    window()->copy_label(str);
    
  pmath_mem_free(str);
}

void MathFltkDocumentWindow::on_callback(Fl_Widget *sender) {
  if(Fl::callback_reason() == FL_REASON_CLOSED) {
    destroy();
    return;
  }
}

//} ... class MathFltkDocumentWindow

//{ class MathFltkDocumentChildWidget ...

MathFltkDocumentChildWidget::MathFltkDocumentChildWidget(MathFltkDocumentWindow *parent)
: base(new Document()),
  _parent(parent)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

MathFltkDocumentChildWidget::~MathFltkDocumentChildWidget() {
}

void MathFltkDocumentChildWidget::after_construction() {
  base::after_construction();
  
  parent()->window()->add(_widget);
  _widget->show();
}

void MathFltkDocumentChildWidget::invalidate_options() {
  base::invalidate_options();
  if(_parent)
    _parent->invalidate_options();
}

void MathFltkDocumentChildWidget::do_set_selected_document() {
  Documents::selected_document(_parent->document());
}

//} ... class MathFltkDocumentChildWidget

//{ class MathFltkWorkingArea ...

MathFltkWorkingArea::MathFltkWorkingArea(MathFltkDocumentWindow *parent)
: base(parent)
{
  SET_BASE_DEBUG_TAG(typeid(*this).name());
}

MathFltkWorkingArea::~MathFltkWorkingArea() {
}

//} ... class MathFltkWorkingArea
