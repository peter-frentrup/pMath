#include <gui/common-document-windows.h>

#include <gui/document.h>
#include <gui/native-widget.h>

#include <eval/application.h>

#include <util/filesystem.h>


using namespace richmath;

namespace richmath {
  class CommonDocumentWindow::Impl {
    public:
      Impl(CommonDocumentWindow &_self) : self(_self) {}
      
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
  
  All.notify_all();
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
  
  All.notify_all();
}

void CommonDocumentWindow::directory(String new_directory) {
  _directory = std::move(new_directory);
}

void CommonDocumentWindow::filename(String new_filename) {
  _filename = new_filename;
  _default_title = std::move(new_filename);
  
  reset_title();
}

String CommonDocumentWindow::full_filename() {
  return FileSystem::file_name_join(_directory, _filename);
}

void CommonDocumentWindow::full_filename(String new_full_filename) {
  if(new_full_filename.is_null()) {
    directory(String());
    filename(String());
  }
  
  String dir = FileSystem::extract_directory_path(&new_full_filename);
  directory(std::move(dir));
  filename(std::move(new_full_filename));
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
  
  if(Document *style_doc = content()->native()->stylesheet_document())
    style_doc->native()->on_saved();
}

void CommonDocumentWindow::title(String text) {
  if(text.is_null()) {
    if(_default_title.is_null()) {
      for(int i = 1;;++i) {
        _default_title = String("Untitled ") + Expr(i).to_string();
        bool already_exists = false;
        
        for(auto win : CommonDocumentWindow::All) {
          if(win == this)
            continue;
          
          if(win->_default_title == _default_title || win->_title.unobserved_equals(_default_title)) {
            already_exists = true;
            break;
          }
        }
        
        if(!already_exists)
          break;
      }
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
