#ifndef RICHMATH__GUI__WIN32__BASIC_WIN32_WIDGET_H__INCLUDED
#define RICHMATH__GUI__WIN32__BASIC_WIN32_WIDGET_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>
#include <ole2.h>
#include <rtscom.h>

#include <util/base.h>

namespace richmath {

  class Win32KeepAliveWindow {
    public:
      Win32KeepAliveWindow();
      ~Win32KeepAliveWindow();
  };
  
  // Must call init() immediately init after the construction of a derived object!
  class BasicWin32Widget: public IDropTarget, public IStylusAsyncPlugin, public virtual Base {
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
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IDropTarget members
      //
      STDMETHODIMP DragEnter(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragOver(DWORD key_state, POINTL pt, DWORD *effect) override;
      STDMETHODIMP DragLeave(void) override;
      STDMETHODIMP Drop(IDataObject *data_object, DWORD key_state, POINTL pt, DWORD *effect) override;
      
      //
      // IStylusSyncPlugin members
      //
      STDMETHODIMP RealTimeStylusEnabled(IRealTimeStylus*, ULONG, const TABLET_CONTEXT_ID*) override { return S_OK; }
      STDMETHODIMP RealTimeStylusDisabled(IRealTimeStylus*, ULONG, const TABLET_CONTEXT_ID*) override { return S_OK; }
      STDMETHODIMP StylusInRange(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID) override { return S_OK; }
      STDMETHODIMP StylusOutOfRange(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID) override { return S_OK; }
      STDMETHODIMP StylusDown(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override { return S_OK; }
      STDMETHODIMP StylusUp(IRealTimeStylus*, const StylusInfo*, ULONG, LONG*, LONG**) override { return S_OK; }
      STDMETHODIMP StylusButtonUp(IRealTimeStylus*, STYLUS_ID, const GUID*, POINT*) override { return S_OK; }
      STDMETHODIMP StylusButtonDown(IRealTimeStylus*, STYLUS_ID, const GUID*, POINT*) override { return S_OK; }
      STDMETHODIMP InAirPackets(IRealTimeStylus*, const StylusInfo*, ULONG, ULONG, LONG*, ULONG*, LONG**) override { return S_OK; }
      STDMETHODIMP Packets(IRealTimeStylus* piSrcRtp, const StylusInfo* pStylusInfo, ULONG cPktCount, ULONG cPktBuffLength, LONG* pPackets, ULONG* pcInOutPkts, LONG** ppInOutPkts) override { return S_OK; }
      STDMETHODIMP SystemEvent(IRealTimeStylus*, TABLET_CONTEXT_ID, STYLUS_ID, SYSTEM_EVENT, SYSTEM_EVENT_DATA) override { return S_OK; }
      STDMETHODIMP TabletAdded(IRealTimeStylus*, IInkTablet*) override { return S_OK; }
      STDMETHODIMP TabletRemoved(IRealTimeStylus*, LONG) override { return S_OK; }
      STDMETHODIMP CustomStylusDataAdded(IRealTimeStylus*, const GUID*, ULONG, const BYTE*) override { return S_OK; }
      STDMETHODIMP Error(IRealTimeStylus*, IStylusPlugin*, RealTimeStylusDataInterest, HRESULT, LONG_PTR*) override { return S_OK; }
      STDMETHODIMP UpdateMapping(IRealTimeStylus*) override { return S_OK; }
      
      STDMETHODIMP DataInterest(RealTimeStylusDataInterest* pEventInterest) override { 
        *pEventInterest = RTSDI_None;
        return S_OK; 
      };

    public:
      HWND &hwnd() { return _hwnd; }
      
      BasicWin32Widget *parent();
      static BasicWin32Widget *from_hwnd(HWND hwnd);
      
      template<class T>
      T *find_parent() {
        BasicWin32Widget *p = parent();
        while(p) {
          T *t = dynamic_cast<T *>(p);
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

#endif // RICHMATH__GUI__WIN32__BASIC_WIN32_WIDGET_H__INCLUDED
