#ifndef __GUI__GTK__BASIC_GTK_WIDGET_H__
#define __GUI__GTK__BASIC_GTK_WIDGET_H__

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
        _destroying = true;
        delete this;
      }
      
      virtual ~BasicGtkWidget();
      
      bool initializing() { return _initializing; }
      bool destroying() {   return _destroying;   }
      
    public:
      GtkWidget *widget() { return _widget; }
      
      BasicGtkWidget *parent();
      static BasicGtkWidget *from_widget(GtkWidget *wid);
      
      template<class T>
      T *find_parent() {
        BasicGtkWidget *p = parent();
        while(p) {
          T *t = dynamic_cast<T*>(p);
          if(t)
            return t;
            
          p = p->parent();
        }
        
        return 0;
      }
      
    protected:
      GtkWidget *_widget;
      
    protected:
      virtual bool on_event(GdkEvent *e);
      
    protected:
      template<class C, typename A, bool (C::*method)(A)>
      struct Marshaller {
        static gboolean function(GtkWidget *wid, A arg, void *dummy) {
//          if(event->type != GDK_EXPOSE
//          && event->type != GDK_MOTION_NOTIFY
//          && event->type != GDK_ENTER_NOTIFY
//          && event->type != GDK_LEAVE_NOTIFY
//          && event->type != GDK_FOCUS_CHANGE)
//            pmath_debug_print("[%s %p] event %d\n", G_OBJECT_TYPE_NAME(wid), wid, event->type);

          AutoMemorySuspension ams;
          C *_this = (C*)BasicGtkWidget::from_widget(wid);
          if(_this)
            return (_this->*method)(arg);
          return TRUE;
        }
      };
      
      template<class C, bool (C::*method)()>
      struct Marshaller0 {
        static gboolean function(GtkWidget *wid, void *dummy) {
//          if(event->type != GDK_EXPOSE
//          && event->type != GDK_MOTION_NOTIFY
//          && event->type != GDK_ENTER_NOTIFY
//          && event->type != GDK_LEAVE_NOTIFY
//          && event->type != GDK_FOCUS_CHANGE)
//            pmath_debug_print("[%s %p] event %d\n", G_OBJECT_TYPE_NAME(wid), wid, event->type);

          AutoMemorySuspension ams;
          C *_this = (C*)BasicGtkWidget::from_widget(wid);
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

#endif // __GUI__GTK__BASIC_GTK_WIDGET_H__
