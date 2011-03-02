#include <pmath-util/modules-private.h>

#include <pmath-core/custom.h>

#include <pmath-util/concurrency/threadlocks.h>
#include <pmath-util/hashtables-private.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <pmath-private.h>

#include <limits.h>
#include <string.h>

#ifdef PMATH_OS_WIN32
  #include <windows.h>
#elif defined(PMATH_OS_UNIX)
  #include <dlfcn.h>
#endif


// initialized in pmath_init():
PMATH_PRIVATE pmath_t _pmath_object_loadlibrary_load_message;
PMATH_PRIVATE pmath_t _pmath_object_get_load_message;

struct _module_t{
  void (*done)(void);
  
  #ifdef PMATH_OS_WIN32
    HANDLE handle;
  #elif defined(PMATH_OS_UNIX)
    void *handle;
  #endif
};

static pmath_threadlock_t all_modules_lock = PMATH_NULL;
static pmath_hashtable_t  all_modules;

static void destroy_module(void *ptr){
  struct _module_t *mod = (struct _module_t*)ptr;
  
  if(mod->done){
    mod->done();
    
    #ifdef PMATH_OS_WIN32
      FreeLibrary(mod->handle);
    #else
      dlclose(mod->handle);
    #endif
  }
  
  pmath_mem_free(mod);
}

  struct _load_info_t{
    pmath_string_t filename;
    pmath_bool_t   success;
  };
  
  static void load_callback(void *data){
    struct _load_info_t          *info = (struct _load_info_t*)data;
    struct _module_t             *mod;
    struct _pmath_object_entry_t *entry;
    pmath_custom_t                mod_obj;
    
    pmath_bool_t (*init_func)(pmath_string_t);
    void         (*done_func)(void);
    
    mod = pmath_mem_alloc(sizeof(struct _module_t));
    if(!mod)
      return;
    
    memset(mod, 0, sizeof(struct _module_t));
    
    mod_obj = pmath_custom_new(mod, destroy_module);
    if(!mod_obj)
      return;
    
    info->filename = _pmath_canonical_file_name(info->filename);
    
    if(!info->filename){
      pmath_unref(mod_obj);
      return;
    }
    
    entry = pmath_ht_search(all_modules, info->filename);
    if(entry){
      pmath_unref(mod_obj);
      info->success = TRUE;
      return;
    }
    
    entry = pmath_mem_alloc(sizeof(struct _pmath_object_entry_t));
    if(!entry){
      pmath_unref(mod_obj);
      return;
    }
    
    entry->key   = pmath_ref(info->filename);
    entry->value = pmath_ref(mod_obj);
    entry = pmath_ht_insert(all_modules, entry);
    if(entry != PMATH_NULL){
      pmath_ht_obj_class.entry_destructor(entry);
      pmath_unref(mod_obj);
      return;
    }
    
    pmath_message(PMATH_SYMBOL_LOADLIBRARY, "load", 1, pmath_ref(info->filename));
            
    #ifdef PMATH_OS_WIN32
    {
      // zero terminate
      pmath_string_t zero_filename = pmath_ref(info->filename);
      zero_filename = pmath_string_insert_latin1(zero_filename, INT_MAX, "", 1);
      
      if(zero_filename){
        mod->handle = LoadLibraryW((WCHAR*)pmath_string_buffer(zero_filename));
        if(mod->handle){
          init_func = (pmath_bool_t(*)(pmath_string_t))GetProcAddress(mod->handle, "pmath_module_init");
          done_func = (void        (*)(void))          GetProcAddress(mod->handle, "pmath_module_done");
          
          if(init_func && done_func){
            info->success = init_func(info->filename);
              
            if(info->success)
              mod->done = done_func;
          }
          
          if(!info->success){
            FreeLibrary(mod->handle);
            mod->handle = PMATH_NULL;
          }
        }
        
        pmath_unref(zero_filename);
      }
    }
    #elif defined(PMATH_OS_UNIX)
    {
      int len;
      char *fname = pmath_string_to_native(info->filename, &len);
      
      if(fname){
        mod->handle = dlopen(fname, RTLD_NOW | RTLD_LOCAL);
        
        dlerror(); /* Clear any existing error */
        
        if(mod->handle){
          init_func = (pmath_bool_t(*)(pmath_string_t))dlsym(mod->handle, "pmath_module_init");
          done_func = (void        (*)(void))          dlsym(mod->handle, "pmath_module_done");
          
          dlerror(); /* Clear any existing error */
          
          if(init_func && done_func){
            info->success = init_func(info->filename);
              
            if(info->success)
              mod->done = done_func;
          }
          
          if(!info->success){
            dlclose(mod->handle);
            dlerror(); /* Clear any existing error */
            mod->handle = PMATH_NULL;
          }
        }
        
        pmath_mem_free(fname);
      }
    }
    #endif
    
    if(!info->success){
      entry = pmath_ht_remove(all_modules, info->filename);
      assert(entry != PMATH_NULL);
      pmath_ht_obj_class.entry_destructor(entry);
    }
    pmath_unref(mod_obj);
  }

PMATH_PRIVATE pmath_bool_t _pmath_module_load(pmath_string_t filename){
  struct _load_info_t  info;
  
  info.filename = filename;
  info.success  = FALSE;
  
  if(_pmath_is_running()){
    pmath_thread_call_locked(&all_modules_lock, load_callback, &info);
  }
  
  pmath_unref(info.filename);
  return info.success;
}

PMATH_PRIVATE pmath_bool_t _pmath_modules_init(void){
  all_modules_lock = 0;
  all_modules = pmath_ht_create(&pmath_ht_obj_class, 0);
  
  return all_modules != PMATH_NULL;
}

PMATH_PRIVATE void _pmath_modules_done(void){
  pmath_ht_destroy(all_modules);
}

