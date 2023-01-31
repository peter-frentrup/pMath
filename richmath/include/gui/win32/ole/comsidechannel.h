#ifndef RICHMATH__GUI__WIN32__OLE__COMSIDECHANNEL_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__COMSIDECHANNEL_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <gui/win32/ole/combase.h>

extern const IID IID_IRichmathComSideChannel;
class IRichmathComSideChannel: public IUnknown {
  public:
    virtual HRESULT STDMETHODCALLTYPE PutSelfOnSideChannel(void) = 0;
};

namespace richmath {
  // Helper class to allow to dynamically cast a COM pointer to a C++ derived class.
  // This is necessary, because dynamic_cast<> can only be used for native C++ classes
  // and would crash for general COM objects which might have no C++ RTTI.
  class ComSideChannelBase : public IRichmathComSideChannel {
    public:
      STDMETHODIMP PutSelfOnSideChannel(void) override;
  
      static ComBase<ComSideChannelBase> from_iunk(IUnknown *pUnk);
  };
}

#ifdef _MSC_VER
  class __declspec(uuid("540C3525-260E-40C4-B624-A9FB9DA5F687")) IRichmathComSideChannel;
#elif defined(__GNUC__)
#  define RICHMATH_MAKE_MINGW_UUID(cls) \
     template<> inline const GUID &__mingw_uuidof<cls>()  { return IID_ ## cls; } \
     template<> inline const GUID &__mingw_uuidof<cls*>() { return IID_ ## cls; }
  
  RICHMATH_MAKE_MINGW_UUID(IRichmathComSideChannel)
  
#  undef RICHMATH_MAKE_MINGW_UUID
#endif


#endif // RICHMATH__GUI__WIN32__OLE__COMSIDECHANNEL_H__INCLUDED
