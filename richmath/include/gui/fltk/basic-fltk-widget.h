#ifndef RICHMATH__GUI__FLTK__BASIC_FLTK_WIDGET_H__INCLUDED
#define RICHMATH__GUI__FLTK__BASIC_FLTK_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_FLTK_GUI
#  error This header is FLTK specific.
#endif

#include <util/base.h>

class Fl_Widget;

namespace richmath {
  // Must call init() immediately init after the construction of a derived object!
  class BasicFltkWidget: public Base {
      class Connector;
    protected:
      virtual void after_construction();
      
    public:
      BasicFltkWidget();
      
      void init() {
        after_construction();
        _initializing = false;
      }
      
      void destroy() {
        if(!_destroying) {
          _destroying = true;
          delete this;
        }
      }
      
      virtual ~BasicFltkWidget();
      
      bool initializing() { return _initializing; }
      bool destroying() {   return _destroying;   }
      
    public:
      Fl_Widget *widget() { return _widget; }
      
      static BasicFltkWidget *from_widget(Fl_Widget *wid);
      
    protected:
      Fl_Widget *_widget;
      
      virtual void on_callback(Fl_Widget *sender);
      
    private:
      bool _initializing;
      bool _destroying;
  };
}

#endif // RICHMATH__GUI__FLTK__BASIC_FLTK_WIDGET_H__INCLUDED
