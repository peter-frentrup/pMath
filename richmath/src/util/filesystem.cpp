#include <util/filesystem.h>


using namespace richmath;

namespace richmath { namespace strings {
  extern String EmptyString;
}}

//{ class FileSystem ...


String FileSystem::to_existing_absolute_file_name(String filename) {
  if(filename.is_null())
    return String();
  
  Expr info = Evaluate(Call(Symbol(PMATH_SYMBOL_FILEINFORMATION), filename));
  if(info[0] == PMATH_SYMBOL_LIST) {
    size_t len = info.expr_length();
    for(size_t i = 1; i <= len; ++i) {
      Expr rule = info[i];
      if(rule.is_rule() && rule[1] == PMATH_SYMBOL_FILE) 
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
  assert(filename != nullptr);
  
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


//} ... class FileSystem
