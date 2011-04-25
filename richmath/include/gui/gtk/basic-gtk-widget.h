#ifndef __GUI__GTK__BASIC_GTK_WIDGET_H__
#define __GUI__GTK__BASIC_GTK_WIDGET_H__

#ifndef RICHMATH_USE_GTK_GUI
  #error this header is gtk specific
#endif

#include <gtk/gtk.h>

#include <util/base.h>

namespace richmath{
  // Must call init() immediately init after the construction of a derived object!
  class BasicGtkWidget: public virtual Base{
    protected:
      struct InitData{
      };
    
    protected:
      virtual void after_construction();
    
    public:
      BasicGtkWidget();
        
      void init(){
        after_construction();
        _initializing = false;
      }
      
      virtual ~BasicGtkWidget();
      
      bool initializing(){ return _initializing; }
      
    public:
      GtkWidget *widget(){ return _widget; }
      
      BasicGtkWidget *parent();
      static BasicGtkWidget *from_widget(GtkWidget *wid);
      
      template<class T>
      T *find_parent(){
        BasicGtkWidget *p = parent();
        while(p){
          T *t = dynamic_cast<T*>(p);
          if(t)
            return t;
          
          p = p->parent();
        }
        
        return 0;
      }
      
    protected:
      GtkWidget *_widget;
      
      virtual bool callback(GdkEvent *event);
    
    private:
      InitData *init_data;
      bool _initializing;
      
      static void destroyed_by_gobject(void *_this);
      static gboolean widget_event(GtkWidget *wid, GdkEvent *event, void *dummy);
  };
}

#endif // __GUI__GTK__BASIC_GTK_WIDGET_H__
