#include "stdafx.h"

extern pmath_symbol_t pmath_System_OperatingSystem;


enum filesystem_flavour_t {
  FILESYSTEM_FLAVOUR_INVALID = -1,
  FILESYSTEM_FLAVOUR_POSIX,  // forward slashes, root is /
  FILESYSTEM_FLAVOUR_MACOSX, // like posix, but also with unicode normalization ... (not yet implemented)
  FILESYSTEM_FLAVOUR_WINDOWS // forward or backward slashes, multiple roots C:\ etc.
};

static enum filesystem_flavour_t get_filesystem_flavour_option(pmath_t options);

static void emit_file_name_components_windows(pmath_string_t s);
static void emit_file_name_components_posix(pmath_string_t s);

PMATH_PRIVATE pmath_t eval_System_FileNameSplit(pmath_expr_t expr) {
  /* FileNameSplit(path)
     
     options:
      OperatingSystem -> "Windows" | "Unix" | "Linux" | "MacOSX" | ...
   */
  pmath_t path;
  pmath_t options;
  enum filesystem_flavour_t flavour;

  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) 
    return expr;
  
  flavour = get_filesystem_flavour_option(options);
  pmath_unref(options);
  
  if(flavour == FILESYSTEM_FLAVOUR_INVALID) 
    return expr;
  
  path = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(path)) {
    pmath_unref(expr);
    pmath_gather_begin(PMATH_NULL);
    
    switch(flavour) {
      case FILESYSTEM_FLAVOUR_INVALID:
      case FILESYSTEM_FLAVOUR_POSIX:
      case FILESYSTEM_FLAVOUR_MACOSX:
        emit_file_name_components_posix(path);
        break;
        
      case FILESYSTEM_FLAVOUR_WINDOWS:
        emit_file_name_components_windows(path);
        break;
    }
    
    pmath_unref(path);
    
    return pmath_gather_end();
  }
  
  pmath_unref(path);
  return expr;
}

static enum filesystem_flavour_t get_filesystem_flavour_option(pmath_t options) {
  pmath_t os = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_OperatingSystem, options));
  if(pmath_is_string(os)) {
    if(pmath_string_equals_latin1(os, "Windows")) {
      pmath_unref(os);
      return FILESYSTEM_FLAVOUR_WINDOWS;
    }
    
    if(pmath_string_equals_latin1(os, "MacOSX")) {
      pmath_unref(os);
      return FILESYSTEM_FLAVOUR_MACOSX;
    }
    
    if( pmath_string_equals_latin1(os, "Linux") || 
        pmath_string_equals_latin1(os, "Solaris") || 
        pmath_string_equals_latin1(os, "SunOS") ||
        pmath_string_equals_latin1(os, "Unix")) 
    {
      pmath_unref(os);
      return FILESYSTEM_FLAVOUR_POSIX;
    }
  }
  
  pmath_message(PMATH_NULL, "ostype", 1, os);
  return FILESYSTEM_FLAVOUR_INVALID;
}

static void emit_file_name_components_windows(pmath_string_t s) {
  const int len = pmath_string_length(s);
  const uint16_t *buf = pmath_string_buffer(&s);
  int pos = 0;
  uint16_t alt_dir_sep = '/';
  
  if(len == 0)
    return;
  
  if(len >= 2 && buf[0] == '\\' && buf[1] == '\\') { //  \\SERVER\SHARE\rest\of\path
    int server_end_pos = 2;
    
    if(len >= 4 && buf[2] == '?' && buf[3] == '\\')  {
      alt_dir_sep = '\\'; // \\?\C:\rest\of\path
      if(len >= 8 && buf[4] == 'U' && buf[5] == 'N' && buf[6] == 'C' && buf[7] == '\\') 
        server_end_pos = 8; //  \\?\UNC\SERVER\SHARE\rest\of\path
    }
    
    while(server_end_pos < len && (buf[server_end_pos] != '\\' && buf[server_end_pos] != alt_dir_sep))
      ++server_end_pos;
    
    if(server_end_pos < len) {
      ++server_end_pos;
      while(server_end_pos < len && (buf[server_end_pos] != '\\' && buf[server_end_pos] != alt_dir_sep))
        ++server_end_pos;
    }
    
    pmath_emit(pmath_string_part(pmath_ref(s), 0, server_end_pos), PMATH_NULL);
    pos = server_end_pos + 1;
  }
  else if(len >= 3 && buf[1] == ':' && (buf[2] == '\\' || buf[2] == alt_dir_sep)) { //  C:\rest\of\path
    pmath_emit(pmath_string_part(pmath_ref(s), 0, 3), PMATH_NULL);
    pos = 3;
  }
  
  while(pos < len) {
    int next = pos;
    while(next < len && buf[next] != '\\' && buf[next] != alt_dir_sep)
      ++next;
    
    pmath_emit(pmath_string_part(pmath_ref(s), pos, next - pos), PMATH_NULL);
    pos = next + 1;
  }
}

static void emit_file_name_components_posix(pmath_string_t s) {
  const int len = pmath_string_length(s);
  const uint16_t *buf = pmath_string_buffer(&s);
  int pos = 0;
  
  if(len == 0)
    return;
    
  if(buf[0] == '/') {
    pos = 1;
    if(len >= 2 && buf[1] == '/' && (len == 2 || buf[2] != '/')) {
      pos = 2;
    }
    pmath_emit(pmath_string_part(pmath_ref(s), 0, pos), PMATH_NULL);
  }
  
  while(pos < len) {
    int next;
    while(pos < len && buf[pos] == '/')
      ++pos;
    
    next = pos;
    while(next < len && buf[next] != '/')
      ++next;
    
    pmath_emit(pmath_string_part(pmath_ref(s), pos, next - pos), PMATH_NULL);
    pos = next + 1;
  }
}
