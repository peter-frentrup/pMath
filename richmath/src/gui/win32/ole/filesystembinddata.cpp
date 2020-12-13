#include <gui/win32/ole/filesystembinddata.h>
#include <gui/win32/ole/combase.h>


using namespace richmath;

static HRESULT CreateBindCtxWithOpts(BIND_OPTS *pbo, IBindCtx **ppbc);
static HRESULT AddFileSystemBindCtx(IBindCtx *pbc, const WIN32_FIND_DATAW &find_data);

//{ class FileSystemBindData ...

STDMETHODIMP FileSystemBindData::QueryInterface(REFIID riid, void **ppv) {
  *ppv = nullptr;
  if(riid == IID_IUnknown || riid == IID_IFileSystemBindData) {
    *ppv = static_cast<IFileSystemBindData *>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) FileSystemBindData::AddRef() {
  return InterlockedIncrement(&refcount);
}

IFACEMETHODIMP_(ULONG) FileSystemBindData::Release() {
  LONG count = InterlockedDecrement(&refcount);
  if (count == 0) delete this;
  return count;
}

IFACEMETHODIMP FileSystemBindData::SetFindData(const WIN32_FIND_DATAW *pfd) {
  find_data = *pfd;
  return S_OK;
}

IFACEMETHODIMP FileSystemBindData::GetFindData(WIN32_FIND_DATAW *pfd) {
  *pfd = find_data;
  return S_OK;
}

FileSystemBindData::FileSystemBindData(const WIN32_FIND_DATAW &find_data)
 : refcount(1),
   find_data(find_data)
{
}

HRESULT FileSystemBindData::CreateInstance(const WIN32_FIND_DATAW &find_data, REFIID riid, void **ppv) {
  *ppv = nullptr;
  HRESULT hr = E_OUTOFMEMORY;
  ComBase<IFileSystemBindData> fsbd;
  fsbd.attach(new FileSystemBindData(find_data));
  if(fsbd) {
    hr = fsbd->QueryInterface(riid, ppv);
  }
  return hr;
}

HRESULT FileSystemBindData::CreateFileSystemBindCtx(const WIN32_FIND_DATAW &find_data, IBindCtx **ppbc) {
  ComBase<IBindCtx> bind_ctx;
  BIND_OPTS bo = { sizeof(bo), 0, STGM_CREATE, 0 };
  HR(CreateBindCtxWithOpts(&bo, bind_ctx.get_address_of()));
  HR(AddFileSystemBindCtx(bind_ctx.get(), find_data));
  *ppbc = bind_ctx.detach();
  return S_OK;
}

//} ... class FileSystemBindData

static HRESULT CreateBindCtxWithOpts(BIND_OPTS *pbo, IBindCtx **ppbc) {
  ComBase<IBindCtx> bind_ctx;
  HR(CreateBindCtx(0, bind_ctx.get_address_of()));
  HR(bind_ctx->SetBindOptions(pbo));
  *ppbc = bind_ctx.detach();
  return S_OK;
}

static HRESULT AddFileSystemBindCtx(IBindCtx *pbc, const WIN32_FIND_DATAW &find_data) {
  ComBase<IFileSystemBindData> fs_bind_data;
  HR(FileSystemBindData::CreateInstance(find_data, IID_PPV_ARGS(fs_bind_data.get_address_of())));
  HR(pbc->RegisterObjectParam((LPOLESTR)STR_FILE_SYS_BIND_DATA, fs_bind_data.get()));
  return S_OK;
}
