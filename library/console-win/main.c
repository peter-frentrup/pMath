#define _CRT_SECURE_NO_WARNINGS

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hyper-console.h>
#include <pmath.h>

#include <math.h>

#ifdef pmath_debug_print_stack
#  undef pmath_debug_print_stack
#endif


#ifdef MIN
# undef MIN
#endif
#define MIN(A, B)  ((A) < (B) ? (A) : (B))

#ifdef MAX
# undef MAX
#endif
#define MAX(A, B)  ((A) > (B) ? (A) : (B))

static void os_init(void);

#define PMATH_SYSTEM_CONSOLE_SYMBOL_X( sym ) X( pmath_System_Console_ ## sym, "System`Console`" #sym )
#define PMATH_SYSTEM_SYMBOL_X( sym )         X( pmath_System_ ## sym        , "System`" #sym )
#define PMATH_SYSTEM_DOLLAR_SYMBOL_X( sym )  X( pmath_System_Dollar ## sym  , "System`$" #sym )
#define PMATH_SYMBOLS_X                      \
  PMATH_SYSTEM_DOLLAR_SYMBOL_X( PageWidth ) \
  PMATH_SYSTEM_SYMBOL_X( Background      ) \
  PMATH_SYSTEM_SYMBOL_X( BaseStyle       ) \
  PMATH_SYSTEM_SYMBOL_X( Bold            ) \
  PMATH_SYSTEM_SYMBOL_X( BoxData         ) \
  PMATH_SYSTEM_SYMBOL_X( Button          ) \
  PMATH_SYSTEM_SYMBOL_X( ButtonBox       ) \
  PMATH_SYSTEM_SYMBOL_X( ButtonFunction  ) \
  PMATH_SYSTEM_SYMBOL_X( Dialog          ) \
  PMATH_SYSTEM_SYMBOL_X( FontColor       ) \
  PMATH_SYSTEM_SYMBOL_X( FontSlant       ) \
  PMATH_SYSTEM_SYMBOL_X( FontWeight      ) \
  PMATH_SYSTEM_SYMBOL_X( Function        ) \
  PMATH_SYSTEM_SYMBOL_X( GrayLevel       ) \
  PMATH_SYSTEM_SYMBOL_X( Highlighted     ) \
  PMATH_SYSTEM_SYMBOL_X( HoldComplete    ) \
  PMATH_SYSTEM_SYMBOL_X( Interrupt       ) \
  PMATH_SYSTEM_SYMBOL_X( Italic          ) \
  PMATH_SYSTEM_SYMBOL_X( List            ) \
  PMATH_SYSTEM_SYMBOL_X( MakeExpression  ) \
  PMATH_SYSTEM_SYMBOL_X( Plain           ) \
  PMATH_SYSTEM_SYMBOL_X( Quit            ) \
  PMATH_SYSTEM_SYMBOL_X( RawBoxes        ) \
  PMATH_SYSTEM_SYMBOL_X( RGBColor        ) \
  PMATH_SYSTEM_SYMBOL_X( Return          ) \
  PMATH_SYSTEM_SYMBOL_X( Row             ) \
  PMATH_SYSTEM_SYMBOL_X( Section         ) \
  PMATH_SYSTEM_SYMBOL_X( SectionPrint    ) \
  PMATH_SYSTEM_SYMBOL_X( Sequence        ) \
  PMATH_SYSTEM_SYMBOL_X( ShowDefinition  ) \
  PMATH_SYSTEM_SYMBOL_X( Style           ) \
  PMATH_SYSTEM_SYMBOL_X( StyleBox        ) \
  PMATH_SYSTEM_SYMBOL_X( Tooltip         ) \
  PMATH_SYSTEM_SYMBOL_X( TooltipBox      ) \
  PMATH_SYSTEM_CONSOLE_SYMBOL_X( KnownConsoleStyles )

#define X( SYM, NAME )  static pmath_symbol_t SYM = PMATH_STATIC_NULL;
  PMATH_SYMBOLS_X
#undef X

#ifdef PMATH_OS_WIN32
#  include <io.h>
#  include <fcntl.h>

#  define NOGDI
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

typedef HANDLE sem_t;

static int sem_init(sem_t *sem, int pshared, unsigned int value) {
  *sem = CreateSemaphore(0, value, 0x7FFFFFFF, 0);
  return *sem == 0 ? -1 : 0;
}

static int sem_destroy(sem_t *sem) {
  return CloseHandle(*sem) ? 0 : -1;
}

static int sem_wait(sem_t *sem) {
  errno = 0;
  return WaitForSingleObject(*sem, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

static int sem_post(sem_t *sem) {
  return ReleaseSemaphore(*sem, 1, 0) ? 0 : -1;
}

static void os_init(void) {
  HMODULE kernel32;
  
  // do not show message boxes on LoadLibrary errors:
  SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
  
  // remove current directory from dll search path:
  kernel32 = GetModuleHandleW(L"Kernel32");
  if(kernel32) {
    BOOL (WINAPI * SetDllDirectoryW_ptr)(const WCHAR *);
    SetDllDirectoryW_ptr = (BOOL (WINAPI *)(const WCHAR *))
                           GetProcAddress(kernel32, "SetDllDirectoryW");
                           
    if(SetDllDirectoryW_ptr)
      SetDllDirectoryW_ptr(L"");
  }
}
#elif defined(PMATH_OS_UNIX)
#  include <unistd.h>
#  include <semaphore.h>

static void os_init(void) {

}
#endif

static struct hyper_console_history_t *history = NULL;

static int console_width = 80;
static volatile int dialog_depth = 0;
static volatile int quit_result = 0;
static volatile pmath_bool_t quitting = FALSE;
static volatile pmath_bool_t show_mem_stats = TRUE;

static sem_t interrupt_semaphore;
static volatile pmath_messages_t main_mq;
static pmath_atomic_t main_mq_lock = PMATH_ATOMIC_STATIC_INIT;

static void init_console_width(void);

static pmath_messages_t get_main_mq(void) {
  pmath_messages_t mq;
  
  pmath_atomic_lock(&main_mq_lock);
  {
    mq = pmath_ref(main_mq);
  }
  pmath_atomic_unlock(&main_mq_lock);
  
  return mq;
}

// Reads a line from stdin without the ending "\n".
static pmath_string_t readline_simple(void) {
  struct hyper_console_settings_t settings;
  wchar_t *str;
  
  memset(&settings, 0, sizeof(settings));
  settings.size = sizeof(settings);
  settings.default_input = L"";
  settings.history = history;
  //settings.need_more_input_predicate = need_more_input_predicate;
  //settings.auto_completion = auto_completion;
  //settings.line_continuation_prompt = L"...>";
  
  str = hyper_console_readline(&settings);
  if(str) {
    pmath_string_t result = pmath_string_insert_ucs2(PMATH_NULL, 0, str, -1);
    hyper_console_free_memory(str);
    return result;
  }
  
  return PMATH_NULL;
}


static pmath_string_t need_more_pmath_input_read(void *data) {
  pmath_bool_t *need_more_input = data;
  *need_more_input = TRUE;
  return PMATH_NULL;
}

static BOOL need_more_pmath_input(void *dummy, const wchar_t *buffer, int len, int cursor_pos) {
  pmath_string_t code = pmath_string_insert_ucs2(PMATH_NULL, 0, buffer, len);
  pmath_span_array_t *spans;
  pmath_bool_t need_more_input = FALSE;
  
  spans = pmath_spans_from_string(
            &code,
            need_more_pmath_input_read,
            NULL,
            NULL,
            NULL,
            &need_more_input);
            
  pmath_span_array_free(spans);
  pmath_unref(code);
  return need_more_input;
}

static void expand_spans_selection(pmath_span_array_t *spans, int *start, int *end) {
  int length;
  int s;
  int e;
  
  assert(start != NULL);
  assert(end != NULL);
  
  if(*start > *end) {
    int tmp = *start;
    *start = *end;
    *end = tmp;
  }
  
  if(!spans)
    return;
    
  length = pmath_span_array_length(spans);
  if(*start <= 0 && *end >= length)
    return;
    
  s = *start;
  while(s > 0 && !pmath_span_array_is_token_end(spans, s - 1))
    --s;
    
  e = MIN(*start + 1, length);
  while(e < length && !pmath_span_array_is_token_end(spans, e - 1))
    ++e;
    
  if(e >= *end && (s != *start || e != *end)) {
    *start = s;
    *end = e;
    return;
  }
  
  for(; s >= 0; --s) {
    pmath_span_t *span = pmath_span_at(spans, s);
    pmath_bool_t found = FALSE;
    
    while(span) {
      int after = 1 + pmath_span_end(span);
      if(after < *end)
        break;
        
      if(after == *end && s == *start)
        break;
        
      found = TRUE;
      e = after;
      span = pmath_span_next(span);
    }
    
    if(found) {
      *start = s;
      *end = e;
      return;
    }
  }
  
  *start = 0;
  *end = length;
}

static int selection_shrink_pos = -1;
static void expand_pmath_selection(void) {
  int length;
  const wchar_t *buffer = hyper_console_get_current_input(&length);
  pmath_t code = pmath_string_insert_ucs2(PMATH_NULL, 0, buffer, length);
  pmath_span_array_t *spans = pmath_spans_from_string(&code, NULL, NULL, NULL, NULL, NULL);
  
  if(spans) {
    int pos, anchor;
    int start, end;
    hyper_console_get_current_selection(&pos, &anchor);
    start = MIN(pos, anchor);
    end = MAX(pos, anchor);
    if(selection_shrink_pos < start || end < selection_shrink_pos) {
      selection_shrink_pos = pos;
    }
    expand_spans_selection(spans, &start, &end);
    hyper_console_set_current_selection(end, start);
    pmath_span_array_free(spans);
  }
  
  pmath_unref(code);
}

static void shrink_pmath_selection(void) {
  int length;
  const wchar_t *buffer = hyper_console_get_current_input(&length);
  pmath_t code = pmath_string_insert_ucs2(PMATH_NULL, 0, buffer, length);
  pmath_span_array_t *spans = pmath_spans_from_string(&code, NULL, NULL, NULL, NULL, NULL);
  
  if(spans) {
    int pos, anchor;
    int start, end;
    hyper_console_get_current_selection(&pos, &anchor);
    start = MIN(pos, anchor);
    end = MAX(pos, anchor);
    if(selection_shrink_pos < start || end < selection_shrink_pos) {
      selection_shrink_pos = pos;
    }
    pos = anchor = selection_shrink_pos;
    for(;;) {
      int next_start = pos;
      int next_end = anchor;
      expand_spans_selection(spans, &next_start, &next_end);
      if(next_start <= start && next_end >= end)
        break;
      
      pos = next_start;
      anchor = next_end;
    }
    hyper_console_set_current_selection(pos, anchor);
    pmath_span_array_free(spans);
  }
  
  pmath_unref(code);
}

static int find_char_name_start(const wchar_t *buffer, int pos) {
  for(; pos > 0; --pos) {
    if(buffer[pos - 1] >= 'a' && buffer[pos - 1] <= 'z')
      continue;
    if(buffer[pos - 1] >= 'A' && buffer[pos - 1] <= 'Z')
      continue;
    if(buffer[pos - 1] >= '0' && buffer[pos - 1] <= '9')
      continue;
    break;
  }
  
  if(pos >= 2 && buffer[pos - 1] == '[' && buffer[pos - 2] == '\\')
    return pos;
    
  return -1;
}

static int find_char_name_end(const wchar_t *buffer, int pos, int len) {
  for(; pos < len; ++pos) {
    if(buffer[pos] >= 'a' && buffer[pos] <= 'z')
      continue;
    if(buffer[pos] >= 'A' && buffer[pos] <= 'Z')
      continue;
    if(buffer[pos] >= '0' && buffer[pos] <= '9')
      continue;
    break;
  }
  
  //if(pos + 1 < len && buffer[pos] == L']')
  //  return pos + 1;
  
  return pos;
}

static int find_name_start(const wchar_t *buffer, int pos) {
  while(pos > 0 && (pmath_char_is_digit(buffer[pos - 1]) || pmath_char_is_name(buffer[pos - 1])))
    --pos;
  
  return pos;
}

static int find_name_end(const wchar_t *buffer, int pos, int len) {
  while(pos < len && (pmath_char_is_digit(buffer[pos]) || pmath_char_is_name(buffer[pos])))
    ++pos;
  
  return pos;
}

static int find_string_start(const wchar_t *buffer, int pos) {
  pmath_bool_t is_in_string = FALSE;
  int i;
  int last_string_start = -1;
  for(i = 0; i < pos; ++i) {
    if(buffer[i] == L'"') {
      is_in_string = !is_in_string;
      if(is_in_string)
        last_string_start = i;
    }
    else if(buffer[i] == L'\\') {
      ++i;
    }
  }
  
  if(is_in_string)
    return last_string_start;
  return -1;
}

static int find_string_end(const wchar_t *buffer, int start, int len) {
  if(buffer[start] == L'"')
    ++start;
    
  while(start < len) {
    if(buffer[start] == L'"')
      return start + 1;
      
    if(buffer[start] == L'\\') {
      ++start;
      if(start == len)
        return len;
    }
    ++start;
  }
  
  return start;
}

static pmath_atomic_t ac_lock = PMATH_ATOMIC_STATIC_INIT;
static pmath_bool_t initialized_auto_completion = FALSE;
static pmath_expr_t all_char_names = PMATH_STATIC_NULL;

void cleanup_input_cache(void) {
  pmath_atomic_lock(&ac_lock);
  {
    pmath_unref(all_char_names);
    all_char_names = PMATH_NULL;
    
    initialized_auto_completion = FALSE;
  }
  pmath_atomic_unlock(&ac_lock);
}

static void need_auto_completion(void) {
  pmath_bool_t init;
  pmath_atomic_lock(&ac_lock);
  {
    init = initialized_auto_completion;
  }
  pmath_atomic_unlock(&ac_lock);
  
  if(init)
    return;
    
  PMATH_RUN("Block({$NamespacePath:= {\"System`\"}}, <<AutoCompletion` )");
  
  pmath_atomic_lock(&ac_lock);
  {
    initialized_auto_completion = TRUE;
  }
  pmath_atomic_unlock(&ac_lock);
}

static pmath_expr_t get_all_char_names(void) {
  if(pmath_is_null(all_char_names)) {
    const struct pmath_named_char_t *nc_data;
    size_t                           nc_count, i;
    
    nc_data = pmath_get_char_names(&nc_count);
    
    all_char_names = pmath_expr_new(pmath_ref(pmath_System_List), nc_count);
    
    for(i = 0; i < nc_count; ++i) {
      all_char_names = pmath_expr_set_item(
                         all_char_names, i + 1,
                         PMATH_C_STRING(nc_data[i].name));
    }
  }
  
  return pmath_ref(all_char_names);
}

static wchar_t *hc_concat3(const wchar_t *prefix, int prefix_len, const wchar_t *s, int s_len, const wchar_t *suffix, int suffix_len) {
  wchar_t *res = hyper_console_allocate_memory((prefix_len + s_len + suffix_len + 1) * sizeof(wchar_t));
  if(!res)
    return NULL;
    
  memcpy(res, prefix, prefix_len * sizeof(wchar_t));
  memcpy(res + prefix_len, s, s_len * sizeof(wchar_t));
  memcpy(res + prefix_len + s_len, suffix, suffix_len * sizeof(wchar_t));
  res[prefix_len + s_len + suffix_len] = '\0';
  return res;
}

static wchar_t **try_convert_matches(pmath_t list, const wchar_t *prefix, int prefix_len, const wchar_t *suffix) {
  wchar_t **matches;
  size_t len;
  size_t i;
  size_t j;
  int suffix_len = (int)wcslen(suffix);
  
  assert(prefix_len >= 0);
  
  if(!pmath_is_expr_of(list, pmath_System_List))
    return NULL;
    
  if(pmath_expr_length(list) == 0)
    return NULL;
    
  len = pmath_expr_length(list);
  matches = hyper_console_allocate_memory((len + 1) * sizeof(wchar_t *));
  if(!matches)
    return NULL;
    
  j = 0;
  for(i = 1; i <= len; ++i) {
    pmath_t item = pmath_expr_get_item(list, i);
    
    if(pmath_is_string(item)) {
      int s_len = pmath_string_length(item);
      const wchar_t *s = pmath_string_buffer(&item);
      
      matches[j] = hc_concat3(prefix, prefix_len, s, s_len, suffix, suffix_len);
      if(!matches[j]) {
        pmath_unref(item);
        return matches;
      }
      
      ++j;
    }
    
    pmath_unref(item);
  }
  
  matches[j] = NULL;
  return matches;
}

static wchar_t **auto_complete_pmath(void *context, const wchar_t *buffer, int len, int cursor_pos, int *completion_start, int *completion_end) {
  pmath_string_t code;
  pmath_span_array_t *spans;
  int token_start;
  int token_end;
  pmath_token_t token;
  
  need_auto_completion();
  
  token_start = find_char_name_start(buffer, cursor_pos);
  if(token_start >= 2 && token_start <= cursor_pos) {
    pmath_t char_names;
    wchar_t **matches;
    token_end = find_char_name_end(buffer, cursor_pos, len);
    if(token_end < len && buffer[token_end] == L']')
      ++token_end;
      
    char_names = get_all_char_names();
    char_names = pmath_evaluate(
                   pmath_parse_string_args(
                     "AutoCompletion`AutoCompleteOther(`1`, `2`)",
                     "(oo)",
                     char_names,
                     pmath_string_insert_ucs2(PMATH_NULL, 0, buffer + token_start, cursor_pos - token_start)));
                     
    matches = try_convert_matches(char_names, L"\\[", 2, L"]");
    pmath_unref(char_names);
    *completion_start = token_start - 2;
    *completion_end = token_end;
    return matches;
  }
  
  token_start = find_string_start(buffer, cursor_pos);
  if(token_start >= 0) {
    pmath_string_t escaped_string_until_cursor;
    pmath_t results;
    wchar_t **matches;
    int backslash = cursor_pos;
    while(backslash > token_start && buffer[backslash - 1] == '\\')
      --backslash;
      
    token_end = find_string_end(buffer, backslash, len);
    
    escaped_string_until_cursor = pmath_string_insert_ucs2(PMATH_NULL, 0, buffer + token_start, cursor_pos - token_start);
    if((cursor_pos - backslash) % 2 == 1)
      escaped_string_until_cursor = pmath_string_insert_latin1(escaped_string_until_cursor, INT_MAX, "\\\"", 2);
    else
      escaped_string_until_cursor = pmath_string_insert_latin1(escaped_string_until_cursor, INT_MAX, "\"", 1);
      
    results = pmath_evaluate(
                pmath_parse_string_args(
                  "Try("
                  "  AutoCompletion`AutoCompleteFullFilename("
                  "    ToExpression(`1`)"
                  "  ).Map("
                  "    #.InputForm.ToString.StringTake(2..-2) &"
                  "  )"
                  ")",
                  "(o)",
                  escaped_string_until_cursor));
                  
    matches = try_convert_matches(results, L"", 0, L"");
    pmath_unref(results);
    *completion_start = token_start + 1;
    *completion_end = token_end - 1;
    return matches;
  }
  
  code = pmath_string_insert_ucs2(PMATH_NULL, 0, buffer, len);
  spans = pmath_spans_from_string(&code, NULL, NULL, NULL, NULL, NULL);
  token_start = cursor_pos;
  if(token_start > 0)
    --token_start;
  while(token_start > 0 && !pmath_span_array_is_token_end(spans, token_start - 1))
    --token_start;
    
  token_end = cursor_pos;
  if(token_end > 0)
    --token_end;
  while(token_end < len && !pmath_span_array_is_token_end(spans, token_end))
    ++token_end;
    
  if(token_end < len)
    ++token_end;
    
  if(token_start >= 2 && buffer[token_start - 1] == '<' && buffer[token_start - 2] == '<') {
    wchar_t **matches;
    pmath_string_t text = pmath_string_part(code, token_start, cursor_pos - token_start); // frees `code`
    pmath_t namespaces = pmath_evaluate(
                           pmath_parse_string_args(
                             "AutoCompletion`AutoCompleteNamespaceGet(`1`)",
                             "(o)",
                             text));
                             
    matches = try_convert_matches(namespaces, L"", 0, L"");
    pmath_unref(namespaces);
    *completion_start = token_start;
    *completion_end = token_end;
    pmath_span_array_free(spans);
    return matches;
  }
  
  token = pmath_token_analyse(buffer + token_start, token_end - token_start, NULL);
  if(token == PMATH_TOK_NAME) {
    wchar_t **matches;
    pmath_string_t text = pmath_string_part(code, token_start, cursor_pos - token_start); // frees `code`
    pmath_t names = pmath_evaluate(
                      pmath_parse_string_args(
                        "AutoCompletion`AutoCompleteName(Names(), `1`)",
                        "(o)",
                        text));
                        
    matches = try_convert_matches(names, L"", 0, L"");
    pmath_unref(names);
    *completion_start = token_start;
    *completion_end = token_end;
    pmath_span_array_free(spans);
    return matches;
  }
  
  pmath_span_array_free(spans);
  pmath_unref(code);
  return NULL;
}

static BOOL key_event_filter_for_pmath(void *context, const KEY_EVENT_RECORD *er) {
  pmath_bool_t ctrl_pressed = (er->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
  pmath_bool_t alt_pressed = (er->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
  pmath_bool_t shift_pressed = (er->dwControlKeyState & SHIFT_PRESSED) != 0;
  
  if(er->bKeyDown) {
    switch(er->wVirtualKeyCode) {
      case VK_F1:
        {
          int pos, anchor;
          int start, end;
          int length;
          const wchar_t *buffer = hyper_console_get_current_input(&length);
          hyper_console_get_current_selection(&pos, &anchor);
          
          if(pos < anchor) {
            start = pos;
            end = anchor;
          }
          else if(anchor < pos) {
            start = anchor;
            end = pos;
          }
          else {
            start = find_name_start(buffer, pos);
            end = find_name_end(buffer, pos, length);
          }
          
          if(start < end) {
            PMATH_RUN_ARGS("System`ShowDefinition(`1`)", "(U#)", buffer + start, end - start);
          }
          else
            PMATH_RUN("System`Console`PrintHelpMessage()");
        }
        return TRUE;
        
      case VK_OEM_PERIOD:
        if(ctrl_pressed && !alt_pressed && !shift_pressed) {
          expand_pmath_selection();
          return TRUE;
        }
        if(ctrl_pressed && shift_pressed && !alt_pressed) {
          shrink_pmath_selection();
          return TRUE;
        }
        break;
    }
  }
  return FALSE;
}

static BOOL mark_mode_key_event_filter_for_pmath(void *context, const KEY_EVENT_RECORD *er) {
  if(er->bKeyDown) {
    switch(er->wVirtualKeyCode) {
      case VK_F1: {
          int line_len;
          int pos;
          wchar_t *mark_line = hyper_console_get_mark_mode_line(&line_len, &pos);
          if(mark_line) {
            int start = find_name_start(mark_line, pos);
            int end   = find_name_end(  mark_line, pos, line_len);
            
            if(start < end) {
              PMATH_RUN_ARGS("System`ShowDefinition(`1`)", "(U#)", mark_line + start, end - start);
            }
            else
              PMATH_RUN("System`Console`PrintHelpMessage()");
            
            hyper_console_free_memory(mark_line);
          }
        } return TRUE;
    }
  }
  return FALSE;
}

// Reads a line from stdin without the ending "\n".
static pmath_string_t readline_pmath(const wchar_t *continuation_prompt) {
  struct hyper_console_settings_t settings;
  wchar_t *str;
  
  memset(&settings, 0, sizeof(settings));
  settings.size = sizeof(settings);
  settings.flags = HYPER_CONSOLE_FLAGS_MULTILINE;
  settings.default_input = L"";
  settings.history = history;
  settings.need_more_input_predicate = need_more_pmath_input;
  settings.auto_completion = auto_complete_pmath;
  settings.line_continuation_prompt = continuation_prompt;
  settings.key_event_filter = key_event_filter_for_pmath;
  settings.mark_mode_key_event_filter = mark_mode_key_event_filter_for_pmath;
  settings.tab_width = 4;
  settings.first_tab_column = (int)wcslen(continuation_prompt);
  
  str = hyper_console_readline(&settings);
  if(str) {
    pmath_string_t result = pmath_string_insert_ucs2(PMATH_NULL, 0, str, -1);
    hyper_console_free_memory(str);
    return result;
  }
  
  return PMATH_NULL;
}

//{ Styled output ...

#define COLOR_MODE_MASK     0xFF000000u
#define COLOR_VALUE_MASK    0x00FFFFFFu
#define COLOR_UNDEFINED     0xFFFFFFFFu
#define COLOR_MODE_DEFAULT  0x00000000u
#define COLOR_MODE_8        0x01000000u
#define COLOR_MODE_16       0x02000000u
#define COLOR_MODE_256      0x03000000u
#define COLOR_MODE_RGB24    0x04000000u

struct style_context_t {
  unsigned bold : 1;
  unsigned italic : 1;
  unsigned fg_color;
  unsigned bg_color;
};

#define MAX_STYLE_DEPTH  16

struct styled_writer_info_t {
  pmath_t  current_hyperlink_obj;
  HFONT    cached_console_font;
  unsigned old_console_mode;
  struct style_context_t current_style[MAX_STYLE_DEPTH];
  unsigned style_depth;
  unsigned raw_boxes_write_depth;
  unsigned formatting_allow_raw_boxes : 1;
  unsigned formatting_is_inside_string : 1;
  unsigned no_font_available : 1;
  unsigned support_ansi_escape_codes : 1;
};

static int bytes_since_last_abortcheck = 0;
const int ABORT_CHECK_BYTE_COUNT = 100;


static int get_default_output_color(void);
static void set_output_color(int color);
static void init_terminal_capabilities(struct styled_writer_info_t *sw);
static void reset_terminal_capabilities(struct styled_writer_info_t *sw);
static void write_style_changes(
  struct styled_writer_info_t *sw, 
  const struct style_context_t *old_style,
  const struct style_context_t *new_style);
static void concat_to_string(void *user, const uint16_t *data, int len);

static pmath_bool_t is_helpline_token(pmath_string_t str);

static pmath_string_t action_to_input_string(pmath_t action);
static pmath_t find_button_function(pmath_expr_t expr, size_t first_option);
static pmath_t button_function_to_action(pmath_t func);
static void pre_write_button(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void pre_write_button_box(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void pre_write_highlighted(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void pre_write_stylebox_or_style(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void post_write_highlighted(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void post_write_stylebox_or_style(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options);
static void styled_pre_write(void *user, pmath_t obj, pmath_write_options_t options);
static void styled_post_write(void *user, pmath_t obj, pmath_write_options_t options);
static void styled_write(void *user, const uint16_t *data, int len);

static pmath_bool_t button_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t buttonbox_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t style_wrapper_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t rawboxes_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t string_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info);
static pmath_bool_t styled_formatter(void *user, pmath_t obj, struct pmath_write_ex_t *info);

static void convert_style_directive(struct style_context_t *context, pmath_t directive, int max_depth);
static double convert_to_double(pmath_t obj, double def); // obj will be freed
static unsigned decode_color(pmath_t color); // will be freed
static void convert_style_directive_Background(struct style_context_t *context, pmath_t color);
static void convert_style_directive_FontColor(struct style_context_t *context, pmath_t color);
static void convert_style_directive_FontSlant(struct style_context_t *context, pmath_t slant);
static void convert_style_directive_FontWeight(struct style_context_t *context, pmath_t weight);

static pmath_bool_t styled_can_write_unicode(void *user, const uint16_t *str, int len);


static void styled_write(void *user, const uint16_t *data, int len) {
  struct styled_writer_info_t *info = user;
  int oldmode;
  
  if(bytes_since_last_abortcheck + len >= ABORT_CHECK_BYTE_COUNT) {
    if(pmath_aborting()) {
      bytes_since_last_abortcheck = ABORT_CHECK_BYTE_COUNT;
      return;
    }
    else
      bytes_since_last_abortcheck = 0;
  }
  else
    bytes_since_last_abortcheck += len;
    
  //pmath_cstr_writer_info_t info;
  //info._pmath_write_cstr = ... some function doing "fwrite(cstr, 1, strlen(cstr), stdout)"
  //info.user = stdout; // will be first argument of my_utf8_output
  //pmath_native_writer(&info, data, len);
  
  fflush(stdout);
  oldmode = _setmode(_fileno(stdout), _O_U8TEXT);
  /* Note that printf and other char* functions do not work at all in _O_U8TEXT mode. */
  
  /* fwrite works with MSVC, but not with Mingw */
  //fwrite(data, 2, len, stdout);
  
  while(len > 0) {
    int next = 0;
    for(; next < len; ++next) {
      uint16_t ch = data[next];
      if(ch < ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
        break;
      }
    }
    
    fwprintf(stdout, L"%.*s", next, data);
    
    if(next < len)
      ++next;
    
    data += next;
    len  -= next;
  }
  
  fflush(stdout);
  _setmode(_fileno(stdout), oldmode);
}

static int get_default_output_color(void) {
  HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  memset(&csbi, 0, sizeof(csbi));
  
  if(!GetConsoleScreenBufferInfo(stdout_handle, &csbi))
    return -1;
    
  return csbi.wAttributes;
}

static void set_output_color(int color) {
  if(color < 0)
    return;
    
  {
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    fflush(stdout);
    SetConsoleTextAttribute(stdout_handle, (WORD)color);
  }
}

static void init_terminal_capabilities(struct styled_writer_info_t *sw) {
  HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if(output_handle == INVALID_HANDLE_VALUE) {
    sw->support_ansi_escape_codes = 0;
    return;
  }
  
  DWORD mode;
  if(GetConsoleMode(output_handle, &mode)) {
    sw->old_console_mode = mode;
    
    DWORD new_mode = mode | (ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
    if(SetConsoleMode(output_handle, new_mode)) {
      sw->support_ansi_escape_codes = 1;
    }
    else {
      sw->support_ansi_escape_codes = 0;
    }
  }
  else {
    sw->support_ansi_escape_codes = 0;
  }
  
  int col = get_default_output_color();
  if(col >= 0) {
    sw->current_style[0].fg_color = COLOR_MODE_16 | (col & 0x0F);
    sw->current_style[0].bg_color = COLOR_MODE_16 | ((col & 0xF0) >> 4);
  }
}

static void reset_terminal_capabilities(struct styled_writer_info_t *sw) {
  HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if(output_handle == INVALID_HANDLE_VALUE)
    return;
  
  SetConsoleMode(output_handle, sw->old_console_mode);
}

static unsigned squared_color_distance(unsigned rgb1, unsigned rgb2) {
  // https://www.compuphase.com/cmetric.htm
  
  int r1 =  rgb1        & 0xFF;
  int g1 = (rgb1 >>  8) & 0xFF;
  int b1 = (rgb1 >> 16) & 0xFF;
  
  int r2 =  rgb2        & 0xFF;
  int g2 = (rgb2 >>  8) & 0xFF;
  int b2 = (rgb2 >> 16) & 0xFF;
  
  int rmean = (r1 + r2) / 2; // no overlflow possible
  int dr = r2 - r1;
  int dg = g2 - g1;
  int db = b2 - b1;
  
  return (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
}

static int try_find_nearest_win32_color(unsigned color, COLORREF palette[16]) {
  static const int8_t color_channel_reorder[8] = {
    0              | 0                | 0              , // ANSI color 0 = Black
    FOREGROUND_RED | 0                | 0              , // ANSI color 1 = Red
    0              | FOREGROUND_GREEN | 0              , // ANSI color 2 = Green
    FOREGROUND_RED | FOREGROUND_GREEN | 0              , // ANSI color 3 = Yellow
    0              | 0                | FOREGROUND_BLUE, // ANSI color 4 = Blue
    FOREGROUND_RED | 0                | FOREGROUND_BLUE, // ANSI color 5 = Magenta
    0              | FOREGROUND_GREEN | FOREGROUND_BLUE, // ANSI color 6 = Cyan
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, // ANSI color 7 = White
  };
  unsigned color_value = color & COLOR_VALUE_MASK;
  
  COLORREF expected_color = 0;
  
  switch(color & COLOR_MODE_MASK) {
    case COLOR_MODE_DEFAULT: return -1;
    
    case COLOR_MODE_8:
      if(color_value < 8)
        return color_channel_reorder[color_value];
      else
        return -1;
        
    case COLOR_MODE_16:
      if(color_value < 8)
        return color_channel_reorder[color_value];
      else if(color_value < 16)
        return FOREGROUND_INTENSITY | color_channel_reorder[color_value - 8];
      else
        return -1;
    
    case COLOR_MODE_256:
      if(color_value < 8)
        return color_channel_reorder[color_value];
      else if(color_value < 16)
        return FOREGROUND_INTENSITY | color_channel_reorder[color_value - 8];
      
      if(color_value < 232) {
        unsigned blue6  =  (color_value - 16)        & 0x3F;
        unsigned green6 = ((color_value - 16) >>  6) & 0x3F;
        unsigned red6   = ((color_value - 16) >> 12) & 0x3F;
        
        //expected_color = RGB(
        //  (red6   << 2) | (red6   >> 4), 
        //  (green6 << 2) | (green6 >> 4),
        //  (blue6  << 2) | (blue6  >> 4));
        expected_color = RGB(
          red6   ? red6   * 40 + 55 : 0,
          green6 ? green6 * 40 + 55 : 0,
          blue6  ? blue6  * 40 + 55 : 0);
        break;
      }
      if(color_value < 256) {
        unsigned gray = (color_value - 232) * 10 + 8;
        expected_color = RGB(gray, gray, gray);
        break;
      }
      
      return -1;
    
    case COLOR_MODE_RGB24:
      expected_color = color_value;
      break;
    
    default: return -1;
  }
  
  int best_dist = INT_MAX;
  int nearest_index = 0;
  for(int i = 0; i < 16; ++i) {
    int dist = squared_color_distance(expected_color, palette[i]);
    
    if(dist < best_dist) {
      best_dist = dist;
      nearest_index = i;
    }
  }
  
  return nearest_index;
}

static void write_style_changes(
  struct styled_writer_info_t *sw, 
  const struct style_context_t *old_style,
  const struct style_context_t *new_style
) {
  pmath_bool_t use_ansi_colors = sw->support_ansi_escape_codes;
  
  if(!use_ansi_colors) {
    if(new_style->fg_color != old_style->fg_color || new_style->bg_color != old_style->bg_color) {
      HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
      if(stdout_handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFOEX infoex = { .cbSize = sizeof(infoex) };
        if(GetConsoleScreenBufferInfoEx(stdout_handle, &infoex)) {
          
          WORD wAttributes = infoex.wAttributes;
          
          if(new_style->fg_color != old_style->fg_color) {
            unsigned fg_color = new_style->fg_color;
            if((fg_color & COLOR_MODE_MASK) == COLOR_MODE_DEFAULT)
              fg_color = sw->current_style[0].fg_color;
            
            int col = try_find_nearest_win32_color(fg_color, infoex.ColorTable);
            
            if(col >= 0)
              wAttributes = (wAttributes & 0xFFF0) | col;
          }
          
          if(new_style->bg_color != old_style->bg_color) {
            unsigned bg_color = new_style->bg_color;
            if((bg_color & COLOR_MODE_MASK) == COLOR_MODE_DEFAULT)
              bg_color = sw->current_style[0].bg_color;
            
            int col = try_find_nearest_win32_color(bg_color, infoex.ColorTable);
            
            if(col >= 0)
              wAttributes = (wAttributes & 0xFF0F) | (col << 4);
          }
          
          fflush(stdout);
          SetConsoleTextAttribute(stdout_handle, wAttributes);
        }
      }
    }
  }
  
  if(!sw->support_ansi_escape_codes) {
    return;
  }
  
  char buf[50];
  
  char *s = buf;
  *s++ = '\x1B';                             // 1
  *s++ = '[';                                // 2
  
  if(new_style->bold != old_style->bold) {
    if(new_style->bold) {
      *s++ = '1';                            // 3
      *s++ = ';';                            // 4
    }
    else {
      *s++ = '2';                            // 3
      *s++ = '2';                            // 4
      *s++ = ';';                            // 5
    }
  }
  
  if(new_style->italic != old_style->italic) {
    if(new_style->italic) {
      *s++ = '3';                            // 6
      *s++ = ';';                            // 7
    }
    else {
      *s++ = '2';                            // 6
      *s++ = '3';                            // 7
      *s++ = ';';                            // 8
    }
  }
  
  if(use_ansi_colors && new_style->fg_color != old_style->fg_color) {
    unsigned fg_value = new_style->fg_color & COLOR_VALUE_MASK;
    switch(new_style->fg_color & COLOR_MODE_MASK) {
      case COLOR_MODE_DEFAULT: 
        *s++ = '3';                                  // 9
        *s++ = '9';                                  // 10
        *s++ = ';';                                  // 11
        break;
      case COLOR_MODE_8: 
        if(fg_value < 8) {
          *s++ = '3';                                // 9
          *s++ = '0' + fg_value;                     // 10
          *s++ = ';';                                // 11
        }
        break;
      case COLOR_MODE_16:
        if(fg_value < 8) {
          *s++ = '3';                                // 9
          *s++ = '0' + fg_value;                     // 10
          *s++ = ';';                                // 11
        }
        else if(fg_value < 16) {
          *s++ = '9';                                // 9
          *s++ = '0' + (fg_value - 8);               // 10
          *s++ = ';';                                // 11
        }
        break;
      case COLOR_MODE_256:
        if(fg_value < 256) {
          s+= sprintf(s, "38;5;%u;", fg_value);      // 9 - up to 17
        }
        break;
      case COLOR_MODE_RGB24:
        s+= sprintf(s, "38;2;%u;%u;%u;",             // 9 - up to 25
          fg_value         & 0xFF,
          (fg_value >>  8) & 0xFF,
          (fg_value >> 16) & 0xFF); 
        break;
    }
  }
  
  if(use_ansi_colors && new_style->bg_color != old_style->bg_color) {
    unsigned bg_value = new_style->bg_color & COLOR_VALUE_MASK;
    switch(new_style->bg_color & COLOR_MODE_MASK) {
      case COLOR_MODE_DEFAULT: 
        *s++ = '4';                                  // 26
        *s++ = '9';                                  // 27
        *s++ = ';';                                  // 28
        break;
      case COLOR_MODE_8: 
        if(bg_value < 8) {
          *s++ = '4';                                // 26
          *s++ = '0' + bg_value;                     // 27
          *s++ = ';';                                // 28
        }
        break;
      case COLOR_MODE_16:
        if(bg_value < 8) {
          *s++ = '4';                                // 26
          *s++ = '0' + bg_value;                     // 27
          *s++ = ';';                                // 28
        }
        else if(bg_value < 16) {
          *s++ = '1';                                // 26
          *s++ = '0';                                // 27
          *s++ = '0' + (bg_value - 8);               // 28
          *s++ = ';';                                // 29
        }
        break;
      case COLOR_MODE_256:
        if(bg_value < 256) {
          s+= sprintf(s, "48;5;%u;", bg_value);      // 26 - up to 34
        }
        break;
      case COLOR_MODE_RGB24:
        s+= sprintf(s, "48;2;%u;%u;%u;",             // 26 - up to 42
          bg_value         & 0xFF,
          (bg_value >>  8) & 0xFF,
          (bg_value >> 16) & 0xFF); 
        break;
    }
  }
  
  if(s[-1] == ';') {
    s[-1] = 'm';
    *s = '\0';                               // 43
    PMATH_STATIC_ASSERT(sizeof(buf) >=          44);
    
    fputs(buf, stdout);
    fflush(stdout);
  }
}

static void concat_to_string(void *user, const uint16_t *data, int len) {
  pmath_string_t *str = user;
  
  *str = pmath_string_insert_ucs2(*str, INT_MAX, data, len);
}

static pmath_bool_t is_helpline_token(pmath_string_t str) {
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  int i;
  
  if(len == 0)
    return FALSE;
    
  for(i = 0; i < len; ++i) {
    if(buf[i] >= 'a' && buf[i] <= 'z')
      continue;
    if(buf[i] >= 'A' && buf[i] <= 'Z')
      continue;
    if(buf[i] >= '0' && buf[i] <= '9')
      continue;
    if(buf[i] == '*' || buf[i] == '`' || buf[i] == '$')
      continue;
      
    return FALSE;
  }
  
  return TRUE;
}

static pmath_string_t action_to_input_string(pmath_t action) { // `action` will be freed
  pmath_string_t str;
  
  if(pmath_is_expr_of_len(action, pmath_System_ShowDefinition, 1)) { // produce ??text instead of ShowDefinition("text")
    str = pmath_expr_get_item(action, 1);
    if(pmath_is_string(str) && is_helpline_token(str)) {
      pmath_unref(action);
      return pmath_string_insert_latin1(str, 0, "??", 2);
    }
    pmath_unref(str);
  }
  
  str = PMATH_NULL;
  pmath_write(action, PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLSTR, concat_to_string, &str);
  pmath_unref(action);
  return str;
}

static void pre_write_button(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  pmath_t label = pmath_expr_get_item(obj, 1);
  pmath_string_t action_str = action_to_input_string(pmath_expr_get_item(obj, 2));
  const uint16_t *action_buf;
  
  action_str = pmath_string_insert_latin1(action_str, INT_MAX, "", 1); // zero-terminate
  action_buf = pmath_string_buffer(&action_str);
  if(action_buf) {
    pmath_string_t tooltip_str = PMATH_NULL;
    const uint16_t *tooltip_buf = action_buf;
    
    info->current_hyperlink_obj = pmath_ref(obj);
    
    if(pmath_is_expr_of(label, pmath_System_Tooltip) && pmath_expr_length(label) >= 2) {
      pmath_t tooltip_obj = pmath_expr_get_item(label, 2);
      
      pmath_write(tooltip_obj, 0, concat_to_string, &tooltip_str);
      pmath_unref(tooltip_obj);
      
      tooltip_str = pmath_string_insert_latin1(tooltip_str, INT_MAX, "", 1); // zero-terminate
      tooltip_buf = pmath_string_buffer(&tooltip_str);
      if(!tooltip_buf)
        tooltip_buf = action_buf;
    }
    
    hyper_console_start_link(tooltip_buf);
    hyper_console_set_link_input_text(action_buf);
    
    pmath_unref(tooltip_str);
  }
  
  pmath_unref(action_str);
  pmath_unref(label);
}

static pmath_t find_button_function(pmath_expr_t expr, size_t first_option) {
  size_t len = pmath_expr_length(expr);
  size_t i;
  
  for(i = first_option;i <= len;++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_rule(item)) {
      if(pmath_expr_item_equals(item, 1, pmath_System_ButtonFunction)) {
        pmath_t rhs = pmath_expr_get_item(item, 2);
        pmath_unref(item);
        return rhs;
      }
    }
    
    pmath_unref(item);
  }
  
  return PMATH_NULL;
}

static pmath_t button_function_to_action(pmath_t func) {
  if(pmath_is_null(func))
    return func;
  
  if(pmath_is_expr_of_len(func, pmath_System_Function, 1)) {
    pmath_t body = pmath_expr_get_item(func, 1);
    pmath_unref(func);
    return body;
  }
  
  return pmath_expr_new(func, 0);
}

static void pre_write_button_box(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  pmath_t label_box;
  pmath_t action;
  const uint16_t *action_buf;
  
  if(!info->raw_boxes_write_depth)
    return;
  
  label_box = pmath_expr_get_item(obj, 1);
  action = find_button_function(obj, 2);
  action = button_function_to_action(action);
  action = action_to_input_string(action);
  
  action = pmath_string_insert_latin1(action, INT_MAX, "", 1); // zero-terminate
  action_buf = pmath_string_buffer(&action);
  if(action_buf) {
    pmath_string_t tooltip_str = PMATH_NULL;
    const uint16_t *tooltip_buf = action_buf;
    
    info->current_hyperlink_obj = pmath_ref(obj);
    
    if(pmath_is_expr_of(label_box, pmath_System_TooltipBox) && pmath_expr_length(label_box) >= 2) {
      pmath_t tooltip_obj = pmath_expr_get_item(label_box, 2);
      tooltip_obj = pmath_expr_new_extended(pmath_ref(pmath_System_RawBoxes), 1, tooltip_obj);
      
      pmath_write(tooltip_obj, 0, concat_to_string, &tooltip_str);
      pmath_unref(tooltip_obj);
      
      tooltip_str = pmath_string_insert_latin1(tooltip_str, INT_MAX, "", 1); // zero-terminate
      tooltip_buf = pmath_string_buffer(&tooltip_str);
      if(!tooltip_buf)
        tooltip_buf = action_buf;
    }
    
    hyper_console_start_link(tooltip_buf);
    hyper_console_set_link_input_text(action_buf);
    
    pmath_unref(tooltip_str);
  }
  
  pmath_unref(action);
  pmath_unref(label_box);
}

static void pre_write_highlighted(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  info->style_depth++;
  
  int tos = info->style_depth;
  
  assert(tos > 0);
  if(tos < MAX_STYLE_DEPTH && tos > 0) {
    struct style_context_t *outer_style = &info->current_style[tos - 1];
    struct style_context_t *inner_style = &info->current_style[tos];
    
    memcpy(inner_style, outer_style, sizeof(struct style_context_t));
    
    convert_style_directive(inner_style, PMATH_C_STRING("Highlighted"), 4);
    
    write_style_changes(info, outer_style, inner_style);
  }
}

static void pre_write_stylebox_or_style(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  info->style_depth++;
  
  int tos = info->style_depth;
  
  assert(tos > 0);
  if(tos < MAX_STYLE_DEPTH && tos > 0) {
    struct style_context_t *outer_style = &info->current_style[tos - 1];
    struct style_context_t *inner_style = &info->current_style[tos];
    
    memcpy(inner_style, outer_style, sizeof(struct style_context_t));
    
    size_t exprlen = pmath_expr_length(obj);
    for(size_t i = 2; i <= exprlen; ++i) {
      convert_style_directive(inner_style, pmath_expr_get_item(obj, i), 4);
    }
    
    write_style_changes(info, outer_style, inner_style);
  }
}

static void post_write_highlighted(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  post_write_stylebox_or_style(info, obj, options);
}

static void post_write_stylebox_or_style(struct styled_writer_info_t *info, pmath_t obj, pmath_write_options_t options) {
  int tos = info->style_depth;

  info->style_depth--;
  
  assert(tos > 0);
  if(tos < MAX_STYLE_DEPTH && tos > 0) {
    struct style_context_t *outer_style = &info->current_style[tos - 1];
    struct style_context_t *inner_style = &info->current_style[tos];
    
    write_style_changes(info, inner_style, outer_style);
  }
}

static void styled_pre_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct styled_writer_info_t *info = user;
  
  if(bytes_since_last_abortcheck >= ABORT_CHECK_BYTE_COUNT && pmath_aborting())
    return;
    
  if(!(options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR))) {
    if(pmath_same(info->current_hyperlink_obj, PMATH_UNDEFINED)) {
      if(pmath_is_expr_of(obj, pmath_System_Button) && pmath_expr_length(obj) >= 2) 
        pre_write_button(info, obj, options);
      else if(pmath_is_expr_of(obj, pmath_System_ButtonBox) && pmath_expr_length(obj) >= 2) 
        pre_write_button_box(info, obj, options);
    }
    
    if(info->raw_boxes_write_depth == 0) {
      if(pmath_is_expr_of(obj, pmath_System_Highlighted))
        pre_write_highlighted(info, obj, options);
      else if(pmath_is_expr_of(obj, pmath_System_Style))
        pre_write_stylebox_or_style(info, obj, options);
    }
    else {
      if(pmath_is_expr_of(obj, pmath_System_StyleBox))
        pre_write_stylebox_or_style(info, obj, options);
    }
    
    if(pmath_is_expr_of_len(obj, pmath_System_RawBoxes, 1))
      info->raw_boxes_write_depth++;
  }
  
  if(pmath_is_string(obj))
    info->raw_boxes_write_depth++;
}

static void styled_post_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct styled_writer_info_t *info = user;
  
  if(bytes_since_last_abortcheck >= ABORT_CHECK_BYTE_COUNT && pmath_aborting())
    return;
  
  if(info->raw_boxes_write_depth == 0) {
    if(pmath_is_expr_of(obj, pmath_System_Highlighted))
      post_write_stylebox_or_style(info, obj, options);
    else if(pmath_is_expr_of(obj, pmath_System_Style))
      post_write_stylebox_or_style(info, obj, options);
  }
  else {
    if(pmath_is_expr_of(obj, pmath_System_StyleBox))
      post_write_stylebox_or_style(info, obj, options);
  }
  
  if(pmath_is_string(obj))
    info->raw_boxes_write_depth--;
  
  if(!(options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR))) {
    if(pmath_is_expr_of_len(obj, pmath_System_RawBoxes, 1))
      info->raw_boxes_write_depth--;
  }
  
  if(!pmath_same(info->current_hyperlink_obj,  PMATH_UNDEFINED)) {
    if(pmath_same(obj, info->current_hyperlink_obj)) {
      pmath_unref(info->current_hyperlink_obj);
      info->current_hyperlink_obj = PMATH_UNDEFINED;
      hyper_console_end_link();
    }
  }
}

static pmath_bool_t button_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info) {
  pmath_t label = pmath_expr_get_item(obj, 1);
  
  if(pmath_is_expr_of(label, pmath_System_Tooltip) && pmath_expr_length(label) >= 2) {
    pmath_t tmp = pmath_expr_get_item(label, 1);
    pmath_unref(label);
    label = tmp;
  }
  
  pmath_write_ex(info, label);
  pmath_unref(label);
  return TRUE;
}

static pmath_bool_t buttonbox_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info) {
  pmath_t label = pmath_expr_get_item(obj, 1);
  
  if(pmath_is_expr_of(label, pmath_System_TooltipBox) && pmath_expr_length(label) >= 2) {
    pmath_t tmp = pmath_expr_get_item(label, 1);
    pmath_unref(label);
    label = tmp;
  }
  
  pmath_write_ex(info, label);
  pmath_unref(label);
  return TRUE;
}

static pmath_bool_t style_wrapper_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info) {
  pmath_t arg = pmath_expr_get_item(obj, 1);
  pmath_write_ex(info, arg);
  pmath_unref(arg);
  return TRUE;
}

static pmath_bool_t rawboxes_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info) {
  pmath_t boxes = pmath_expr_get_item(obj, 1);
  pmath_bool_t old_raw = sw->formatting_allow_raw_boxes;
  sw->formatting_allow_raw_boxes = TRUE;
  
  pmath_write_ex(info, boxes);
  
  sw->formatting_allow_raw_boxes = old_raw;
  pmath_unref(boxes);
  return TRUE;
}

static pmath_bool_t string_formatter(struct styled_writer_info_t *sw, pmath_t obj, struct pmath_write_ex_t *info) {
  pmath_bool_t old_raw = sw->formatting_allow_raw_boxes;
  
  if(sw->formatting_is_inside_string)
    return FALSE;
  
  sw->formatting_is_inside_string = TRUE;
  sw->formatting_allow_raw_boxes = TRUE;
  
  pmath_write_ex(info, obj);
  
  sw->formatting_allow_raw_boxes = old_raw;
  sw->formatting_is_inside_string = FALSE;
  return TRUE;
}

static pmath_bool_t styled_formatter(void *user, pmath_t obj, struct pmath_write_ex_t *info) {
  struct styled_writer_info_t *sw = user;
  
  if(0 != (info->options & (PMATH_WRITE_OPTIONS_FULLEXPR | PMATH_WRITE_OPTIONS_INPUTEXPR))) 
    return FALSE;
  
  if(pmath_is_string(obj))
    return string_formatter(sw, obj, info);
  
  if(pmath_is_expr_of(obj, pmath_System_Button) && pmath_expr_length(obj) >= 2) 
    return button_formatter(sw, obj, info);
  
  if(pmath_is_expr_of_len(obj, pmath_System_RawBoxes, 1))
    return rawboxes_formatter(sw, obj, info);
  
  if(sw->formatting_allow_raw_boxes) {
    if(pmath_is_expr_of(obj, pmath_System_ButtonBox) && pmath_expr_length(obj) >= 2) 
      return buttonbox_formatter(sw, obj, info);
    if(pmath_is_expr_of(obj, pmath_System_StyleBox) && pmath_expr_length(obj) >= 1) 
      return style_wrapper_formatter(sw, obj, info);
  }
  else {
    if(pmath_is_expr_of(obj, pmath_System_Highlighted) && pmath_expr_length(obj) >= 1) 
      return style_wrapper_formatter(sw, obj, info);
    if(pmath_is_expr_of(obj, pmath_System_Style) && pmath_expr_length(obj) >= 1) 
      return style_wrapper_formatter(sw, obj, info);
  }
  
  return FALSE;
}

static void convert_style_directive(struct style_context_t *context, pmath_t directive, int max_depth) {
  if(max_depth-- <= 0)
    return;
  
  if(pmath_is_rule(directive)) {
    pmath_t lhs = pmath_expr_get_item(directive, 1);
    pmath_t rhs = pmath_expr_get_item(directive, 2);
    
    if(pmath_same(lhs, pmath_System_Background)) {
      pmath_unref(lhs);
      pmath_unref(directive);
      convert_style_directive_Background(context, rhs);
      return;
    }
    
    if(pmath_same(lhs, pmath_System_FontColor)) {
      pmath_unref(lhs);
      pmath_unref(directive);
      convert_style_directive_FontColor(context, rhs);
      return;
    }
    
    if(pmath_same(lhs, pmath_System_FontSlant)) {
      pmath_unref(lhs);
      pmath_unref(directive);
      convert_style_directive_FontSlant(context, rhs);
      return;
    }
    
    if(pmath_same(lhs, pmath_System_FontWeight)) {
      pmath_unref(lhs);
      pmath_unref(directive);
      convert_style_directive_FontWeight(context, rhs);
      return;
    }
    
    if(pmath_same(lhs, pmath_System_BaseStyle)) {
      pmath_unref(lhs);
      pmath_unref(directive);
      convert_style_directive(context, rhs, max_depth);
      return;
    }
    
    pmath_unref(lhs);
    pmath_unref(rhs);
  }
  else if(pmath_is_expr_of(directive, pmath_System_List)) {
    size_t exprlen = pmath_expr_length(directive);
    for(size_t i = 1; i <= exprlen; ++i) {
      convert_style_directive(context, pmath_expr_get_item(directive, i), max_depth);
    }
  }
  else if(pmath_is_expr_of(directive, pmath_System_RGBColor)) {
    convert_style_directive_FontColor(context, directive);
    return;
  }
  else if(pmath_is_expr_of(directive, pmath_System_GrayLevel)) {
    convert_style_directive_FontColor(context, directive);
    return;
  }
  else if(pmath_is_symbol(directive)) {
    if(pmath_same(directive, pmath_System_Bold)) {
      convert_style_directive_FontWeight(context, directive);
      return;
    }
    if(pmath_same(directive, pmath_System_Italic)) {
      convert_style_directive_FontSlant(context, directive);
      return;
    }
    if(pmath_same(directive, pmath_System_Plain)) {
      convert_style_directive_FontSlant(context, pmath_ref(directive));
      convert_style_directive_FontWeight(context, directive);
      return;
    }
  }
  else if(pmath_is_string(directive)) {
    directive = pmath_expr_new_extended(pmath_ref(pmath_System_Console_KnownConsoleStyles), 1, directive);
    directive = pmath_evaluate_secured(directive, PMATH_SECURITY_LEVEL_NON_DESTRUCTIVE_ALLOWED);
    
    convert_style_directive(context, directive, max_depth);
    return;  
  }
  
  pmath_unref(directive);
}

static double convert_to_double(pmath_t obj, double def){ // obj will be freed
  if(!pmath_is_number(obj)) {
    obj = pmath_set_precision(obj, -HUGE_VAL);
  }
  
  if(pmath_is_number(obj)) {
    double value = pmath_number_get_d(obj);
    pmath_unref(obj);
    return value;
  }
  
  pmath_unref(obj);
  return def;
}

static unsigned decode_color(pmath_t color) { // will be freed
  if(pmath_is_expr_of_len(color, pmath_System_RGBColor, 3)) {
    double red   = convert_to_double(pmath_expr_get_item(color, 1), -1.0);
    double green = convert_to_double(pmath_expr_get_item(color, 2), -1.0);
    double blue  = convert_to_double(pmath_expr_get_item(color, 3), -1.0);
    
    pmath_unref(color);
    if(0.0 <= red   && red   <= 1.0
    && 0.0 <= green && green <= 1.0
    && 0.0 <= blue  && blue  <= 1.0) {
      int red_val   = (int)round(red * 255);
      int green_val = (int)round(green * 255);
      int blue_val  = (int)round(blue * 255);
      
      return COLOR_MODE_RGB24 | (red_val) | (green_val << 8) | (blue_val << 16);
    }
    
    return COLOR_UNDEFINED;
  }
  
  if(pmath_is_expr_of_len(color, pmath_System_GrayLevel, 1)) {
    double level = convert_to_double(pmath_expr_get_item(color, 1), -1.0);
    
    pmath_unref(color);
    
    if(0.0 <= level && level <= 1.0) {
      int gray_val = (int)round(level * 255);
      
      return COLOR_MODE_RGB24 | (gray_val) | (gray_val << 8) | (gray_val << 16);
    }
    
    return COLOR_UNDEFINED;
  }
  
  pmath_unref(color);
  return COLOR_UNDEFINED;
}

static void convert_style_directive_Background(struct style_context_t *context, pmath_t color) {
  unsigned bg_color = decode_color(color);
  if(bg_color != COLOR_UNDEFINED) {
    context->bg_color = bg_color;
  }
}
static void convert_style_directive_FontColor(struct style_context_t *context, pmath_t color) {
  unsigned fg_color = decode_color(color);
  if(fg_color != COLOR_UNDEFINED) {
    context->fg_color = fg_color;
  }
}

static void convert_style_directive_FontSlant(struct style_context_t *context, pmath_t slant) {
  if(pmath_same(slant, pmath_System_Italic)) {
    context->italic = 1;
  }
  else if(pmath_same(slant, pmath_System_Plain)) {
    context->italic = 0;
  }
  
  pmath_unref(slant);
}

static void convert_style_directive_FontWeight(struct style_context_t *context, pmath_t weight) {
  if(pmath_same(weight, pmath_System_Bold)) {
    context->bold = 1;
  }
  else if(pmath_same(weight, pmath_System_Plain)) {
    context->bold = 0;
  }
  
  pmath_unref(weight);
}

static pmath_bool_t styled_can_write_unicode(void *user, const uint16_t *str, int len) {
  struct styled_writer_info_t *sw = user;
  UINT cp;
  
  if(!sw->cached_console_font && !sw->no_font_available) {
    CONSOLE_FONT_INFOEX cfi = {sizeof(CONSOLE_FONT_INFOEX)};
    if(GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi)) {
      cfi.FaceName[LF_FACESIZE - 1] = L'\0';
      sw->cached_console_font = CreateFontW(
          0, 0, 0, 0, cfi.FontWeight, FALSE, FALSE, FALSE, 
          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
          cfi.FontFamily, cfi.FaceName);
    }
    else
      sw->no_font_available = TRUE;
  }
  
  if(sw->cached_console_font) {
    HDC dc;
    if(dc = GetDC(NULL)) {
      HGDIOBJ oldfont = SelectObject(dc, sw->cached_console_font);
      pmath_bool_t success = TRUE;
#define GLYPH_BUF_SIZE  8
      WORD glyphs[GLYPH_BUF_SIZE];
      
      while(len) {
        int next_len = len <= GLYPH_BUF_SIZE ? len : GLYPH_BUF_SIZE;
        int i;
        
        DWORD ret = GetGlyphIndicesW(dc, (const wchar_t*)str, next_len, glyphs, GGI_MARK_NONEXISTING_GLYPHS);
        if(ret == GDI_ERROR) {
          success = FALSE;
          break;
        }
        if(ret != (DWORD)next_len) {
          success = FALSE;
          break;
        }
        
        for(i = 0; i < next_len; ++i) {
          if(glyphs[i] == 0xFFFF) {
            success = FALSE;
            break;
          }
        }
        
        len-= next_len;
        str+= next_len;
      }
         
      SelectObject(dc, oldfont);
      return success;
 #undef GLYPH_BUF_SIZE  
    }
  }
  
  // TODO: check current code page instead
  if(cp = GetConsoleOutputCP()) {
    int conv;
    BOOL used_def;
    
    switch(cp) {
      case CP_UTF7:
      case CP_UTF8:
      case CP_WINUNICODE:
        return TRUE;
      
      default: break;
    }
    
    used_def = FALSE;
    conv = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, (const wchar_t*)str, len, NULL, 0, NULL, &used_def);
    if(!conv)
      return FALSE;
    
    // TODO: is this ever set if we provide no output buffer?
    if(used_def)
      return FALSE;
    
    return TRUE;
  }
  return FALSE;
}

//} ... Styled output

static pmath_threadlock_t print_lock = NULL;

struct write_output_t {
  pmath_t object;
  const char *indent;
};

static void write_output_locked_callback(void *_context) {
  struct pmath_line_writer_options_t options;
  struct styled_writer_info_t info;
  struct write_output_t *context = _context;
  int indent_length;
  
  memset(&info, 0, sizeof(info));
  info.current_hyperlink_obj = PMATH_UNDEFINED;
  init_terminal_capabilities(&info);
  
  indent_length = dialog_depth;
  while(indent_length-- > 0)
    printf(" ");
  printf("%s", context->indent);
  
  indent_length = 0;
  while(indent_length <= console_width / 2 && context->indent[indent_length])
    ++indent_length;
    
  indent_length += dialog_depth;
  
  memset(&options, 0, sizeof(options));
  options.size = sizeof(options);
  options.flags = 0;
  options.page_width = console_width - indent_length;
  options.indentation_width = indent_length;
  options.write = styled_write;
  options.user = &info;
  options.pre_write = styled_pre_write;
  options.post_write = styled_post_write;
  options.custom_formatter = styled_formatter;
  options.can_write_unicode = styled_can_write_unicode;
  
  pmath_write_with_pagewidth_ex(&options, context->object);
  
  if(!pmath_same(info.current_hyperlink_obj, PMATH_UNDEFINED))
    hyper_console_end_link();
  pmath_unref(info.current_hyperlink_obj);
  
  if(info.cached_console_font)
    DeleteObject(info.cached_console_font);
  
  printf("\n");
  fflush(stdout);
  
  reset_terminal_capabilities(&info);
}

static void write_line_locked_callback(void *s) {
  printf("%s", (const char *)s);
  fflush(stdout);
}

static void write_wchar_link_locked_callback(void *_s) {
  const wchar_t *s = _s;
  int oldmode;
  
  hyper_console_start_link(s);
  hyper_console_set_link_input_text(s);
  
  fflush(stdout);
  oldmode = _setmode(_fileno(stdout), _O_U8TEXT);
  /* Note that printf and other char* functions do not work at all in _O_U8TEXT mode. */
  
  fwrite(s, 2, wcslen(s), stdout);
  
  fflush(stdout);
  _setmode(_fileno(stdout), oldmode);
  
  hyper_console_end_link();
}

static void indent_locked_callback(void *s) {
  int i = dialog_depth;
  while(i-- > 0)
    printf(" ");
  printf("%s", (const char *)s);
  fflush(stdout);
}

static void write_output(const char *indent, pmath_t obj) {
  struct write_output_t context;
  
  context.object = obj;
  context.indent = indent ? indent : "";
  
  pmath_thread_call_locked(
    &print_lock,
    write_output_locked_callback,
    &context);
}

static void write_line(const char *s) {
  pmath_thread_call_locked(
    &print_lock,
    write_line_locked_callback,
    (void *)s);
}

static void write_indent(const char *s) {
  pmath_thread_call_locked(
    &print_lock,
    indent_locked_callback,
    (void *)s);
}

static void signal_dummy(int sig) {
  signal(sig, signal_dummy);
}

static void signal_handler(int sig) {
  int old_errno;
  
  signal(sig, signal_dummy);
  pmath_suspend_all_please();
  
  old_errno = errno;
  sem_post(&interrupt_semaphore);
  
  signal(sig, signal_handler);
  
  errno = old_errno;
  pmath_resume_all();
}

static void signal_term(int sig) {
  quitting = TRUE;
  pmath_abort_please();
}

static void interrupt_daemon(void *dummy) {
  pmath_messages_t mq;
  
  while(!quitting) {
    sem_wait(&interrupt_semaphore);
    
    if(quitting)
      break;
      
    mq = get_main_mq();
    pmath_thread_send(
      mq,
      pmath_expr_new(pmath_ref(pmath_System_Interrupt), 0));
    pmath_unref(mq);
  }
}

static void kill_interrupt_daemon(void *dummy) {
  quitting = TRUE;
  sem_post(&interrupt_semaphore);
}

struct parse_data_t {
  pmath_t code;
  pmath_t filename;
  int start_line;
  int numlines;
  unsigned error: 1;
};

static void scanner_error(pmath_string_t code, int pos, void *_data, pmath_bool_t critical) {
  struct parse_data_t *data = _data;
  
  if(!data->error)
    pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
    
  if(critical)
    data->error = TRUE;
}

static void handle_options(int argc, const char **argv) {
  --argc;
  ++argv;
  while(argc > 0) {
    if(strcmp(*argv, "-q") == 0 || strcmp(*argv, "--quit") == 0) {
      quitting = TRUE;
      show_mem_stats = FALSE;
    }
    else if((strcmp(*argv, "-l") == 0 || strcmp(*argv, "--load") == 0) &&
            argc > 1)
    {
      --argc;
      ++argv;
      
      PMATH_RUN_ARGS("Get(`1`)", "(o)", pmath_string_from_native(*argv, -1));
    }
    else if((strcmp(*argv, "-x") == 0 || strcmp(*argv, "--exec") == 0) &&
            argc > 1)
    {
      --argc;
      ++argv;
      
      pmath_unref(
        pmath_evaluate(
          pmath_parse_string(
            pmath_string_from_native(*argv, -1))));
    }
    else {
      if(strcmp(*argv, "--help") != 0) {
        quit_result = 1;
        fprintf(stderr, "Unknown option %s\n", *argv);
      }
      
      fprintf(stderr, "\nPossible options are:\n"
              "    -l, --load FILENAME  Load a pMath script and execute it.\n"
              "    -q, --quit           Exit after processing the command line options.\n"
              "    -x, --exec CMD       Evaluate a pMath expression\n\n");
      quitting = TRUE;
      show_mem_stats = FALSE;
      return;
    }
    
    ++argv;
    --argc;
  }
}

static pmath_t check_dialog_return(pmath_t result) { // result wont be freed
  if( pmath_is_expr_of(result, pmath_System_Return) &&
      pmath_expr_length(result) <= 1)
  {
    return pmath_expr_get_item(result, 1);
  }
  
  return PMATH_UNDEFINED;
}

static pmath_t add_debug_metadata(
  pmath_t                             token_or_span,
  const struct pmath_text_position_t *start,
  const struct pmath_text_position_t *end,
  void                               *_data
) {
  pmath_t debug_metadata;
  struct parse_data_t *data = _data;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  assert(end->index <= pmath_string_length(data->code));
  
  if(!pmath_is_expr(token_or_span) && !pmath_is_string(token_or_span))
    return token_or_span;
  
  int startline = start->line + data->start_line;
  int startcol  = start->index - start->line_start_index;
  
  int endline   = end->line + data->start_line;
  int endcol    = end->index - end->line_start_index;
  
  debug_metadata = pmath_language_new_file_location(
                     pmath_ref(data->filename), 
                     startline, startcol, 
                     endline, endcol);
  
  return pmath_try_set_debug_metadata(token_or_span, debug_metadata);
}

static pmath_t dialog(pmath_t first_eval) {
  pmath_t result = PMATH_NULL;
  pmath_t old_dialog = pmath_session_start();
  pmath_string_t continuation_prompt;
  int i;
  const wchar_t *continuation_prompt_buf;
  
  first_eval = pmath_evaluate(first_eval);
  result = check_dialog_return(first_eval);
  pmath_unref(first_eval);
  
  init_console_width();
  
  continuation_prompt = pmath_string_new(dialog_depth + 7 + 1);
  for(i = dialog_depth; i > 0; --i)
    continuation_prompt = pmath_string_insert_latin1(continuation_prompt, INT_MAX, " ", 1);
  continuation_prompt = pmath_string_insert_latin1(continuation_prompt, INT_MAX, "     > ", -1);
  continuation_prompt = pmath_string_insert_latin1(continuation_prompt, INT_MAX, "", 1); // zero-terminate
  continuation_prompt_buf = pmath_string_buffer(&continuation_prompt);
  
  if(pmath_same(result, PMATH_UNDEFINED)) {
    struct pmath_boxes_from_spans_ex_t parse_settings;
    struct parse_data_t                parse_data;
    
    memset(&parse_settings, 0, sizeof(parse_settings));
    parse_settings.size           = sizeof(parse_settings);
    parse_settings.flags          = PMATH_BFS_PARSEABLE;
    parse_settings.data           = &parse_data;
    parse_settings.add_debug_metadata = add_debug_metadata;
    
    parse_data.filename = PMATH_C_STRING("<input>");
    parse_data.start_line = 1;
    parse_data.numlines = 0;
    
    result = PMATH_NULL;
    while(!quitting) {
      pmath_span_array_t *spans;
      
      pmath_continue_after_abort();
      
      write_line("\n");
      write_indent("pmath> ");
      
      parse_data.start_line += parse_data.numlines;
      parse_data.numlines = 1;
      parse_data.error = FALSE;
      parse_data.code = readline_pmath(continuation_prompt_buf);
      
      if(dialog_depth > 0 && pmath_aborting()) {
        pmath_unref(parse_data.code);
        break;
      }
      
      spans = pmath_spans_from_string(
                &parse_data.code,
                NULL,
                NULL,
                NULL,
                scanner_error,
                &parse_data);
      
      init_console_width();
      
      if(!parse_data.error) {
        pmath_t debug_metadata;
        pmath_t obj = pmath_boxes_from_spans_ex(
                        spans,
                        parse_data.code,
                        &parse_settings);
                        
        debug_metadata = pmath_get_debug_metadata(obj);
        
        obj = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(pmath_System_MakeExpression), 1,
                  obj));
                  
        if(pmath_is_expr_of(obj, pmath_System_HoldComplete)) {
          if(pmath_expr_length(obj) == 1) {
            pmath_t tmp = obj;
            obj = pmath_expr_get_item(tmp, 1);
            pmath_unref(tmp);
          }
          else {
            obj = pmath_expr_set_item(
                    obj, 0, pmath_ref(pmath_System_Sequence));
          }
          
          obj = pmath_try_set_debug_metadata(obj, debug_metadata);
          obj = pmath_session_execute(obj, NULL);
        }
        else
          pmath_unref(debug_metadata);
          
        if(!quitting && !pmath_is_null(obj)) {
          if(dialog_depth > 0) {
            result = check_dialog_return(obj);
            
            if(!pmath_same(result, PMATH_UNDEFINED)) {
              pmath_unref(obj);
              pmath_unref(parse_data.code);
              pmath_span_array_free(spans);
              
              break;
            }
            
            result = PMATH_NULL;
          }
          
          write_output("       ", obj);
        }
        
        pmath_unref(obj);
      }
      
      pmath_unref(parse_data.code);
      pmath_span_array_free(spans);
    }
    
    pmath_unref(parse_data.filename);
  }
  
  pmath_unref(continuation_prompt);
  pmath_session_end(old_dialog);
  return result;
}

static pmath_threadlock_t dialog_lock = NULL;

struct dialog_callback_info_t {
  pmath_t result;
  pmath_t first_eval;
};

static void dialog_callback(void *_info) {
  struct dialog_callback_info_t *info = (struct dialog_callback_info_t *)_info;
  
  ++dialog_depth;
  
  info->result = dialog(info->first_eval);
  info->first_eval = PMATH_NULL;
  
  --dialog_depth;
}

static pmath_t builtin_dialog(pmath_expr_t expr) {
  struct dialog_callback_info_t info;
  
  if(pmath_expr_length(expr) > 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  info.result = PMATH_NULL;
  info.first_eval = PMATH_NULL;
  if(pmath_expr_length(expr) == 1) {
    info.first_eval = pmath_expr_get_item(expr, 1);
  }
  
  pmath_unref(expr);
  
  pmath_thread_call_locked(
    &dialog_lock,
    dialog_callback,
    &info);
    
  pmath_unref(info.first_eval);
  return info.result;
}

static pmath_string_t next_word(pmath_string_t *line) {
  const uint16_t *buf = pmath_string_buffer(line);
  int             len = pmath_string_length(*line);
  pmath_string_t word;
  int i;
  
  for(i = 0; i < len && buf[i] > ' '; ++i)
    ++i;
    
  word = pmath_string_part(pmath_ref(*line), 0, i);
  
  for(i = 0; i < len && buf[i] <= ' '; ++i)
    ++i;
    
  *line = pmath_string_part(*line, i, INT_MAX);
  return word;
}

static pmath_threadlock_t interrupt_lock = NULL;

static void interrupt_callback(void *dummy) {
  pmath_string_t line = PMATH_NULL;
  pmath_string_t word = PMATH_NULL;
  
  pmath_suspend_all_please();
  
  while(!quitting/* && !pmath_aborting()*/) {
    pmath_unref(line);
    pmath_unref(word);
    
    write_line("\ninterrupt: ");
    
    line = readline_simple();
    word = next_word(&line);
    
    if( pmath_string_equals_latin1(word, "a") ||
        pmath_string_equals_latin1(word, "abort"))
    {
      write_line("aborting...\n");
      pmath_abort_please();
      break;
    }
    
    if( pmath_string_equals_latin1(word, "bt") ||
        pmath_string_equals_latin1(word, "backtrace"))
    {
      pmath_debug_print_stack();
      continue;
    }
    
    if( pmath_string_equals_latin1(word, "c") ||
        pmath_string_equals_latin1(word, "continue"))
    {
      write_line("continuing...\n");
      break;
    }
    
    if( pmath_string_equals_latin1(word, "i") ||
        pmath_string_equals_latin1(word, "inspect"))
    {
      pmath_messages_t mq ;
      
      write_line("entering interactive dialog (finish with `");
      write_wchar_link_locked_callback(L"Return()");
      write_line("`) ...\n");
      
      mq = get_main_mq();
      pmath_thread_send(
        mq,
        pmath_expr_new(pmath_ref(pmath_System_Dialog), 0));
      pmath_unref(mq);
      break;
    }
    
    if(pmath_string_length(word) > 0) {
      write_output("unknown command: ", word);
    }
    
    write_line(
      "possible commands are:\n"
      "  a, ");
    write_wchar_link_locked_callback(L"abort");
    write_line(
      /**/      "       Abort the current evaluation.\n"
      "  bt, ");
    write_wchar_link_locked_callback(L"backtrace");
    write_line(
      /**/           "  Show the current evaluation stack.\n"
      "  c, ");
    write_wchar_link_locked_callback(L"continue");
    write_line(
      /**/         "    Continue the current evaluation.\n"
      "  i, ");
    write_wchar_link_locked_callback(L"inspect");
    write_line(
      /**/        "     Enter an interactive dialog and continue afterwards.\n");
  }
  
  pmath_unref(line);
  pmath_unref(word);
  
  write_line("\n");
  
  pmath_resume_all();
}

static pmath_t builtin_interrupt(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  if(pmath_thread_queue_is_blocked_by(pmath_ref(main_mq), pmath_thread_get_queue())) {
    /* Already in main thread or the main thread is waiting on the current
       thread (e.g. through pmath_task_wait()) */
    
    pmath_unref(expr);
    
    pmath_thread_call_locked(
      &interrupt_lock,
      interrupt_callback,
      NULL);
  }
  else {
    // in another thread => send to main thread
    
    pmath_messages_t mq = get_main_mq();
    pmath_thread_send(mq, expr);
    pmath_unref(mq);
  }
  
  return PMATH_NULL;
}

static pmath_t builtin_quit(pmath_expr_t expr) {
  if(pmath_expr_length(expr) == 1) {
    pmath_t res = pmath_expr_get_item(expr, 1);
    
    if(!pmath_is_int32(res)) {
      pmath_unref(res);
      pmath_message(PMATH_NULL, "intm", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
      return expr;
    }
    
    quit_result = PMATH_AS_INT32(res);
    pmath_unref(res);
  }
  else if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 1);
    return expr;
  }
  
  quitting = TRUE;
  pmath_abort_please();
  
  pmath_unref(expr);
  return PMATH_NULL;
}

// style will be freed
static void convert_section_style(pmath_t style, int default_color, const char **indent, int *color) {
  *color = default_color;
  *indent = "";

  if(pmath_is_string(style)) {
    if(pmath_string_equals_latin1(style, "Echo")) {
      *indent = ">> ";
    }
    else if(pmath_string_equals_latin1(style, "Message")) {
      *color = (default_color & 0xF0) | 0xC; // red on default background
    }
  }
  
  pmath_unref(style);
}

static void sectionprint_callback(void *arg) {
  pmath_expr_t *expr_ptr = (pmath_expr_t*)arg;
  pmath_expr_t expr = *expr_ptr;
  size_t exprlen = pmath_expr_length(expr);
  
  int default_color = get_default_output_color();
  int color;
  const char *indent;
  
  if(exprlen == 0)
    return;
  
  if(exprlen == 1) {
    pmath_t sections = pmath_expr_get_item(expr, 1);
    size_t i;
    
    if(!pmath_is_expr_of(sections, pmath_System_List))
      sections = pmath_expr_new_extended(pmath_ref(pmath_System_List), 1, sections);
    
    for(i = 1; i <= pmath_expr_length(sections); ++i) {
      pmath_t item = pmath_expr_get_item(sections, i);
      pmath_t style = PMATH_NULL;
      
      if(pmath_is_expr_of(item, pmath_System_Section)) {
        pmath_t boxes = pmath_expr_get_item(item, 1);
        style = pmath_expr_get_item(item, 2);
        
        pmath_unref(item);
        if(pmath_is_expr_of(boxes, pmath_System_BoxData)) 
          item = pmath_expr_set_item(boxes, 0, pmath_ref(pmath_System_RawBoxes));
        else
          item = pmath_expr_new_extended(pmath_ref(pmath_System_RawBoxes), 1, boxes);
      }
      
      convert_section_style(style, default_color, &indent, &color);
      style = PMATH_NULL;
      
      if(color != default_color)
        set_output_color(color);
      
      write_output(indent, item);
      
      if(color != default_color)
        set_output_color(default_color);
        
      pmath_unref(item);
      write_line("\n");
    }
    
    pmath_unref(sections);
    pmath_unref(expr);
    *expr_ptr = PMATH_NULL;
    return;
  }
  
  convert_section_style(pmath_expr_get_item(expr, 1), default_color, &indent, &color);
  
  if(color != default_color)
    set_output_color(color);
    
  if(exprlen == 2) {
    pmath_t item = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    *expr_ptr = PMATH_NULL;
    
    write_output(indent, item);
    pmath_unref(item);
  }
  else {
    pmath_t row = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
    pmath_unref(expr);
    *expr_ptr = PMATH_NULL;
    
    row = pmath_expr_set_item(row, 0, pmath_ref(pmath_System_List));
    row = pmath_expr_new_extended(pmath_ref(pmath_System_Row), 1, row);
    
    write_output(indent, row);
    pmath_unref(row);
  }
  
  if(color != default_color)
    set_output_color(default_color);
    
  write_line("\n");
}

static pmath_t builtin_sectionprint(pmath_expr_t expr) {
  hyper_console_interrupt(sectionprint_callback, &expr);
  return expr;
}

static void init_console_width(void) {
  pmath_t pw;

#ifdef PMATH_OS_WIN32
  {
    CONSOLE_SCREEN_BUFFER_INFO info;
    
    memset(&info, 0, sizeof(info));
    
    if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
      static int last_true_width = 0;
      int width = info.dwSize.X;
      if(last_true_width != width) {
        last_true_width = width;
        if(console_width != width) {
          console_width = width;
          pmath_symbol_set_value(pmath_System_DollarPageWidth, PMATH_FROM_INT32(console_width));
        }
        return;
      }
    }
  }
#endif
  pw = pmath_evaluate(pmath_ref(pmath_System_DollarPageWidth));
  
  if(pmath_is_int32(pw)) 
    console_width = PMATH_AS_INT32(pw);
  
  pmath_unref(pw);
}

static pmath_bool_t init_pmath_bindings(void) {
#define X( SYM, NAME )  SYM = pmath_symbol_get(PMATH_C_STRING( NAME ), TRUE);
  PMATH_SYMBOLS_X
#undef X
  
#define X( SYM, NAME )  !pmath_is_null( SYM ) &&
  return PMATH_SYMBOLS_X
         pmath_register_code(pmath_System_Dialog,       builtin_dialog,       0) &&
         pmath_register_code(pmath_System_Interrupt,    builtin_interrupt,    0) &&
         pmath_register_code(pmath_System_Quit,         builtin_quit,         0) &&
         pmath_register_code(pmath_System_SectionPrint, builtin_sectionprint, 0);
#undef X
}

static void done_pmath_bindings(void) {
  // FIXME: race condition when another thread still runs
#define X( SYM, NAME )  pmath_unref( SYM ); SYM = PMATH_NULL;
  PMATH_SYMBOLS_X
#undef X
}

int main(int argc, const char **argv) {
  main_mq = PMATH_NULL;
  
  os_init();
  
  if(sem_init(&interrupt_semaphore, 0, 0) < 0) {
    fprintf(stderr, "Out of System resoures (sem_init failed).\n");
    quit_result = 1;
    goto FAIL_SEM_INIT;
  }
  
  signal(SIGINT, signal_handler);
  #ifdef SIGBREAK
  signal(SIGBREAK, signal_handler);
  #endif
  signal(SIGTERM, signal_term);
  
  if(!pmath_init()) {
    fprintf(stderr, "Cannot initialize pMath.\n");
    quit_result = 1;
    goto FAIL_PMATH_INIT;
  }
  
  if(!init_pmath_bindings()) {
    fprintf(stderr, "Cannot complete pMath initialization.\n");
    quit_result = 1;
    goto FAIL_INIT_PMATH_BINDINGS;
  }
  
  init_console_width();
  hyper_console_init_hyperlink_system();
  history = hyper_console_history_new(0);
  
  pmath_unref(
    pmath_thread_fork_daemon(
      interrupt_daemon,
      kill_interrupt_daemon,
      NULL));
      
  main_mq = pmath_thread_get_queue();
  
  PMATH_RUN("Get(ToFileName({$BaseDirectory, \"auto\", \"hyper-console\"}, \"init.pmath\"))");
  
  handle_options(argc, argv);
  
  if(!quitting) {
    PMATH_RUN("System`Console`PrintWelcomeMessage()");
  }
  
  pmath_unref(dialog(PMATH_NULL));
  pmath_continue_after_abort();
  
  cleanup_input_cache();
  
  { // freeing main_mq
    pmath_t mq;
    
    pmath_atomic_lock(&main_mq_lock);
    {
      mq = main_mq;
      main_mq = PMATH_NULL;
    }
    pmath_atomic_unlock(&main_mq_lock);
    
    pmath_unref(mq);
  }
  
  signal(SIGINT,   signal_dummy);
  #ifdef SIGBREAK
  signal(SIGBREAK, signal_dummy);
  #endif
  
  hyper_console_done_hyperlink_system();
  hyper_console_history_free(history);
  
  done_pmath_bindings();
  
FAIL_INIT_PMATH_BINDINGS:
  pmath_done();
  
  {
    size_t current, max;
    pmath_mem_usage(&current, &max);
    if(show_mem_stats || current != 0) {
      printf("memory: %"PRIuPTR" (should be 0)\n", current);
      printf("max. used: %"PRIuPTR"\n", max);
    }
  }
  
FAIL_PMATH_INIT:
  sem_destroy(&interrupt_semaphore);
FAIL_SEM_INIT:
  return quit_result;
}
