#include <gui/win32/api/win32-version.h>


using namespace richmath;

//{ class Win32Version ...

void Win32Version::init() {
  static Win32Version w;
}

Win32Version::Win32Version() {
}

Win32Version::~Win32Version() {
}

bool Win32Version::check_osversion(int min_major, int min_minor, int min_build) {
  OSVERSIONINFO osvi;
  memset(&osvi, 0, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  GetVersionEx(&osvi);
  
  if((int)osvi.dwMajorVersion > min_major)
    return true;
    
  if((int)osvi.dwMajorVersion < min_major)
    return false;
    
  if((int)osvi.dwMinorVersion > min_minor)
    return true;
    
  if((int)osvi.dwMinorVersion < min_minor)
    return false;
    
  return (int)osvi.dwBuildNumber >= min_build;
}

//} ... class Win32Version
