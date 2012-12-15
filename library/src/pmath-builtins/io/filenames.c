#include <pmath-core/expressions-private.h>

#include <pmath-language/regex-private.h>
#include <pmath-language/scanner.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/io-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>
#else
#include <dirent.h>
#include <string.h>
#endif

#define PCRE_STATIC
#include <pcre.h>


static void emit_directory_entries(
  struct _regex_t    *regex,
  struct _capture_t  *capture,
  pmath_t             directory // will be freed
) {
  if( pmath_same(directory, PMATH_UNDEFINED) ||
      pmath_is_string(directory))
  {
#ifdef PMATH_OS_WIN32
    {
      static const uint16_t rest[4] = {'.', '\\', '*', '\0'};
      pmath_bool_t is_default = pmath_same(directory, PMATH_UNDEFINED);
      
      if(is_default) {
        directory = pmath_string_insert_ucs2(PMATH_NULL, 0, rest, 4);
      }
      else{
        if(pmath_string_buffer(&directory)[pmath_string_length(directory) - 1] == '\\')
          directory = pmath_string_insert_ucs2(directory, INT_MAX, rest + 2, 2);
        else
          directory = pmath_string_insert_ucs2(directory, INT_MAX, rest + 1, 3);
      }
      
      if(!pmath_is_null(directory)) {
        WIN32_FIND_DATAW data;
        HANDLE h = FindFirstFileW(pmath_string_buffer(&directory), &data);
        
        if(!is_default)
          directory = pmath_string_part(directory, 0, pmath_string_length(directory) - 2);
          
        if(h != INVALID_HANDLE_VALUE) {
          do {
            pmath_string_t subject, fullname;
            
            if( data.cFileName[0] == '.' &&
            data.cFileName[1] == '\0')
            {
              continue;
            }
            
            if( data.cFileName[0] == '.' &&
                data.cFileName[1] == '.' &&
                data.cFileName[2] == '\0')
            {
              continue;
            }
            
            fullname = pmath_string_insert_ucs2(PMATH_NULL, 0, data.cFileName, -1);
            
            if(!is_default)
              fullname = pmath_string_concat(pmath_ref(directory), fullname);
              
            /* If we cannot access the file through its normal name, we use the
               alternate name (DOS 8.3 name).
               This happens sometimes when my mobile phone moves files on the
               memory card: The file is named e.g. "Foto-0038.jpg..." with "..."
               being random more characters.
               Maybe I should just buy a new memory card or phone... :D
             */
            if(data.cAlternateFileName[0] != '\0') {
              fullname = pmath_string_insert_latin1(fullname, INT_MAX, "", 1); // zero terminate
              
              if(!pmath_is_null(fullname)) {
                HANDLE hfile = CreateFileW(
                                 (const wchar_t *)pmath_string_buffer(&fullname),
                                 0,
                                 FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
                                 
                if(hfile == INVALID_HANDLE_VALUE) { // normal file name does not work
                  pmath_unref(fullname);
                  
                  fullname = pmath_string_insert_ucs2(PMATH_NULL, 0, data.cAlternateFileName, -1);
                  
                  if(!is_default)
                    fullname = pmath_string_concat(pmath_ref(directory), fullname);
                    
                  fullname = pmath_string_insert_latin1(fullname, INT_MAX, "", 1); // zero terminate
                  
                  if(!pmath_is_null(fullname)) {
                    hfile = CreateFileW(
                              (const wchar_t *)pmath_string_buffer(&fullname),
                              0,
                              FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
                              
                    if(hfile == INVALID_HANDLE_VALUE) { // alternate name does not work either !?!
                      pmath_unref(fullname);
                      
                      fullname = pmath_string_insert_ucs2(PMATH_NULL, 0, data.cFileName, -1);
                      
                      if(!is_default)
                        fullname = pmath_string_concat(pmath_ref(directory), fullname);
                    }
                    else {
                      fullname = pmath_string_part(fullname, 0, pmath_string_length(fullname) - 1); // remove terminating zero
                      CloseHandle(hfile);
                    }
                  }
                  else 
                    continue;
                }
                else {
                  fullname = pmath_string_part(fullname, 0, pmath_string_length(fullname) - 1); // remove terminating zero
                  CloseHandle(hfile);
                }
              }
            }
            
            if(is_default)
              subject = pmath_ref(fullname);
            else
              subject = pmath_string_part(pmath_ref(fullname), pmath_string_length(directory), -1);
              
            if(_pmath_regex_match(regex, subject, 0, 0, capture, NULL)) {
              pmath_emit(fullname, PMATH_NULL);
            }
            else
              pmath_unref(fullname);
              
            pmath_unref(subject);
          } while(FindNextFileW(h, &data) && !pmath_aborting());
          
          {
            DWORD err = GetLastError();
            
            if(err != ERROR_NO_MORE_FILES)
              pmath_debug_print("error: %x\n", (int)err);
          }
          FindClose(h);
        }
      }
    }
#else
    {
      DIR *dir = NULL;
      struct dirent *entry;
    
      if(pmath_same(directory, PMATH_UNDEFINED)) {
        dir = opendir(".");
      }
      else{
        char *dir_name;
        dir_name = pmath_string_to_native(directory, NULL);
    
        if(dir_name) {
          int len = pmath_string_length(directory);
          dir = opendir(dir_name);
          pmath_mem_free(dir_name);
    
          if(pmath_string_buffer(&directory)[len - 1] != '/')
            directory = pmath_string_insert_latin1(directory, len, "/", 1);
        }
      }
    
      if(dir) {
        while(0 != (entry = readdir(dir))) {
          pmath_string_t str = pmath_string_from_native(entry->d_name, -1);
    
          if( !pmath_string_equals_latin1(str, ".") &&
          !pmath_string_equals_latin1(str, "..") &&
          _pmath_regex_match(regex, str, 0, 0, capture, NULL))
          {
            if(!pmath_same(directory, PMATH_UNDEFINED))
              str = pmath_string_concat(pmath_ref(directory), str);
    
            pmath_emit(str, PMATH_NULL);
          }
          else
            pmath_unref(str);
        }
    
        closedir(dir);
      }
    }
#endif
  }
  else if(pmath_is_expr_of(directory, PMATH_SYMBOL_LIST)) {
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(directory); ++i) {
      emit_directory_entries(regex, capture,
                             pmath_expr_get_item(directory, i));
    }
  }
  
  pmath_unref(directory);
}

static pmath_t prepare_filename_form(pmath_t obj) {
  if(pmath_is_string(obj)) {
    return pmath_evaluate(
             pmath_parse_string_args(
               "StringReplace(`1`, \"*\"->~~~)",
               "(o)", obj));
  }
  
  if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_LITERAL, 1)) {
    pmath_t s = pmath_expr_get_item(obj, 1);
    
    if(pmath_is_string(s)) {
      pmath_unref(obj);
      return s;
    }
    
    pmath_unref(s);
    return obj;
  }
  
  if(pmath_is_expr_of(obj, PMATH_SYMBOL_LIST)) {
    size_t i;
    for(i = pmath_expr_length(obj); i > 0; --i) {
      obj = pmath_expr_set_item(
              obj, i,
              prepare_filename_form(
                pmath_expr_get_item(obj, i)));
    }
  }
  
  return obj;
}

PMATH_PRIVATE pmath_t builtin_filenames(pmath_expr_t expr) {
  /* FileNames()                         = FileNames(".",  "*")
     FileNames(form)                     = FileNames(".", form)
     FileNames(dir, form)
     FileNames({dir1, dir2, ...}, form) ... search in directories dir_i
  
     form can be StringExpression(...), RegularExpression(...), Literal("text")
     or "string". If it is "string", "*" stands for a sequence of any 
     characters. The pattern is embraced in StartOfString ++ ... ++ EndOfString.
  
     Options:
      IgnoreCase->Automatic
  
     Messages:
      General::opttfa
   */
  pmath_t form, directory;
  struct _regex_t   *regex;
  struct _capture_t  capture;
  size_t exprlen = pmath_expr_length(expr);
  size_t last_nonoption = 1;
  int pcre_options = 0;
  
  directory = PMATH_UNDEFINED;
  if(exprlen >= 2) {
    pmath_t snd = pmath_expr_get_item(expr, 2);
    
    if(!pmath_is_set_of_options(snd)) {
      directory = pmath_expr_get_item(expr, 1);
      last_nonoption = 2;
    }
    
    pmath_unref(snd);
  }
  else {
    if(exprlen == 0)
      last_nonoption = 0;
  }
  
  { // options
    pmath_t option_value, options;
    
    options = pmath_options_extract(expr, last_nonoption);
    if(pmath_is_null(options))
      return expr;
      
    option_value = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_IGNORECASE, options));
    pmath_unref(options);
    if(pmath_same(option_value, PMATH_SYMBOL_TRUE)) {
      pcre_options = PCRE_CASELESS;
    }
    else if(pmath_same(option_value, PMATH_SYMBOL_AUTOMATIC)) {
#ifdef PMATH_OS_WIN32
      pcre_options = PCRE_CASELESS;
#endif
    }
    else if(!pmath_same(option_value, PMATH_SYMBOL_FALSE)) {
      pmath_message(
        PMATH_NULL, "opttfa", 2,
        pmath_ref(PMATH_SYMBOL_IGNORECASE),
        option_value);
      pmath_unref(directory);
      return expr;
    }
    pmath_unref(option_value);
  }
  
  regex = 0;
  if(last_nonoption > 0)
    form = pmath_expr_get_item(expr, last_nonoption);
  else
    form = PMATH_C_STRING("*");
    
  pmath_unref(expr);
  
  form = prepare_filename_form(form);
  form = pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_STRINGEXPRESSION), 3,
    pmath_ref(PMATH_SYMBOL_STARTOFSTRING),
    form,
    pmath_ref(PMATH_SYMBOL_ENDOFSTRING));
  
  regex = _pmath_regex_compile(form, pcre_options);
  
  if(!regex) {
    pmath_unref(directory);
    return pmath_ref(_pmath_object_emptylist);
  }
  
  pmath_gather_begin(PMATH_NULL);
  _pmath_regex_init_capture(regex, &capture);
  if(capture.ovector) {
    emit_directory_entries(regex, &capture, directory);
    directory = PMATH_NULL;
  }
  _pmath_regex_free_capture(&capture);
  _pmath_regex_unref(regex);
  pmath_unref(directory);
  return pmath_gather_end();
}
