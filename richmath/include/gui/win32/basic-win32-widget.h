#ifndef __GUI__WIN32__BASIC_WIN32_WIDGET_H__
#define __GUI__WIN32__BASIC_WIN32_WIDGET_H__

#include <windows.h>

#include <util/base.h>

namespace richmath{
  // Must call init() immediately init after the construction of a derived object!
  class BasicWin32Widget: public virtual Base{
      struct InitData{
        DWORD style_ex;
        DWORD style;
        int x;
        int y; 
        int width;
        int height;
        HWND *parent;
      };
    
    protected:
      virtual void after_construction();
      
    public:
      BasicWin32Widget(
        DWORD style_ex, 
        DWORD style, 
        int x, 
        int y, 
        int width,
        int height,
        HWND *parent);
        
      void init(){
        after_construction();
        _initializing = false;
      }
      
      virtual ~BasicWin32Widget();
      
      bool initializing(){ return _initializing; }
      
    public:
      HWND &hwnd(){ return _hwnd; }
      
      BasicWin32Widget *parent();
      static BasicWin32Widget *from_hwnd(HWND hwnd);
      
      template<class T>
      T *find_parent(){
        BasicWin32Widget *p = parent();
        while(p){
          T *t = dynamic_cast<T*>(p);
          if(t)
            return t;
          
          p = p->parent();
        }
        
        return 0;
      }
      
    protected:
      HWND _hwnd;
    
    protected:
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
    
    private:
      bool _initializing;
      InitData *init_data;
      
      static void init_window_class();
      static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
  };
}

#endif // __GUI__WIN32__BASIC_WIN32_WIDGET_H__
