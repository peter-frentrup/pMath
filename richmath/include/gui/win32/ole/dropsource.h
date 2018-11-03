#ifndef RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <ole2.h>


namespace richmath {
  class DropSource: public IDropSource {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject) override;
      STDMETHODIMP_(ULONG) AddRef(void) override;
      STDMETHODIMP_(ULONG) Release(void) override;
      
      //
      // IDropSource members
      //
      STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override;
      STDMETHODIMP GiveFeedback(DWORD dwEffect) override;
      
    public:
      DropSource();
      virtual ~DropSource();
      
    private:
      LONG m_lRefCount;
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__DROPSOURCE_H__INCLUDED
