#ifndef RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED
#define RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED

#ifndef RICHMATH_USE_WIN32_GUI
#  error this header is win32 specific
#endif

#include <windows.h>

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
}

#endif // RICHMATH__GUI__WIN32__API__WIN32_VERSION_H__INCLUDED
