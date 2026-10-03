#include <gui/fltk/mfltk-document-window.h>

#include <FL/FL.H>
#include <FL/FL_Scrollbar.H>
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
    
    public:
      virtual Fl_Scrollbar *vscroll() override { return _vscroll; }
      virtual Fl_Scrollbar *hscroll() override { return _hscroll; }
      void vscroll(Fl_Scrollbar *sb) { _vscroll = sb; }
      void hscroll(Fl_Scrollbar *sb) { _hscroll = sb; }
      
      static void static_scrolled_cb(Fl_Widget *sender, void *_self);
      
    protected:
      virtual void paint_canvas(Canvas &canvas, bool resize_only) override;
      
      void adjust_scollbars();
    
    protected:
      int best_width  = 1;
      int best_height = 1;
      int outer_width  = 1;
      int outer_height = 1;
      
    private:
      Fl_Scrollbar *_vscroll = nullptr;
      Fl_Scrollbar *_hscroll = nullptr;
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
  Fl_Window *win;
  if(!_widget) {
    win = new Fl_Window(500, 530, nullptr);
    win->end();
    _widget = win;
    win->box(FL_NO_BOX);
  }
  else {
    win = window();
  }
  
  BasicFltkWidget::after_construction();
  
  _working_area->init(); // calls win->add(_widget)
  
  int SW = Fl::scrollbar_size();
  Fl_Scrollbar *vscroll = new Fl_Scrollbar(win->w() - SW, 0, SW, win->h());
  win->add(vscroll);
  
  vscroll->callback(MathFltkWorkingArea::static_scrolled_cb, _working_area);
  
  _working_area->vscroll(vscroll);
  _working_area->widget()->resize(0, 0, win->w() - SW, win->h());
  win->resizable(_working_area->widget());
  
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

void MathFltkWorkingArea::static_scrolled_cb(Fl_Widget *sender, void *_self) {
  MathFltkWorkingArea *self = (MathFltkWorkingArea*)_self;
  self->invalidate();
}

void MathFltkWorkingArea::paint_canvas(Canvas &canvas, bool resize_only) {
  base::paint_canvas(canvas, resize_only);
  
  int old_bh = best_height;
  int old_bw = best_width;
  
  int old_ow = outer_width;
  int old_oh = outer_height;
  
  if(_widget->parent()) {
    outer_width  = _widget->parent()->w();
    outer_height = _widget->parent()->h();
  }
  
  best_height = (int)ceilf(document()->extents().height() * scale_factor());
  best_width  = (int)ceilf(document()->unfilled_width     * scale_factor());
  
  if(best_height < 1)
    best_height = 1;
    
  if(best_width < 1)
    best_width = 1;
  
  if(old_bw != best_width || old_bh != best_height || old_ow != outer_width || old_oh != outer_height) {
    adjust_scollbars();
  }
}

void MathFltkWorkingArea::adjust_scollbars() {
  struct { int pos, page, max; } v_range, h_range;
  v_range.pos  = _vscroll ? _vscroll->value() : 0;
  v_range.page = outer_height;
  v_range.max  = best_height;
  if(!autohide_vertical_scrollbar())
    v_range.max += (outer_height * 4) / 5;
  
  pmath_debug_print("[vscroll 0 .. %d, page %d ]\n", v_range.max, v_range.page);
  
  h_range.pos  = _hscroll ? _hscroll->value() : 0;
  h_range.page = outer_width;
  h_range.max  = best_width;
  
  bool show_vscroll = _vscroll && v_range.max > 0;
  if(show_vscroll) {
    h_range.page -= Fl::scrollbar_size();
  }
  
  bool show_hscroll = _hscroll && v_range.max > 0;
  if(show_hscroll) {
    v_range.page -= Fl::scrollbar_size();
  }
  
  if(v_range.max  < 0)           v_range.max = 0;
  if(v_range.page > v_range.max) v_range.page = v_range.max;
  if(v_range.pos  > v_range.max) v_range.pos  = v_range.max;
  
  if(h_range.max  < 0)           h_range.max = 0;
  if(h_range.page > h_range.max) h_range.page = h_range.max;
  if(h_range.pos  > h_range.max) h_range.pos  = h_range.max;
  
  _widget->size(
    outer_width  - (show_vscroll ? Fl::scrollbar_size() : 0),
    outer_height - (show_hscroll ? Fl::scrollbar_size() : 0));
  
  if(_vscroll) {
    _vscroll->value(v_range.pos, v_range.page, 0, v_range.max);
    if(show_vscroll) {
      _vscroll->resize(
        _widget->x() + _widget->w(),
        _widget->y(),
        Fl::scrollbar_size(),
        _widget->h());
      _vscroll->show();
    }
    else {
      _vscroll->hide();
    }
  }
  
  if(_hscroll) {
    _hscroll->value(h_range.pos, h_range.page, 0, h_range.max);
    if(show_hscroll) {
      _vscroll->resize(
        _widget->x(),
        _widget->y() + _widget->h(),
        _widget->w(),
        Fl::scrollbar_size());
      _hscroll->show();
    }
    else {
      _hscroll->hide();
    }
  }
}

//} ... class MathFltkWorkingArea
