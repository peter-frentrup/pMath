#include <pmath-core/numbers.h>
#include <pmath-core/strings-private.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>

#include <limits.h>

#ifdef PMATH_OS_WIN32
  #define WIN32_LEAN_AND_MEAN 
  #define NOGDI
  #include <Windows.h>
#else
  #include <errno.h>
  #include <pwd.h>
  #include <string.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

static pmath_string_t get_directory(void){
  #ifdef PMATH_OS_WIN32
  {
    struct _pmath_string_t *result;
    wchar_t *buffer;
    DWORD    buflen = 0;
    DWORD    reslen = 0;
    
    buflen = GetCurrentDirectory(0, NULL);
    if((int)buflen * sizeof(wchar_t) <= 0)
      return NULL;
    
    buffer = pmath_mem_alloc(sizeof(wchar_t) * buflen);
    if(!buffer)
      return NULL;
    
    GetCurrentDirectoryW(buflen, buffer);
    buffer[buflen-1] = L'\0';
    
    reslen = GetFullPathNameW(buffer, 0, NULL, NULL);
    if((int)reslen * sizeof(uint16_t) <= 0){
      pmath_mem_free(buffer);
      return NULL;
    }
    
    result = _pmath_new_string_buffer(reslen);
    if(result){
      GetFullPathNameW(buffer, reslen, (wchar_t*)AFTER_STRING(result), NULL);
      
      result->length = reslen - 1;
    }
    
    pmath_mem_free(buffer);
    return (pmath_string_t)result;
  }
  #else
  {
    char   *buffer = NULL;
    size_t  size = 256;
    for(;;){
      buffer = pmath_mem_realloc(buffer, 2*size);
      size*= 2;
      
      if(!buffer)
        return NULL;
      
      if(getcwd(buffer, size)){
        pmath_string_t result = pmath_string_from_native(buffer, -1);
        pmath_mem_free(buffer);
        return result;
      }
      
      if(errno != ERANGE){
        pmath_mem_free(buffer);
        return NULL;
      }
    }
  }
  #endif
}

#ifndef PMATH_OS_WIN32
static int first_slash(const uint16_t *buf, int len){
  int i = 0;
  for(i = 0;i < len;++i)
    if(buf[i] == '/')
      return i;
  
  return len;
}

static int last_slash(const uint16_t *buf, int len){
  int i = 0;
  for(i = len-1;i >= 0;--i)
    if(buf[i] == '/')
      return i;
  
  return -1;
}
#endif

PMATH_PRIVATE 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t _pmath_canonical_file_name(pmath_string_t relname){
  if(pmath_string_length(relname) == 0)
    return relname;
  
  #ifdef PMATH_OS_WIN32
  {
    DWORD reslen;
    struct _pmath_string_t *result;
    
    relname = pmath_string_insert_latin1(relname, INT_MAX, "", 1);
    if(!relname)
      return NULL;
    
    reslen = GetFullPathNameW(pmath_string_buffer(relname), 0, NULL, NULL);
    if((int)reslen * sizeof(uint16_t) <= 0){
      pmath_unref(relname);
      return NULL;
    }
    
    result = _pmath_new_string_buffer(reslen);
    if(result){
      GetFullPathNameW(
        pmath_string_buffer(relname), 
        reslen, 
        (wchar_t*)AFTER_STRING(result), 
        NULL);
      result->length = reslen - 1;
    }
    
    pmath_unref(relname);
    return (pmath_string_t)result;
  }
  #else
  {
    struct _pmath_string_t *result;
    pmath_string_t dir;
    int dirlen, namelen, enddir, startname;
    int             rellen = pmath_string_length(relname);
    const uint16_t *ibuf   = pmath_string_buffer(relname);
    uint16_t       *obuf;
    
    if(ibuf[0] == '/'){
      dir = PMATH_C_STRING("");//pmath_string_part(pmath_ref(relname), 0, 1);
      relname = pmath_string_part(relname, 1, INT_MAX);
    }
    else if(ibuf[0] == '~'){
      const char *prefix = 0;
      
      int slashpos = 1;
      while(slashpos < rellen && ibuf[slashpos] != '/')
        ++slashpos;
      
      if(slashpos == 1){ // "~/rest/of/path"
        prefix = getenv("HOME");
        if(!prefix){
          struct passwd *pw = getpwuid(getuid());
          if(pw)
            prefix = pw->pw_dir;
        }
      }
      else{ // "~user/rest/of/path"
        pmath_string_t user = pmath_string_part(pmath_ref(relname), 1, slashpos-1);
        char *user_s = pmath_string_to_native(user, 0);
        
        if(user_s){
          struct passwd *pw = getpwnam(user_s);
          if(pw)
            prefix = pw->pw_dir;
        }
        
        pmath_mem_free(user_s);
        pmath_unref(user);
      }
      
      if(prefix){
        dir = pmath_string_from_native(prefix, -1);
        relname = pmath_string_part(relname, rellen+1, INT_MAX); // all after "/"
      }
      else{
        dir = PMATH_C_STRING("");
        relname = pmath_string_part(relname, rellen+1, INT_MAX); // all after "/"
      }
    }
    else
      dir = get_directory();
  
    if(!dir){
      pmath_unref(relname);
      return NULL;
    }
    
    dirlen  = pmath_string_length(dir);
    namelen = pmath_string_length(relname);
    
    result = _pmath_new_string_buffer(dirlen + namelen + 1);
    if(!result){
      pmath_unref(dir);
      pmath_unref(relname);
      return NULL;
    }
    
    memcpy(AFTER_STRING(result), pmath_string_buffer(dir), sizeof(uint16_t) * dirlen);
    pmath_unref(dir);
    obuf = AFTER_STRING(result);
    obuf[dirlen] = '/';
    memcpy(obuf + dirlen + 1, pmath_string_buffer(relname), sizeof(uint16_t) * namelen);
    pmath_unref(relname);
    
    enddir = startname = dirlen + 1;
    namelen = dirlen + namelen + 1;
    
    while(startname < namelen){
      int next = startname + first_slash(obuf + startname, namelen - startname);
      
      if(next == startname + 2 
      && obuf[startname]     == '.'
      && obuf[startname + 1] == '.'){
        enddir = last_slash(obuf, enddir - 1) + 1;
        if(enddir < 0)
          enddir = 0;
      }
      else if(next != startname + 1 || obuf[startname] != '.'){
        memmove(obuf + enddir, obuf + startname, (next - startname) * sizeof(uint16_t));
        enddir+= next - startname;
        obuf[enddir] = '/';
        ++enddir;
      }
      
      startname = next + 1;
    }
    
    while(enddir > 1 && obuf[enddir - 1] == '/')
      --enddir;
    
    result->length = enddir;
    return (pmath_string_t)result;
  }
  #endif
}

static pmath_bool_t try_change_directory(
  pmath_string_t name // will be freed
){
  #ifdef PMATH_OS_WIN32
  {
    static const uint16_t zero = 0;
    
    name = pmath_string_insert_ucs2(name, INT_MAX, &zero, 1);
    if(!name)
      return FALSE;
    
    if(SetCurrentDirectoryW((const wchar_t*)pmath_string_buffer(name))){
      pmath_unref(name);
      return TRUE;
    }
  
    pmath_unref(name);
    return FALSE;
  }
  #else
  {
    pmath_bool_t result = FALSE;
    char *str = pmath_string_to_native(name, NULL);
      
    if(str && !chdir(str)){
      result = TRUE;
    }
    
    pmath_mem_free(str);
    pmath_unref(name);
    return result;
  }
  #endif
}

PMATH_PRIVATE pmath_t builtin_directory(pmath_expr_t expr){
/* Directory()
 */
  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  expr = get_directory();
  if(!expr)
    return pmath_ref(PMATH_SYMBOL_FAILED);
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_directoryname(pmath_expr_t expr){
  pmath_string_t name, obj;
  size_t count, exprlen;
  const uint16_t *buf;
  int len, i;
  
  exprlen = pmath_expr_length(expr);
  if(exprlen < 1 || exprlen > 2){
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(name, PMATH_TYPE_STRING)){
    pmath_unref(name);
    pmath_message(NULL, "str", 2, pmath_integer_new_si(1), pmath_ref(expr));
    return expr;
  }
  
  count = 1;
  if(exprlen == 2){
    obj = pmath_expr_get_item(expr, 2);
    if(!pmath_instance_of(obj, PMATH_TYPE_INTEGER)
    || !pmath_integer_fits_ui(obj)
    || pmath_number_sign(obj) <= 0){
      pmath_unref(obj);
      pmath_message(NULL, "intpm", 2, pmath_integer_new_si(2), pmath_ref(expr));
      return expr;
    }
    
    count = pmath_integer_get_ui(obj);
    pmath_unref(obj);
  }
  
  pmath_unref(expr);
  buf = pmath_string_buffer(name);
  len = pmath_string_length(name);
  i = len;
  while(count-- > 0){
    --i;
    #ifdef PMATH_OS_WIN32
      while(i >= 0 && buf[i] != '\\' && buf[i] != '/')
        --i;
    #else
      while(i >= 0 && buf[i] != '\\' && buf[i] != '/')
        --i;
    #endif
  }
  
  if(i < 0){
    pmath_unref(name);
    return pmath_string_new(0);
  }
  
  return pmath_string_part(name, 0, i + 1);
}

PMATH_PRIVATE pmath_t builtin_setdirectory(pmath_expr_t expr){
  pmath_string_t name;
  
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_instance_of(name, PMATH_TYPE_STRING)
  || pmath_string_length(name) == 0){
    pmath_message(NULL, "fstr", 1, name);
    return expr;
  }
  
  pmath_unref(expr);
  expr = get_directory();
  
  if(!try_change_directory(pmath_ref(name))){
    pmath_message(NULL, "cdir", 1, name);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(name);
  
  name = pmath_evaluate(pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_PREPEND), 2,
    pmath_thread_local_load(PMATH_SYMBOL_DIRECTORYSTACK),
    expr));
    
  pmath_unref(pmath_thread_local_save(PMATH_SYMBOL_DIRECTORYSTACK, name));
  
  return get_directory();
}

PMATH_PRIVATE pmath_t builtin_resetdirectory(pmath_expr_t expr){
  pmath_t dirstack;
  pmath_string_t name;
  
  if(pmath_expr_length(expr) != 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  dirstack = pmath_thread_local_load(PMATH_SYMBOL_DIRECTORYSTACK);
  if(!pmath_is_expr_of(dirstack, PMATH_SYMBOL_LIST)
  || pmath_expr_length(dirstack) == 0){
    pmath_message(NULL, "dtop", 0);
    pmath_unref(dirstack);
    return get_directory();
  }
  
  name = pmath_expr_get_item(dirstack, 1);
  
  if(!try_change_directory(pmath_ref(name))){
    pmath_message(NULL, "cdir", 1, name);
    pmath_unref(dirstack);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  pmath_unref(
    pmath_thread_local_save(
      PMATH_SYMBOL_DIRECTORYSTACK, 
      pmath_expr_get_item_range(dirstack, 2, SIZE_MAX)));
  pmath_unref(dirstack);
  
  return name;
}
