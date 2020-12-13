#ifndef RICHMATH__GUI__WIN32__OLE__FILESYSTEMBINDDATA_H__INCLUDED
#define RICHMATH__GUI__WIN32__OLE__FILESYSTEMBINDDATA_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <ole2.h>
#include <shlobj.h>

namespace richmath {
  class FileSystemBindData : public IFileSystemBindData {
    public:
      static HRESULT CreateInstance(const WIN32_FIND_DATAW &find_data, REFIID riid, void **ppv);
      static HRESULT CreateFileSystemBindCtx(const WIN32_FIND_DATAW &find_data, IBindCtx **ppbc);
      
      ///
      /// IUnknown members
      ///
      IFACEMETHODIMP         QueryInterface(REFIID riid, void **ppv) override;
      IFACEMETHODIMP_(ULONG) AddRef() override;
      IFACEMETHODIMP_(ULONG) Release() override;
      
      ///
      /// IFileSystemBindData methods
      ///
      IFACEMETHODIMP SetFindData(const WIN32_FIND_DATAW *pfd) override;
      IFACEMETHODIMP GetFindData(WIN32_FIND_DATAW *pfd) override;
      
    private:
      FileSystemBindData(const WIN32_FIND_DATAW &find_data);
    
    private:
      LONG             refcount;
      WIN32_FIND_DATAW find_data;
  };
}

#endif // RICHMATH__GUI__WIN32__OLE__FILESYSTEMBINDDATA_H__INCLUDED
