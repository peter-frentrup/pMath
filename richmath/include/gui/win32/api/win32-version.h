#ifndef RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED
#define RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>
#include <cstdio>

namespace richmath {
  class Win32Version {
    public:
      static bool check_osversion(int min_major, int min_minor, int min_build = 0);
      static bool is_windows_vista_or_newer() {   return check_osversion(6, 0); }
      static bool is_windows_7_or_newer() {       return check_osversion(6, 1); }
      static bool is_windows_8_or_newer() {       return check_osversion(6, 2); }
      static bool is_windows_8_1_or_newer() {     return check_osversion(6, 3); }
      static bool is_windows_10_or_newer() {      return check_osversion(10, 0); }
      static bool is_windows_10_1803_or_newer() { return check_osversion(10, 0, 17134); }
      static bool is_windows_10_1809_or_newer() { return check_osversion(10, 0, 17763); }
      static bool is_windows_10_1903_or_newer() { return check_osversion(10, 0, 18362); }
      static bool is_windows_10_1909_or_newer() { return check_osversion(10, 0, 18363); }
      static bool is_windows_10_20H1_or_newer() { return check_osversion(10, 0, 19041); }
      static bool is_windows_10_20H2_or_newer() { return check_osversion(10, 0, 19042); }
      static bool is_windows_10_21H1_or_newer() { return check_osversion(10, 0, 19043); }
      static bool is_windows_11_or_newer() {      return check_osversion(10, 0, 22000); }
      
      static void init();
      
    private:
      Win32Version();
      ~Win32Version();
  };
  
  #define WIN32report(call)                check_WIN32_nonzero((call), #call, __FILE__, __LINE__)
  #define WIN32report_errval(call, badres) check_WIN32_errval((call), (badres), #call, __FILE__, __LINE__)
#ifndef NDEBUG
  template<typename T>
  static inline T check_WIN32_nonzero(T res, const char *call, const char *file, int line) {
    if(!res) {
      if(DWORD err = GetLastError())
        fprintf(stderr, "%s:%d: call %s failed with 0x%x\n", file, line, call, err);
    }
    return res;
  }
  template<typename T>
  static inline T check_WIN32_errval(T res, T badres, const char *call, const char *file, int line) {
    if(res == badres) {
      if(DWORD err = GetLastError())
        fprintf(stderr, "%s:%d: call %s failed with 0x%x\n", file, line, call, err);
    }
    return res;
  }
#else
  #define check_WIN32_nonzero(res,         call, file, line)  (res)
  #define check_WIN32_errval( res, badres, call, file, line)  (res)
#endif // NDEBUG
}

#endif // RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED
