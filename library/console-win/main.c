#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hyper-console.h>
#include <pmath.h>

#ifdef pmath_debug_print_stack
#  undef pmath_debug_print_stack
#endif


#define MIN(A, B)  ((A) < (B) ? (A) : (B))
#define MAX(A, B)  ((A) > (B) ? (A) : (B))

static void os_init(void);

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
    
    all_char_names = pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), nc_count);
    
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
  
  if(!pmath_is_expr_of(list, PMATH_SYMBOL_LIST))
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
        PMATH_RUN("System`Con`PrintHelpMessage()");
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

struct styled_writer_info_t {
  pmath_t current_hyperlink_obj;
  pmath_t current_hyperlink_label;
  
  unsigned skipping_hyperlink_data: 1;
  unsigned raw_boxes: 1;
};

static int bytes_since_last_abortcheck = 0;
const int ABORT_CHECK_BYTE_COUNT = 100;

static void styled_write(void *user, const uint16_t *data, int len) {
  struct styled_writer_info_t *info = user;
  int oldmode;
  
  if(info->skipping_hyperlink_data)
    return;
    
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
  
  fwprintf(stdout, L"%.*s", len, data); // TODO: handle NUL chars
  
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
  
  if(pmath_is_expr_of_len(action, PMATH_SYMBOL_SHOWDEFINITION, 1)) { // produce ??text instead of ShowDefinition("text")
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
    
    pmath_unref(info->current_hyperlink_label);
    info->current_hyperlink_label = pmath_ref(label);
    info->current_hyperlink_obj = pmath_ref(obj);
    info->skipping_hyperlink_data = TRUE;
    
    if(pmath_is_expr_of(label, PMATH_SYMBOL_TOOLTIP) && pmath_expr_length(label) >= 2) {
      pmath_t tooltip_obj = pmath_expr_get_item(label, 2);
      
      pmath_write(tooltip_obj, 0, concat_to_string, &tooltip_str);
      pmath_unref(tooltip_obj);
      
      tooltip_str = pmath_string_insert_latin1(tooltip_str, INT_MAX, "", 1); // zero-terminate
      tooltip_buf = pmath_string_buffer(&tooltip_str);
      if(tooltip_buf) {
        pmath_unref(info->current_hyperlink_label);
        info->current_hyperlink_label = pmath_expr_get_item(label, 1);
      }
      else
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
    
    if(pmath_is_expr_of_len(item, PMATH_SYMBOL_RULE, 2) || pmath_is_expr_of_len(item, PMATH_SYMBOL_RULEDELAYED, 2)) {
      pmath_t lhs = pmath_expr_get_item(item, 1);
      pmath_unref(lhs);
      if(pmath_same(lhs, PMATH_SYMBOL_BUTTONFUNCTION)) {
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
  
  if(pmath_is_expr_of_len(func, PMATH_SYMBOL_FUNCTION, 1)) {
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
  
  if(!info->raw_boxes)
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
    
    pmath_unref(info->current_hyperlink_label);
    info->current_hyperlink_label = pmath_ref(label_box);
    info->current_hyperlink_obj = pmath_ref(obj);
    info->skipping_hyperlink_data = TRUE;
    
    if(pmath_is_expr_of(label_box, PMATH_SYMBOL_TOOLTIPBOX) && pmath_expr_length(label_box) >= 2) {
      pmath_t tooltip_obj = pmath_expr_get_item(label_box, 2);
      tooltip_obj = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RAWBOXES), 1, tooltip_obj);
      
      pmath_write(tooltip_obj, 0, concat_to_string, &tooltip_str);
      pmath_unref(tooltip_obj);
      
      tooltip_str = pmath_string_insert_latin1(tooltip_str, INT_MAX, "", 1); // zero-terminate
      tooltip_buf = pmath_string_buffer(&tooltip_str);
      if(tooltip_buf) {
        pmath_unref(info->current_hyperlink_label);
        info->current_hyperlink_label = pmath_expr_get_item(label_box, 1);
      }
      else
        tooltip_buf = action_buf;
    }
    
    hyper_console_start_link(tooltip_buf);
    hyper_console_set_link_input_text(action_buf);
    
    pmath_unref(tooltip_str);
  }
  
  pmath_unref(action);
  pmath_unref(label_box);
}

static void styled_pre_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct styled_writer_info_t *info = user;
  
  if(bytes_since_last_abortcheck >= ABORT_CHECK_BYTE_COUNT && pmath_aborting())
    return;
    
  if(info->skipping_hyperlink_data && pmath_same(obj, info->current_hyperlink_label))
    info->skipping_hyperlink_data = FALSE;
    
  if(0 == (options & (PMATH_WRITE_OPTIONS_FULLEXPR | PMATH_WRITE_OPTIONS_INPUTEXPR))) {
    if(pmath_same(info->current_hyperlink_obj, PMATH_UNDEFINED)) {
      if(pmath_is_expr_of(obj, PMATH_SYMBOL_BUTTON) && pmath_expr_length(obj) >= 2) 
        pre_write_button(info, obj, options);
      if(pmath_is_expr_of(obj, PMATH_SYMBOL_BUTTONBOX) && pmath_expr_length(obj) >= 2) 
        pre_write_button_box(info, obj, options);
    }
  }
  
  if(!(options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR))) {
    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_RAWBOXES, 1))
      info->raw_boxes = TRUE;
  }
}

static void styled_post_write(void *user, pmath_t obj, pmath_write_options_t options) {
  struct styled_writer_info_t *info = user;
  
  if(bytes_since_last_abortcheck >= ABORT_CHECK_BYTE_COUNT && pmath_aborting())
    return;
  
  if(!(options & (PMATH_WRITE_OPTIONS_INPUTEXPR | PMATH_WRITE_OPTIONS_FULLEXPR))) {
    if(pmath_is_expr_of_len(obj, PMATH_SYMBOL_RAWBOXES, 1))
      info->raw_boxes = FALSE;
  }
  
  if(!pmath_same(info->current_hyperlink_obj,  PMATH_UNDEFINED)) {
    if(pmath_same(obj, info->current_hyperlink_obj)) {
      pmath_unref(info->current_hyperlink_obj);
      pmath_unref(info->current_hyperlink_label);
      info->current_hyperlink_obj = PMATH_UNDEFINED;
      info->current_hyperlink_label = PMATH_UNDEFINED;
      info->skipping_hyperlink_data = FALSE;
      hyper_console_end_link();
    }
    
    if(!info->skipping_hyperlink_data && pmath_same(obj, info->current_hyperlink_label)) {
      info->skipping_hyperlink_data = TRUE;
      pmath_unref(info->current_hyperlink_label);
      info->current_hyperlink_label = PMATH_UNDEFINED;
    }
  }
}

static pmath_threadlock_t print_lock = NULL;

struct write_output_t {
  pmath_t object;
  const char *indent;
};

static void write_output_locked_callback(void *_context) {
  struct line_writer_options_t options;
  struct styled_writer_info_t info;
  struct write_output_t *context = _context;
  int indent_length;
  
  memset(&info, 0, sizeof(info));
  info.current_hyperlink_obj = PMATH_UNDEFINED;
  info.current_hyperlink_label = PMATH_UNDEFINED;
  
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
  
  pmath_write_with_pagewidth_ex(&options, context->object);
  
  if(!pmath_same(info.current_hyperlink_obj, PMATH_UNDEFINED))
    hyper_console_end_link();
  pmath_unref(info.current_hyperlink_obj);
  pmath_unref(info.current_hyperlink_label);
  
  printf("\n");
  fflush(stdout);
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
      pmath_expr_new(pmath_ref(PMATH_SYMBOL_INTERRUPT), 0));
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
  if( pmath_is_expr_of(result, PMATH_SYMBOL_RETURN) &&
      pmath_expr_length(result) <= 1)
  {
    return pmath_expr_get_item(result, 1);
  }
  
  return PMATH_UNDEFINED;
}

static pmath_t add_debug_info(
  pmath_t                             token_or_span,
  const struct pmath_text_position_t *start,
  const struct pmath_text_position_t *end,
  void                               *_data
) {
  pmath_t debug_info;
  struct parse_data_t *data = _data;
  int start_line, end_line, start_column, end_column;
  
  assert(0 <= start->index);
  assert(start->index <= end->index);
  assert(end->index <= pmath_string_length(data->code));
  
  if(!pmath_is_expr(token_or_span))
    return token_or_span;
    
  start_line   = start->line + data->start_line;
  start_column = start->index - start->line_start_index;
  
  end_line   = end->line + data->start_line;
  end_column = end->index - end->line_start_index;
  
  debug_info = pmath_expr_new_extended(
                 pmath_ref(PMATH_SYMBOL_DEVELOPER_DEBUGINFOSOURCE), 2,
                 pmath_ref(data->filename),
                 pmath_expr_new_extended(
                   pmath_ref(PMATH_SYMBOL_RANGE), 2,
                   pmath_build_value("(ii)", start_line, start_column),
                   pmath_build_value("(ii)", end_line,   end_column)));
                   
  return pmath_try_set_debug_info(token_or_span, debug_info);
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
  
  PMATH_RUN_ARGS("$PageWidth:=`1`", "(i)", console_width - (7 + dialog_depth));
  
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
    parse_settings.add_debug_info = add_debug_info;
    
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
                
      if(!parse_data.error) {
        pmath_t debug_info;
        pmath_t obj = pmath_boxes_from_spans_ex(
                        spans,
                        parse_data.code,
                        &parse_settings);
                        
        debug_info = pmath_get_debug_info(obj);
        
        obj = pmath_evaluate(
                pmath_expr_new_extended(
                  pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
                  obj));
                  
        if(pmath_is_expr_of(obj, PMATH_SYMBOL_HOLDCOMPLETE)) {
          if(pmath_expr_length(obj) == 1) {
            pmath_t tmp = obj;
            obj = pmath_expr_get_item(tmp, 1);
            pmath_unref(tmp);
          }
          else {
            obj = pmath_expr_set_item(
                    obj, 0, pmath_ref(PMATH_SYMBOL_SEQUENCE));
          }
          
          obj = pmath_try_set_debug_info(obj, debug_info);
          obj = pmath_session_execute(obj, NULL);
        }
        else
          pmath_unref(debug_info);
          
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
  
  PMATH_RUN_ARGS("$PageWidth:=`1`", "(i)", console_width - (7 + dialog_depth - 1));
  
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
        pmath_expr_new(pmath_ref(PMATH_SYMBOL_DIALOG), 0));
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

static void sectionprint_callback(void *arg) {
  pmath_expr_t *expr_ptr = (pmath_expr_t*)arg;
  pmath_expr_t expr = *expr_ptr;
  
  pmath_t style;
  const char *indent = "";
  int default_color = get_default_output_color();
  int color = default_color;
  
  if(pmath_expr_length(expr) < 2)
    return;
    
  style = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(style)) {
    if(pmath_string_equals_latin1(style, "Echo")) {
      indent = ">> ";
    }
    else if(pmath_string_equals_latin1(style, "Message")) {
      color = (default_color & 0xF0) | 0xC; // red on default background
    }
  }
  pmath_unref(style);
  
  if(color != default_color)
    set_output_color(color);
    
  if(pmath_expr_length(expr) == 2) {
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
    
    row = pmath_expr_set_item(row, 0, pmath_ref(PMATH_SYMBOL_LIST));
    row = pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_ROW), 1, row);
    
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
  pmath_t pw = pmath_evaluate(pmath_ref(PMATH_SYMBOL_PAGEWIDTHDEFAULT));
  
  if(pmath_is_int32(pw))
    console_width = PMATH_AS_INT32(pw) - 1;
    
  pmath_unref(pw);
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
  signal(SIGTERM, signal_term);
  
  if( !pmath_init() ||
      !pmath_register_code(PMATH_SYMBOL_DIALOG,       builtin_dialog,       0) ||
      !pmath_register_code(PMATH_SYMBOL_INTERRUPT,    builtin_interrupt,    0) ||
      !pmath_register_code(PMATH_SYMBOL_QUIT,         builtin_quit,         0) ||
      !pmath_register_code(PMATH_SYMBOL_SECTIONPRINT, builtin_sectionprint, 0))
  {
    fprintf(stderr, "Cannot initialize pMath.\n");
    quit_result = 1;
    goto FAIL_PMATH_INIT;
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
    PMATH_RUN("System`Con`PrintWelcomeMessage()");
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
  
  pmath_done();
  
  {
    size_t current, max;
    pmath_mem_usage(&current, &max);
    if(show_mem_stats || current != 0) {
      printf("memory: %"PRIuPTR" (should be 0)\n", current);
      printf("max. used: %"PRIuPTR"\n", max);
    }
  }
  
  signal(SIGINT, signal_dummy);
  
  hyper_console_done_hyperlink_system();
  hyper_console_history_free(history);
FAIL_PMATH_INIT:
  sem_destroy(&interrupt_semaphore);
FAIL_SEM_INIT:
  return quit_result;
}
