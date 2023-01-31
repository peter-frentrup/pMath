#include <gui/win32/ole/comsidechannel.h>

#include <eval/application.h>


const IID IID_IRichmathComSideChannel { 0x540c3525, 0x260e, 0x40c4, 0xb6, 0x24, 0xa9, 0xfb, 0x9d, 0xa5, 0xf6, 0x87 };

using namespace richmath;

static ComSideChannelBase *volatile_cpp_side_channel = nullptr;

//{ class ComSideChannelBase ...

STDMETHODIMP ComSideChannelBase::PutSelfOnSideChannel(void) {
  volatile_cpp_side_channel = this;
  return S_OK;
}

ComBase<ComSideChannelBase> ComSideChannelBase::from_iunk(IUnknown *pUnk) {
  if(!pUnk)
    return nullptr;
    
  if(!Application::is_running_on_gui_thread())
    return nullptr;
  
  ComBase<IRichmathComSideChannel> comSideChannel;
  if(FAILED(pUnk->QueryInterface(comSideChannel.get_address_of())) || !comSideChannel)
    return nullptr;
  
  // TODO: Locking or thread-local storage required if multiple threads should be allowed.
  volatile_cpp_side_channel = nullptr;
  if(!HRbool(comSideChannel->PutSelfOnSideChannel()))
    return nullptr;
  
  ComBase<ComSideChannelBase> result;
  result.attach(volatile_cpp_side_channel);
  volatile_cpp_side_channel = nullptr;
  
  if(comSideChannel.get() == static_cast<IRichmathComSideChannel*>(result.get())) 
    return result;
  
  return nullptr;
}

//} ... class ComSideChannelBase
