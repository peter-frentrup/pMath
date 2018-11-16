#include <gui/common-document-windows.h>

#include <gui/document.h>

#include <eval/application.h>


using namespace richmath;

namespace richmath {
  class CommonDocumentWindowImpl {
    public:
      CommonDocumentWindowImpl(CommonDocumentWindow &_self) : self(_self) {}
      
    private:
      CommonDocumentWindow &self;
  };
}

//{ class CommonDocumentWindow::Enum ...

CommonDocumentWindow::Enum CommonDocumentWindow::All{};

CommonDocumentWindow::Enum::Enum()
  : _first(nullptr),
    _count(0)
{
}

//} ... class CommonDocumentWindow::All

//{ class CommonDocumentWindow ...

CommonDocumentWindow::CommonDocumentWindow()
  : Base(),
    _content(nullptr),
    _has_unsaved_changes(false)
{
  ++All._count;
  
  if(All._first) {
    _prev_window = All._first->_prev_window;
    _prev_window->_next_window = this;
    _next_window = All._first;
    All._first->_prev_window = this;
  }
  else {
    All._first = this;
    _prev_window = this;
    _next_window = this;
  }
}

CommonDocumentWindow::~CommonDocumentWindow() {
  --All._count;
  
  if(All._first == this) {
    All._first = _next_window;
    if(All._first == this)
      All._first = nullptr;
  }

  _next_window->_prev_window = _prev_window;
  _prev_window->_next_window = _next_window;
}

void CommonDocumentWindow::filename(String new_filename) {
  _filename = new_filename;
  if(new_filename.is_valid()) {
    int c = new_filename.length();
    const uint16_t *buf = new_filename.buffer();
    while(c >= 0 && buf[c] != '\\' && buf[c] != '/')
      --c;
      
    _default_title = new_filename.part(c + 1);
  }
  reset_title();
}

void CommonDocumentWindow::on_idle_after_edit() {
  if(!_has_unsaved_changes) {
    _has_unsaved_changes = true;
    reset_title();
  }
}

void CommonDocumentWindow::on_saved() {
  if(_has_unsaved_changes) {
    _has_unsaved_changes = false;
    reset_title();
  }
}

void CommonDocumentWindow::title(String text) {
  if(text.is_null()) {
    if(_default_title.is_null()) {
      _default_title = "untitled";
    }
    text = _default_title;
  }
  
  _title = text;
  
  if(_has_unsaved_changes)
    text = String("*") + text;
  
  if(_content && Application::is_running_job_for(_content))
    text = String("Running... ") + text;
  
  finish_apply_title(std::move(text));
  //String tmp = text + String::FromChar(0);
  //
  //SetWindowTextW(_hwnd, (const WCHAR *)tmp.buffer());
}

//} ... class CommonDocumentWindow
