#ifndef __GUI__WIN32__OLE__DROPSOURCE_H__
#define __GUI__WIN32__OLE__DROPSOURCE_H__

#include <ole2.h>

namespace richmath{
  class DropSource: public IDropSource {
    public:
      //
      // IUnknown members
      //
      STDMETHODIMP         QueryInterface(REFIID iid, void **ppvObject);
      STDMETHODIMP_(ULONG) AddRef(        void);
      STDMETHODIMP_(ULONG) Release(       void);
        
      //
      // IDropSource members
      //
      STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState);
      STDMETHODIMP GiveFeedback(     DWORD dwEffect);
    
    public:
      DropSource();
      ~DropSource();
    
    private:
      LONG m_lRefCount;
  };
}

#endif // __GUI__WIN32__OLE__DROPSOURCE_H__
