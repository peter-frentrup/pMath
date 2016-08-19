#include "console.h"

#include <editline/readline.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#  define USE_WINEDITLINE 1
#else
#  define USE_WINEDITLINE 0
#endif


/* WinEditLine gives UCS-2 indices but UTF-8 text

   So `start` and `end point into _el_line_buffer, not rl_line_buffer!
 */
#if USE_WINEDITLINE
#  define EL_LINE_BUFFER _el_line_buffer
#else
#  define EL_LINE_BUFFER rl_line_buffer
#  define rl_free free
#endif



pmath_threadlock_t read_lock = NULL;

static pmath_string_t _simple_readline(const char *prompt);
static pmath_string_t _completing_readline(const char *prompt);

static pmath_bool_t initialized_auto_completion = FALSE;
static pmath_expr_t all_char_names = PMATH_STATIC_NULL;

static char **pmath_completion(const char *text, int start,  int end);

struct _rl_data_t {
  const char      *prompt;
  pmath_string_t   result;
  pmath_string_t   previous_input;
  pmath_string_t (*read_fn)(const char *prompt);
  pmath_bool_t     with_completion;
};

static void _readline_locked_callback(void *_data) {
  struct _rl_data_t *data = _data;
  
  char       **(*old_rl_attempted_completion_function)(const char *, int, int);
  const char    *old_rl_completer_word_break_characters;
  
  old_rl_attempted_completion_function   = rl_attempted_completion_function;
  old_rl_completer_word_break_characters = rl_completer_word_break_characters;
  
  if(data->with_completion) {
    rl_attempted_completion_function   = pmath_completion;
    rl_completer_word_break_characters = " \t\n\"\\'@<>=;,:.#~|&{}[]()+-*/!%";
  }
  
  data->result = data->read_fn(data->prompt);
  
  rl_attempted_completion_function   = old_rl_attempted_completion_function;
  rl_completer_word_break_characters = old_rl_completer_word_break_characters;
}

pmath_string_t readline_pmath(
  const char     *prompt,
  pmath_bool_t    with_completion,
  pmath_string_t  previous_input
) {
  struct _rl_data_t data;
  
  data.prompt = prompt;
  data.read_fn = with_completion ? _completing_readline : _simple_readline;
  data.result = PMATH_NULL;
  data.previous_input = previous_input;
  data.with_completion = with_completion;
  
  pmath_thread_call_locked(
    &read_lock,
    _readline_locked_callback,
    &data);
    
  return data.result;
}

static void _cleanup_input_cache_callback(void *dummy) {
  pmath_unref(all_char_names);
  all_char_names = PMATH_NULL;
  
  initialized_auto_completion = FALSE;
}

void cleanup_input_cache(void) {
  pmath_thread_call_locked(
    &read_lock,
    _cleanup_input_cache_callback,
    NULL);
}

static void need_auto_completion(void) {
  if(initialized_auto_completion)
    return;
    
  PMATH_RUN("Block({$NamespacePath:= {\"System`\"}}, <<AutoCompletion` )");
  
  initialized_auto_completion = TRUE;
}

static void insert_tab(void) {
#if USE_WINEDITLINE
  _el_insert_char(L"  ", 2);
#else
  rl_insert_text("  ");
#endif
}

static char **no_matches(void) {
  char **matches = malloc(1 * sizeof(char *));
  
  if(matches) {
    matches[0] = NULL;
  }
  
  return matches;
}

static char *concat2(const char *prefix, int prefix_len, const char *suffix, int suffix_len) {
  char *res = malloc(prefix_len + suffix_len + 1);
  
  if(!res)
    return NULL;
    
  memcpy(res, prefix, prefix_len);
  memcpy(res + prefix_len, suffix, suffix_len);
  res[prefix_len + suffix_len] = '\0';
  return res;
}

static char *concat3(const char *prefix, int prefix_len, const char *s, int s_len, const char *suffix, int suffix_len) {
  char *res = malloc(prefix_len + s_len + suffix_len + 1);
  
  if(!res)
    return NULL;
    
  memcpy(res, prefix, prefix_len);
  memcpy(res + prefix_len, s, s_len);
  memcpy(res + prefix_len + s_len, suffix, suffix_len);
  res[prefix_len + s_len + suffix_len] = '\0';
  return res;
}

static char **try_convert_matches(pmath_t list, const char *text, int prefix_len, const char *suffix) {
  char **matches;
  size_t len;
  size_t i;
  size_t j;
  pmath_bool_t has_exact_match;
  int suffix_len = strlen(suffix);
  
  assert(prefix_len >= 0);
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST))
    return NULL;
    
  if(pmath_expr_length(list) == 0)
    return NULL;
    
  len = pmath_expr_length(list);
  matches = malloc((len + 2) * sizeof(char *));
  
  if(!matches)
    return NULL;
    
  has_exact_match = FALSE;
  j = 0;
  for(i = 1; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(list, i);
    
    if(pmath_is_string(item)) {
      int s_len;
      char *s = pmath_string_to_utf8(item, &s_len);
      
      if(!s) {
        pmath_unref(item);
        matches[j] = NULL;
        return matches;
      }
      
      if(j == 0 && strcmp(text + prefix_len, s) == 0) {
        has_exact_match = TRUE;
      }
      
      matches[j] = concat3(text, prefix_len, s, s_len, suffix, suffix_len);
      pmath_mem_free(s);
      
      if(!matches[j]) {
        pmath_unref(item);
        return matches;
      }
      
      ++j;
    }
    
    pmath_unref(item);
  }
  
  if(!has_exact_match) {
    matches[j] = concat2(text, strlen(text), suffix, suffix_len);
    
    if(matches[j])
      ++j;
  }
  
  matches[j] = NULL;
  return matches;
}

static pmath_bool_t is_at_start(int index) {
  while(index-- > 0) {
    if(EL_LINE_BUFFER[index] > ' ' || EL_LINE_BUFFER[index] <= '\0')
      return FALSE;
  }
  
  return TRUE;
}

static pmath_expr_t get_all_char_names(void) {
  if(pmath_is_null(all_char_names)) {
    const struct pmath_named_char_t *nc_data;
    size_t                           nc_count, i;
    
    nc_data = pmath_get_char_names(&nc_count);
    
    all_char_names = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), nc_count);
    
    for(i = 0; i < nc_count; ++i) {
      all_char_names = pmath_expr_set_item(
                         all_char_names, i + 1,
                         PMATH_C_STRING(nc_data[i].name));
    }
  }
  
  return pmath_ref(all_char_names);
}

static char **complete_char_name(const char *text, int start, int end) {
  /* If read line contains a quote, all text between quotes is selected for
     completion. Need to find '\[' in strings on our own.
   */
  
  int prefix_len = 0;
  if(start >= 1 && EL_LINE_BUFFER[start - 1] == '"') {
    int i = end;
    const char *s = text + strlen(text);
    
    while(i-- > start) {
      --s;
      
      if(EL_LINE_BUFFER[i] >= 'a' && EL_LINE_BUFFER[i] <= 'z')
        continue;
        
      if(EL_LINE_BUFFER[i] >= 'A' && EL_LINE_BUFFER[i] <= 'Z')
        continue;
        
      if(EL_LINE_BUFFER[i] >= '0' && EL_LINE_BUFFER[i] <= '9')
        continue;
        
      break;
    }
    
    start = i + 1;
    prefix_len = (s + 1) - text;
  }
  
  if( start >= 2 &&
      EL_LINE_BUFFER[start - 1] == '[' &&
      EL_LINE_BUFFER[start - 2] == '\\')
  {
    pmath_t char_names;
    char **matches;
    
    char_names = get_all_char_names();
    char_names = pmath_evaluate(
                   pmath_parse_string_args(
                     "AutoCompletion`AutoCompleteOther(`1`, `2`)",
                     "(os)",
                     char_names,
                     text + prefix_len));
                     
    matches = try_convert_matches(char_names, text, prefix_len, ""); // "]"
    pmath_unref(char_names);
    return matches ? matches : no_matches();
  }
  
  return NULL;
}

static char **complete_filename(const char *text, int start, int end) {
  if(start >= 1 && EL_LINE_BUFFER[start - 1] == '"') {
    pmath_t full_filenames;
    char **matches;
    
    full_filenames = pmath_evaluate(
                       pmath_parse_string_args(
                         "AutoCompletion`AutoCompleteFullFilename(`1`.StringReplace(`2`** -> `2`)).StringReplace(`2` -> `3`)",
                         "(sss)",
                         text,
                         "\\",
                         "\\\\"));
                         
    matches = try_convert_matches(full_filenames, text, 0, "");
    pmath_unref(full_filenames);
    return matches;
  }
  
  return NULL;
}

static char **complete_name(const char *text, int start, int end) {
  if( start >= 2 &&
      EL_LINE_BUFFER[start - 1] != '<' &&
      EL_LINE_BUFFER[start - 2] != '<')
  {
    pmath_t namespaces;
    char **matches;
    
    namespaces = pmath_evaluate(
                   pmath_parse_string_args(
                     "AutoCompletion`AutoCompleteNamespaceGet(`1`)",
                     "(s)",
                     text));
                     
    matches = try_convert_matches(namespaces, text, 0, "");
    pmath_unref(namespaces);
    if(matches)
      return matches;
  }
  
  if(start == 0 || EL_LINE_BUFFER[start - 1] != '"') { // not in string
    pmath_t names;
    char **matches;
    
    names = pmath_evaluate(
              pmath_parse_string_args(
                "AutoCompletion`AutoCompleteName(Names(), `1`)",
                "(s)",
                text));
                
    matches = try_convert_matches(names, text, 0, "");
    pmath_unref(names);
    return matches;
  }
  
  return NULL;
}

static char **pmath_completion(const char *text, int start, int end) {
  char **matches;
  
  rl_attempted_completion_over = TRUE;
  
  if(start == end && is_at_start(start)) {
    insert_tab();
    return no_matches();
  }
  
  need_auto_completion();
  
  matches = complete_char_name(text, start, end);
  if(matches)
    return matches;
    
  matches = complete_filename(text, start, end);
  if(matches)
    return matches;
    
  matches = complete_name(text, start, end);
  if(matches)
    return matches;
    
  return no_matches();
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
  
  //signal(SIGINT, signal_dummy);
  s = readline(prompt);
  //signal(SIGINT, signal_handler);
  
  if(!s)
    return PMATH_NULL;
    
  result = pmath_string_from_utf8(s, -1);
  
  if(*s) {
    add_history(s);
  }
  
  rl_free(s);
  
  return result;
}
