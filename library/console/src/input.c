#include "console.h"

#include <editline/readline.h>
#include <stdio.h>


#if defined(WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif /* !WIN32 */


#ifndef RTLD_NOW
#  define RTLD_NOW 0
#endif
#ifndef RTLD_LOCAL
#  define RTLD_LOCAL 0
#endif

#define DL_OPEN_FLAGS RTLD_NOW|RTLD_LOCAL

#ifdef WIN32
#  define  dlopen(X,Y)    LoadLibrary((X))
#  define  dlsym(X,Y)    GetProcAddress((HINSTANCE)(X),(Y))
#  define  dlclose(X)    FreeLibrary((X))
#endif /* WIN32 */


pmath_threadlock_t read_lock = NULL;

#ifdef WIN32
#  define EDITLINE_DLL_NAME  "edit.dll"
#else
#  define EDITLINE_DLL_NAME  "libedit.so"
#endif

static pmath_bool_t initialized_editline = FALSE;
static void    *editline_dll = NULL;
static char  *(*sym_readline)(const char *prompt) = NULL;
static char  *(*sym_add_history)(char *line) = NULL;
static void   (*sym_rl_free)(void *mem) = NULL;
static char **(*sym_rl_completion_matches)(const char *text, char *entry_func(const char *, int)) = NULL;

static rl_completion_func_t    *(*sym_rl_attempted_completion_function) = NULL;
static rl_compentryfree_func_t *(*sym_rl_user_completion_entry_free_function) = NULL;

#define SEARCH_EDITLINE_SYMBOL(name) \
  do{                                                                              \
    if(!(sym_ ## name = (void*)dlsym(editline_dll, #name ))) {                     \
      fprintf(stderr, "Failed to load editline library: no symbol %s.\n", #name ); \
      goto FAIL;                                                                   \
    }                                                                              \
  } while(0)

pmath_bool_t search_editline(void) {
  if(!initialized_editline) {
    initialized_editline = TRUE;
    
    editline_dll = dlopen(EDITLINE_DLL_NAME, DL_OPEN_FLAGS);
    
    if(!editline_dll) {
      fprintf(stderr, "Failed to load editline library.\n");
      goto FAIL;
    }
      
    SEARCH_EDITLINE_SYMBOL( readline );
    SEARCH_EDITLINE_SYMBOL( add_history );
    SEARCH_EDITLINE_SYMBOL( rl_free );
    SEARCH_EDITLINE_SYMBOL( rl_completion_matches );
    
    SEARCH_EDITLINE_SYMBOL( rl_attempted_completion_function );
    SEARCH_EDITLINE_SYMBOL( rl_user_completion_entry_free_function );
    
    return TRUE;
    
  FAIL:
    cleanup_input();
    initialized_editline = TRUE;
    return FALSE;
  }
  
  return editline_dll != NULL;
}

void cleanup_input(void) {
  if(editline_dll) {
    initialized_editline = FALSE;
    
    sym_readline              = NULL;
    sym_add_history           = NULL;
    sym_rl_free               = NULL;
    sym_rl_completion_matches = NULL;
    
    sym_rl_attempted_completion_function = NULL;
    sym_rl_user_completion_entry_free_function = NULL;
    
    dlclose(editline_dll);
    editline_dll = NULL;
  }
}


static pmath_string_t _simple_readline(const char *prompt);
static pmath_string_t _completing_readline(const char *prompt);


static void my_free(void *p);
static char **my_completion(const char *text , int start,  int end);
static char *my_generator(const char *text, int state);
static char *my_empty_generator(const char *text, int state);

struct _rl_data_t {
  const char      *prompt;
  pmath_string_t   result;
  pmath_string_t (*read_fn)(const char *prompt);
  pmath_bool_t     with_completion;
};

static void _readline_locked_callback(void *_data) {
  struct _rl_data_t *data = _data;
  
  rl_completion_func_t    *old_rl_attempted_completion_function;
  rl_completion_func_t    *new_rl_attempted_completion_function;
  rl_compentryfree_func_t *old_rl_user_completion_entry_free_function;
  rl_compentryfree_func_t *new_rl_user_completion_entry_free_function;
  
  if(data->with_completion && search_editline()) {
    old_rl_attempted_completion_function        = *sym_rl_attempted_completion_function;
    *sym_rl_attempted_completion_function       = my_completion;
    old_rl_user_completion_entry_free_function  = *sym_rl_user_completion_entry_free_function;
    *sym_rl_user_completion_entry_free_function = my_free;
  }
  
  data->result = data->read_fn(data->prompt);
  
  if(data->with_completion && search_editline()) {
    *sym_rl_attempted_completion_function       = old_rl_attempted_completion_function;
    *sym_rl_user_completion_entry_free_function = old_rl_user_completion_entry_free_function;
  }
  //rl_attempted_completion_function       = old_rl_attempted_completion_function;
  //rl_user_completion_entry_free_function = old_rl_user_completion_entry_free_function;
}

pmath_string_t readline_pmath(
  const char   *prompt,
  pmath_bool_t  with_completion
) {
  struct _rl_data_t data;
  
  data.prompt = prompt;
  data.read_fn = with_completion ? _completing_readline : _simple_readline;
  data.result = PMATH_NULL;
  data.with_completion = with_completion;
  
  pmath_thread_call_locked(
    &read_lock,
    _readline_locked_callback,
    &data);
    
  return data.result;
}

static char *cmd [] = { "hello", "world", "hell" , "word", "quit", " " };
static void my_free(void *p) {
  free(p);
}

static char **my_completion(const char *text , int start,  int end) {
//  char **matches;
  
  if(start == 0) {
    return sym_rl_completion_matches(text, my_generator);
  }
  
  return sym_rl_completion_matches(text, my_empty_generator);
//return NULL;
//  matches = malloc(sizeof(char *));
//  
//  if(matches)
//    matches[0] = NULL;
//    
//  return matches;
  
  //rl_bind_key('\t', rl_abort);
}

static char *my_generator(const char *text, int state) {
  static int list_index, len;
  char *name;
  
  if(!state) {
    list_index = 0;
    len = strlen(text);
  }
  
  while((name = cmd[list_index])) {
    list_index++;
    
    if(strncmp(name, text, len) == 0) {
      int name_len = strlen(name);
      char *name_copy = malloc(name_len + 1);
      
      if(name_copy) 
        strcpy(name_copy, name);
        
      return name_copy;
    }
  }
  
  return NULL;
}


static char *my_empty_generator(const char *text, int state) {
  return NULL;
}


static pmath_string_t _simple_readline(const char *prompt) {
  pmath_string_t result = PMATH_NULL;
  char buf[512];
  
  printf("%s", prompt);
  
  while(fgets(buf, sizeof(buf), stdin) != NULL) {
    int len = strlen(buf);
    
    if(buf[len - 1] == '\n') {
      if(len == 1)
        return pmath_string_new(0);
        
      return pmath_string_concat(result, pmath_string_from_native(buf, len - 1));
    }
    
    result = pmath_string_concat(result, pmath_string_from_native(buf, len));
  }
  
  return result;
}

static pmath_string_t _completing_readline(const char *prompt) {
  char *s;
  pmath_string_t result;
  
  if(!search_editline())
    return _simple_readline(prompt);
    
  //signal(SIGINT, signal_dummy);
  s = sym_readline(prompt);
  //signal(SIGINT, signal_handler);
  
  if(!s)
    return PMATH_NULL;
    
  result = pmath_string_from_utf8(s, -1);
  
  if(*s) {
    sym_add_history(s);
  }
  
  sym_rl_free(s);
  
  return result;
}
