#ifndef RICHMATH__GUI__GTK__BASIC_GTK_WIDGET_H__INCLUDED
#define RICHMATH__GUI__GTK__BASIC_GTK_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_GTK_GUI
#  error this header is gtk specific
#endif

#include <util/base.h>
#include <boxes/box.h>

#include <pmath-cpp.h>
#include <gtk/gtk.h>


namespace richmath {
  // Must call init() immediately init after the construction of a derived object!
  class BasicGtkWidget: public virtual Base {
    protected:
      virtual void after_construction();
      
    public:
      BasicGtkWidget();
      
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
      
      virtual ~BasicGtkWidget();
      
      bool initializing() { return _initializing; }
      bool destroying() {   return _destroying;   }
      
    public:
      GtkWidget *widget() { return _widget; }
      
      static BasicGtkWidget *from_widget(GtkWidget *wid);
      
      template<typename Func>
      static void container_foreach(GtkContainer *container, Func func) {
        gtk_container_foreach(container, [](GtkWidget *child, void *_func) { (*(Func*)_func)(child); }, &func);
      }
      
      template<typename Func>
      static void internal_forall_recursive(GtkWidget *widget, Func func) {
        func(widget);
        if(GTK_IS_CONTAINER(widget)) {
          gtk_container_forall(
            GTK_CONTAINER(widget),
            [](GtkWidget *child, void *func_ptr) {
              internal_forall_recursive(child, *(Func*)func_ptr);
            },
            &func);
        }
      }
      
    protected:
      GtkWidget *_widget;
      
    protected:
      virtual bool on_event(GdkEvent *e);
      
    protected:
      template<class C, typename A, bool (C::*method)(A)>
      struct Marshaller {
        static gboolean function(GtkWidget *wid, A arg, void *dummy) {
          AutoMemorySuspension ams;
          C *_this = static_cast<C*>(BasicGtkWidget::from_widget(wid));
          if(_this)
            return (_this->*method)(arg);
          return TRUE;
        }
      };
      
      template<class C, bool (C::*method)()>
      struct Marshaller0 {
        static gboolean function(GtkWidget *wid, void *dummy) {
          AutoMemorySuspension ams;
          C *_this = static_cast<C*>(BasicGtkWidget::from_widget(wid));
          if(_this)
            return (_this->*method)();
          return TRUE;
        }
      };
      
      template<class C, typename A, bool (C::*method)(A)>
      void signal_connect(const char *name) {
        g_signal_connect(_widget, name, G_CALLBACK((Marshaller<C, A, method>::function)), nullptr);
      }
      
      template<class C, bool (C::*method)()>
      void signal_connect(const char *name) {
        g_signal_connect(_widget, name, G_CALLBACK((Marshaller0<C, method>::function)), nullptr);
      }
      
    private:
      bool _initializing;
      bool _destroying;
      
      static void destroy_widget_key_callback(BasicGtkWidget *_this);
  };
}

#endif // RICHMATH__GUI__GTK__BASIC_GTK_WIDGET_H__INCLUDED
