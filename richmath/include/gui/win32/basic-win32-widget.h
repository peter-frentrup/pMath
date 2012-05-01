#ifndef __GUI__WIN32__BASIC_WIN32_WIDGET_H__
#define __GUI__WIN32__BASIC_WIN32_WIDGET_H__

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>
#include <ole2.h>

#include <util/base.h>

namespace richmath {
  // Must call init() immediately init after the construction of a derived object!
  class BasicWin32Widget: public IDropTarget, public virtual Base {
      struct InitData {
        DWORD style_ex;
        DWORD style;
        int x;
        int y;
        int width;
        int height;
        HWND *parent;
        const wchar_t *window_class_name;
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
        
      void set_window_class_name(const wchar_t *static_name);
      void init() {
        after_construction();
        _initializing = false;
      }
      
      virtual ~BasicWin32Widget();
      
      bool initializing() { return _initializing; }
      
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject);
      STDMETHODIMP_(ULONG) AddRef(void);
      STDMETHODIMP_(ULONG) Release(void);
      
      //
      // IDropTarget members
      //
      STDMETHODIMP DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect);
      STDMETHODIMP DragOver(DWORD key_state, POINTL pt, DWORD *effect);
      STDMETHODIMP DragLeave(void);
      STDMETHODIMP Drop(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect);
      
    public:
      HWND &hwnd() { return _hwnd; }
      
      BasicWin32Widget *parent();
      static BasicWin32Widget *from_hwnd(HWND hwnd);
      
      template<class T>
      T *find_parent() {
        BasicWin32Widget *p = parent();
        while(p) {
          T *t = dynamic_cast<T*>(p);
          if(t)
            return t;
            
          p = p->parent();
        }
        
        return 0;
      }
      
    protected:
      HWND _hwnd;
      bool _allow_drop;
      bool _is_dragging_over;
      
    protected:
      virtual LRESULT callback(UINT message, WPARAM wParam, LPARAM lParam);
      
      virtual bool is_data_droppable(IDataObject *data_object);
      virtual DWORD drop_effect(DWORD key_state, POINTL pt, DWORD allowed_effects);
      virtual void do_drop_data(IDataObject *data_object, DWORD effect);
      virtual void position_drop_cursor(POINTL pt);
      
      static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
      
    private:
      InitData *init_data;
      bool _initializing;
      
      static void init_window_class();
  };
}

#endif // __GUI__WIN32__BASIC_WIN32_WIDGET_H__
