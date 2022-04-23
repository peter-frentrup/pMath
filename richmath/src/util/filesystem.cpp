#include <util/filesystem.h>

#include <util/base.h>

#ifdef RICHMATH_USE_WIN32_GUI
#  include <gui/win32/ole/combase.h>
#  include <shellapi.h>
#  include <shlwapi.h>
#endif

#ifdef RICHMATH_USE_GTK_GUI
#  include <gtk/gtk.h>
#endif

using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
}}

extern pmath_symbol_t richmath_System_File;
extern pmath_symbol_t richmath_System_FileInformation;
extern pmath_symbol_t richmath_System_List;

//{ class FileSystem ...

String FileSystem::to_existing_absolute_file_name(String filename) {
  if(filename.is_null())
    return String();
  
  Expr info = Evaluate(Call(Symbol(richmath_System_FileInformation), filename));
  if(info[0] == richmath_System_List) {
    size_t len = info.expr_length();
    for(size_t i = 1; i <= len; ++i) {
      Expr rule = info[i];
      if(rule.is_rule() && rule[1] == richmath_System_File) 
        return rule[2];
    }
  }
  
  return String();
}

String FileSystem::to_possibly_nonexisting_absolute_file_name(String filename) {
  if(filename.is_null())
    return String();
  
  return String(pmath_to_absolute_file_name(filename.release()));
}

String FileSystem::file_name_join(String dir, String name) {
  if(dir.is_null() || name.is_null())
    return String();
  
  int dirlen = dir.length();
  if(dirlen > 0) {
#ifdef PMATH_OS_WIN32
    if(dir[dirlen-1] == '\\')
      return dir + name;
    
    if(dir[dirlen-1] == '/')
      dir = dir.part(0, dirlen-1);
#else
    if(dir[dirlen-1] == '/')
      return dir + name;
#endif
  }
  
#ifdef PMATH_OS_WIN32
  return dir + "\\" + name;
#else
  return dir + "/" + name;
#endif
}

bool FileSystem::is_filename_without_directory(String filename) {
  int             i   = filename.length() - 1;
  const uint16_t *buf = filename.buffer();
  if(i < 0)
    return false;
  
#ifdef PMATH_OS_WIN32
  if(buf[i] == '.')
    return false;
#else
  if(i == 0 && buf[i] == '.')
    return false;
    
  if(i == 1 && buf[0] == '.' && buf[1] == '.')
    return false;
#endif
  
  for(;i >= 0;--i) {
    switch(buf[i]) {
      case 0:
      case '/':
#ifdef PMATH_OS_WIN32
      case  1: case  2: case  3: case  4: case  5: case  6: case  7: case  8: case  9: case 10:
      case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20:
      case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
      case 31:
      case '<':
      case '>':
      case ':':
      case '"':
      case '|':
      case '\\':
      case '?':
      case '*':
#endif
        return false;
    }
  }
  
  return true;
}

String FileSystem::extract_directory_path(String *filename) {
  RICHMATH_ASSERT(filename != nullptr);
  
  if(filename->is_null())
    return String();
  
  int             i   = filename->length() - 1;
  const uint16_t *buf = filename->buffer();
  while(i > 0 && buf[i] != '\\' && buf[i] != '/')
    --i;
    
  if(i > 0) {
    String dir = filename->part(0, i);
    *filename = filename->part(i + 1);
    return dir;
  }
  else
    return strings::EmptyString;
}

String FileSystem::get_uri_scheme(String uri) {
  int len = uri.length();
  const uint16_t *buf = uri.buffer();
  
  if(len < 2)
    return String();
  
  if( (buf[0] >= 'a' && buf[0] <= 'z') ||
      (buf[0] >= 'A' && buf[0] <= 'Z'))
  {
    int i = 1;
    while(i < len) {
      if( (buf[i] >= 'a' && buf[i] <= 'z') || 
          (buf[i] >= 'A' && buf[i] <= 'Z') ||
          (buf[i] >= '0' && buf[i] <= '9') ||
          buf[i] == '+' || buf[i] == '-' || buf[i] == '.') 
      {
        ++i;
        continue;
      }
      else  break;
    }
    
    if(i < len && buf[i] == ':')
      return uri.part(0, i);
  }
  
  return String();
}

String FileSystem::get_local_path_from_uri(String uri) {
  if(const uint16_t *uri_buf = uri.buffer()) {
    int uri_len = uri.length();
    if(uri_len == 0)
      return String();
    
    for(int i = 0; i < uri_len; ++i) {
      if(uri_buf[i] <= (uint16_t)' ' || uri_buf[i] > 0x7F)
        return String();
    }
  }
  else
    return String();
  
#if defined(RICHMATH_USE_WIN32_GUI)
  {
    uri+= String::FromChar(0);
    if(const wchar_t *uri_buf = uri.buffer_wchar()) {
      pmath_string_t path = pmath_string_new_raw(uri.length() + 20);
      uint16_t *path_buf;
      int path_capacity;
      if(pmath_string_begin_write(&path, &path_buf, &path_capacity)) {
        DWORD cch_path = (DWORD)path_capacity;
        if(HRbool(PathCreateFromUrlW(uri_buf, (wchar_t*)path_buf, &cch_path, 0))) {
          return String(pmath_string_part(path, 0, (int)cch_path));
        }
      }
      pmath_unref(path);
    }
  }
#elif defined(RICHMATH_USE_GTK_GUI)
  {
    if(char *uri_ascii = pmath_string_to_utf8(uri.get(), nullptr)) {
      String path;
      
      if(char *s = g_filename_from_uri(uri_ascii, nullptr, nullptr)) {
#ifdef WIN32
        path = String(pmath_string_from_utf8(s, -1));
#else
        path = String(pmath_string_from_native(s, -1));
#endif
        g_free(s);
      }
      
      pmath_mem_free(uri_ascii);
      return path;
    }
  }
#endif
  return String();
}

//} ... class FileSystem
