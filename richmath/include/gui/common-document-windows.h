#ifndef RICHMATH__GUI__COMMON_DOCUMENT_WINDOWS_H__INCLUDED
#define RICHMATH__GUI__COMMON_DOCUMENT_WINDOWS_H__INCLUDED

#include <util/base.h>

#include <eval/observable.h>

#include <util/pmath-extra.h>


namespace richmath {
  class CommonDocumentWindow;
  class Document;
  
  class CommonDocumentWindow: public virtual Base {
      friend class CommonDocumentWindowImpl;
    public:
      CommonDocumentWindow();
      virtual ~CommonDocumentWindow();
    
      // all windows are arranged in a ring buffer:
      CommonDocumentWindow *prev_window() { return _prev_window; }
      CommonDocumentWindow *next_window() { return _next_window; }
      
      virtual void reset_title(){ title(_title); }
      
      String filename() { return _filename; }
      void filename(String new_filename);
      String title() { return _title; }
      
      void on_idle_after_edit();
      void on_saved();
    
    protected:
      void title(String text);
      
      virtual void finish_apply_title(String displayed_title) = 0;
    
    private:
      CommonDocumentWindow *_prev_window;
      CommonDocumentWindow *_next_window;
    
    protected:
      Document                *_content;
      ObservableValue<String>  _filename;
      String                   _default_title;
      ObservableValue<String>  _title;
      bool                     _has_unsaved_changes;
    
    public:
      class Iterator {
        public:
          explicit Iterator(CommonDocumentWindow *current, bool first) 
          : _current(current), 
            _first(first) 
          { 
          }
          
          bool operator!=(const Iterator &other) const {
            return _current != other._current || _first != other._first;
          }
          const CommonDocumentWindow *operator*() const {
            return _current;
          }
          CommonDocumentWindow *operator*() {
            return _current;
          }
          const Iterator &operator++() {
            _first = false;
            if(_current) {
              _current = _current->_next_window;
            }
            return *this;
          }
          
        private:
          CommonDocumentWindow *_current;
          bool                  _first;
      };
      static class Enum {
        friend class CommonDocumentWindow;
        public:
          Enum();
          
          CommonDocumentWindow *first() { return _first; }
          int count() { return _count; }
          
          Iterator begin() {
            return Iterator(_first, _first ? true : false);
          }
          Iterator end() {
            return Iterator(_first, false);
          }
          
        private:
          CommonDocumentWindow *_first;
          int                   _count;
      } All;
  };
}

#endif // RICHMATH__GUI__COMMON_DOCUMENT_WINDOWS_H__INCLUDED
