#ifdef _WIN32_WINNT
#  if _WIN32_WINNT < 0x602
#    undef _WIN32_WINNT
#  endif
#endif

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x602 // Direct MAnipulation requires Windows 8 or newer (0x602)
#endif

#include <initguid.h>

#include <gui/win32/api/win32-hd-trackpad.h>

#include <gui/win32/api/win32-touch.h>
#include <gui/win32/ole/combase.h>
#include <gui/native-widget.h>
#include <util/base.h>
#include <graphics/rectangle.h>

#include <directmanipulation.h>
#include <cmath>

#define TRACKPAD_TIMER_DELAY  (16)

using namespace richmath;

namespace richmath {
  class HiDefTrackpadHandlerImpl : public IDirectManipulationViewportEventHandler, public Base {
      enum class State : uint8_t {
        None,
        Scrolling,
        Zooming,
      };
    public:
      HiDefTrackpadHandlerImpl();
      virtual ~HiDefTrackpadHandlerImpl() {}
      
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IDirectManipulationViewportEventHandler members
      //
      STDMETHODIMP OnViewportStatusChanged(IDirectManipulationViewport *viewport, DIRECTMANIPULATION_STATUS current, DIRECTMANIPULATION_STATUS previous) override;
      STDMETHODIMP OnViewportUpdated(IDirectManipulationViewport *viewport) override;
      STDMETHODIMP OnContentUpdated(IDirectManipulationViewport *viewport, IDirectManipulationContent *content) override;
   
      HRESULT init(HWND hwnd, NativeWidget *destination);
      void detach();
      HRESULT reset_viewport();
      HRESULT on_pointer_hit_test(WPARAM wParam);
      HRESULT on_timer();
      void start_timer();
      void stop_timer();
      HRESULT update();
      
    public:
      HWND                                       _hwnd;
      NativeWidget                              *_destination;
      ComBase<IDirectManipulationManager>        _manager;
      ComBase<IDirectManipulationUpdateManager>  _update_manager;
      ComBase<IDirectManipulationViewport>       _viewport;
      
      float _last_scale;
      Point _last_pos;
      State _state;
      
    private:
      LONG _refcount;
  };
}

static const RECT DefaultViewportRect = {0, 0, 1000, 1000};

//{ class HiDefTrackpadHandler ...

HiDefTrackpadHandler::TimerIdType HiDefTrackpadHandler::TimerId;

HiDefTrackpadHandler::HiDefTrackpadHandler()
: _data(nullptr)
{
}
 
HiDefTrackpadHandler::~HiDefTrackpadHandler() {
  detach();
}

void HiDefTrackpadHandler::init(HWND hwnd, NativeWidget *destination) {
  if(_data)
    return;
    
  _data = new HiDefTrackpadHandlerImpl;
  
  if(!HRbool(_data->init(hwnd, destination))) {
    detach();
    return;
  }
}

void HiDefTrackpadHandler::detach() {
  if(auto tmp = _data) {
    _data = nullptr;
    tmp->detach();
    tmp->Release();
  }
}

void HiDefTrackpadHandler::on_pointer_hit_test(WPARAM wParam) {
  if(!_data)
    return;
  
  HRreport(_data->on_pointer_hit_test(wParam));
}

void HiDefTrackpadHandler::on_timer() {
  if(!_data)
    return;
  
  HRreport(_data->on_timer());
}

//} ... class HiDefTrackpadHandler

//{ class HiDefTrackpadHandlerImpl ...

HiDefTrackpadHandlerImpl::HiDefTrackpadHandlerImpl() 
: Base(),
  _hwnd{nullptr},
  _destination{nullptr},
  _last_scale{1.0f},
  _last_pos{0.0f, 0.0f},
  _state{State::None},
  _refcount(1)
{
}

HRESULT HiDefTrackpadHandlerImpl::init(HWND hwnd, NativeWidget *destination) {
  if(_hwnd)
    return E_UNEXPECTED;
  
  if(!hwnd)
    return E_INVALIDARG;
  
  _destination = destination;
  _hwnd = hwnd;
  HR(CoCreateInstance(
       CLSID_DirectManipulationManager, nullptr, CLSCTX_INPROC_SERVER,
       _manager.iid(), (void**)_manager.get_address_of()));
  
  HR(_manager->GetUpdateManager(_update_manager.iid(), (void**)_update_manager.get_address_of()));
  
  HR(_manager->CreateViewport(nullptr, hwnd, _viewport.iid(), (void**)_viewport.get_address_of()));
  
  DIRECTMANIPULATION_CONFIGURATION configuration =
    DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
    DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
    DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
    DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
    DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
    DIRECTMANIPULATION_CONFIGURATION_RAILS_Y |
    DIRECTMANIPULATION_CONFIGURATION_SCALING;
  
  HR(_viewport->ActivateConfiguration(configuration));
  
  HR(_viewport->SetViewportOptions(DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE));
  
  DWORD dummy_cookie; // don't need to remember, because we will Abandon() the viewport
  HR(_viewport->AddEventHandler(hwnd, this, &dummy_cookie));
  
  HR(_viewport->SetViewportRect(&DefaultViewportRect));
  HR(_manager->Activate(hwnd));
  HR(_viewport->Enable());
  HR(reset_viewport());
  HR(_update_manager->Update(nullptr));
  
  return S_OK;
}

HRESULT HiDefTrackpadHandlerImpl::reset_viewport() {
  if(!_viewport)
    return E_UNEXPECTED;
    
  HR(_viewport->ZoomToRect(
      DefaultViewportRect.left,  DefaultViewportRect.top,
      DefaultViewportRect.right, DefaultViewportRect.bottom,
      false));
  
  _state = State::None;
  _last_scale = 1.0f;
  _last_pos = Point{0.0f, 0.0f};
  
  return S_OK;
}

void HiDefTrackpadHandlerImpl::detach() {
  stop_timer();
  
  if(_viewport) {
    HRreport(_viewport->Abandon());
    _viewport.reset();
  }
  
  _hwnd = nullptr;
}

HRESULT HiDefTrackpadHandlerImpl::on_pointer_hit_test(WPARAM wParam) {
  if(!_viewport)
    return E_UNEXPECTED;
  
  UINT id = GET_POINTERID_WPARAM(wParam);
  Win32Touch::POINTER_INPUT_TYPE type;
  
  if(Win32Touch::GetPointerType && Win32Touch::GetPointerType(id, &type)) {
    pmath_debug_print("[DM_POINTERHITTEST id=%d type=%d]\n", id, type);
    if(type == Win32Touch::PT_TOUCHPAD) {
      HR(_viewport->SetContact(id));
    }
  }
  
  return S_OK;
}

HRESULT HiDefTrackpadHandlerImpl::on_timer() {
  stop_timer();
  
  pmath_debug_print("T");
  if(_update_manager)
    HR(_update_manager->Update(nullptr));
  
  if(_viewport) {
    DIRECTMANIPULATION_STATUS status;
    HR(_viewport->GetStatus(&status));
    
    switch(status) {
      case DIRECTMANIPULATION_RUNNING:
      case DIRECTMANIPULATION_INERTIA:
        start_timer();
        break;
      
      default:
        break;
    }
  }
    
  return S_OK;
}

void HiDefTrackpadHandlerImpl::start_timer() {
  if(!_hwnd)
    return;
  
  SetTimer(_hwnd, (UINT_PTR)&HiDefTrackpadHandler::TimerId, TRACKPAD_TIMER_DELAY, nullptr);
}

void HiDefTrackpadHandlerImpl::stop_timer() {
  if(_hwnd)
    KillTimer(_hwnd, (UINT_PTR)&HiDefTrackpadHandler::TimerId);
}

HRESULT HiDefTrackpadHandlerImpl::update() {
  if(_update_manager)
    return _update_manager->Update(nullptr);
  
  return S_OK;
}

//
//  IUnknown::AddRef
//
STDMETHODIMP_(ULONG) HiDefTrackpadHandlerImpl::AddRef(void) {
  return InterlockedIncrement(&_refcount);
}

//
//  IUnknown::Release
//
STDMETHODIMP_(ULONG) HiDefTrackpadHandlerImpl::Release(void) {
  LONG count = InterlockedDecrement(&_refcount);
  
  if(count == 0) {
    delete this;
    return 0;
  }
  
  return count;
}

//
//  IUnknown::QueryInterface
//
STDMETHODIMP HiDefTrackpadHandlerImpl::QueryInterface(REFIID iid, void **ppvObject) {
  const IID IID_IDirectManipulationViewportEventHandler =
      __uuidof(IDirectManipulationViewportEventHandler);
  
  if(iid == IID_IUnknown || iid == IID_IDirectManipulationViewportEventHandler) {
    AddRef();
    *ppvObject = static_cast<IDirectManipulationViewportEventHandler*>(this);
    return S_OK;
  }
  
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

//
// IDirectManipulationViewportEventHandler::OnViewportStatusChanged
//
STDMETHODIMP HiDefTrackpadHandlerImpl::OnViewportStatusChanged(
  IDirectManipulationViewport *viewport, 
  DIRECTMANIPULATION_STATUS current, 
  DIRECTMANIPULATION_STATUS previous
) {
  pmath_debug_print("[DM:OnViewportStatusChanged %d -> %d]\n", previous, current);
  
  switch(current) {
    case DIRECTMANIPULATION_RUNNING:
    case DIRECTMANIPULATION_INERTIA:
      start_timer();
      break;
    
    default:
      stop_timer();
      break;
  }
  
  if(current == DIRECTMANIPULATION_READY) {
    HR(reset_viewport());
    
    if(_destination) {
      float scale = _destination->custom_scale_factor();
      
      static const float good_scales[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f};
      for(float good_scale : good_scales) {
        if(fabs(scale - good_scale) < 0.1f) {
          if(scale == good_scale)
            break;
          
          pmath_debug_print("[snap scale %.2f -> %.2f]\n", scale, good_scale);
          
          _destination->set_custom_scale(good_scale);
        }
      }
    }
  }
  
  return S_OK;
}

//
// IDirectManipulationViewportEventHandler::OnViewportUpdated
//
STDMETHODIMP HiDefTrackpadHandlerImpl::OnViewportUpdated(IDirectManipulationViewport *viewport) {
  return S_OK;
}

//
// IDirectManipulationViewportEventHandler::OnContentUpdated
//
STDMETHODIMP HiDefTrackpadHandlerImpl::OnContentUpdated(IDirectManipulationViewport *viewport, IDirectManipulationContent *content) {
  float matrix[6];
  
  HR(content->GetContentTransform(matrix, sizeof(matrix)/sizeof(float)));
//  pmath_debug_print("[DM:OnContentUpdated %d %.2f %.2f  %.2f %.2f  %.2f %.2f ]\n", 
//    _state,
//    matrix[0], matrix[1], 
//    matrix[2], matrix[3], 
//    matrix[4], matrix[5]);
  
  float scale = matrix[0];
  Point pos{matrix[4], matrix[5]};
  
  if(!(scale > 0)) {
    return E_UNEXPECTED;
  }
  
  if(scale != _last_scale) {
    float rel_scale = scale / _last_scale;
    
    if(fabs(rel_scale - 1.0f) > 0.001f) {
      _last_scale = scale;
      
      _state = State::Zooming;
      //pmath_debug_print("[DM: zoom by %f]\n", rel_scale);
      
      if(_destination)
        _destination->scale_by(rel_scale);
    }
  }
  
  if(_state == State::None || _state == State::Scrolling) {
    if(pos != _last_pos) {
      Vector2F delta = pos - _last_pos;
      
      _state = State::Scrolling;
      _last_pos = pos;
    
      //pmath_debug_print("[DM: move by %f %f]\n", delta.x, delta.y);
      if(_destination)
        _destination->scroll_by(-delta);
    
    }
  }
  
  return S_OK;
}

//} ... class HiDefTrackpadHandlerImpl
