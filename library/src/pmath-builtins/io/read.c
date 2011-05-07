#include <pmath-core/numbers-private.h>

#include <pmath-language/scanner.h>

#include <pmath-util/approximate.h>
#include <pmath-util/concurrency/threads.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>
#include <pmath-builtins/io-private.h>
#include <pmath-builtins/language-private.h>

static void syntax_error(pmath_string_t code, int pos, void *data, pmath_bool_t critical){
  pmath_message_syntax_error(code, pos, PMATH_NULL, 0);
}

static pmath_files_status_t skip_whitespace(pmath_t file, pmath_bool_t *eol){
  pmath_files_status_t status;
  pmath_string_t line;
  const uint16_t *str;
  int len, i;

  status = pmath_file_status(file);

  while(status == PMATH_FILE_OK){
    line = pmath_file_readline(file);
    len = pmath_string_length(line);
    str = pmath_string_buffer(&line);

    i = 0;
    while(i < len && str[i] <= ' ')
      ++i;

    if(i < len){
      pmath_file_set_textbuffer(
        file,
        pmath_string_part(line, i, -1));
      return pmath_file_status(file);
    }

    *eol = TRUE;
    pmath_unref(line);
    status = pmath_file_status(file);
  }

  return status;
}

static pmath_string_t readline(void *p){
  pmath_t file = *(pmath_t*)p;

  return pmath_file_readline(file);
}

static pmath_t read_expression(pmath_t file){
  pmath_span_array_t *spans;
  pmath_string_t code;
  pmath_t result;

  code = pmath_file_readline(file);
  while(pmath_string_length(code) == 0
  && pmath_file_status(file) == PMATH_FILE_OK){
    pmath_unref(code);
    code = pmath_file_readline(file);
  }

  spans = pmath_spans_from_string(
    &code,
    readline,
    NULL,
    NULL,
    syntax_error,
    &file);

  result = pmath_boxes_from_spans(
    spans,
    code,
    TRUE,
    NULL,
    NULL);

  pmath_unref(code);
  pmath_span_array_free(spans);

  result = pmath_evaluate(
    pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_MAKEEXPRESSION), 1,
      result));

  if(!pmath_is_expr_of(result, PMATH_SYMBOL_HOLDCOMPLETE))
    return result;

  if(pmath_expr_length(result) == 1){
    pmath_t item = pmath_expr_get_item(result, 1);
    pmath_unref(result);
    return item;
  }

  return pmath_expr_set_item(
    result, 0,
    pmath_ref(PMATH_SYMBOL_SEQUENCE));
}

static pmath_string_t read_word(pmath_t file, pmath_bool_t *eol){
  pmath_string_t line;
  const uint16_t *str;
  int len, start, end, next;

  switch(skip_whitespace(file, eol)){
    case PMATH_FILE_ENDOFFILE:
      *eol = TRUE;
      return pmath_ref(PMATH_SYMBOL_ENDOFFILE);

    case PMATH_FILE_OK: break;

    default:
      *eol = TRUE;
      return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  line = pmath_file_readline(file);
  len = pmath_string_length(line);
  str = pmath_string_buffer(&line);

  start = 0;
  while(start < len && str[start] <= ' ')
    ++start;

  end = start;
  while(end < len && str[end] > ' ')
    ++end;

  next = end;
  while(next < len && str[next] <= ' ')
    ++next;

  if(next == len)
    *eol = TRUE;

  pmath_file_set_textbuffer(
    file,
    pmath_string_part(pmath_ref(line), next, -1));

  return pmath_string_part(line, start, end - start);
}

static pmath_t word_to_number(pmath_string_t word){
  pmath_t result = _pmath_parse_number(word, TRUE);

  if(pmath_is_null(result)){
    pmath_message(PMATH_NULL, "readn", 0);

    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  return result;
}

static pmath_bool_t _read(
  pmath_t       file,
  pmath_t      *type_value,
  pmath_bool_t *eol
){
  if(pmath_same(*type_value, PMATH_SYMBOL_STRING)){
    pmath_unref(*type_value);
    *type_value = pmath_file_readline(file);

    if(pmath_is_null(*type_value)){
      if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE)
        *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      else
        *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
    }

    *eol = TRUE;

    return TRUE;
  }

  if(pmath_same(*type_value, PMATH_SYMBOL_CHARACTER)){
    pmath_unref(*type_value);
    *type_value = pmath_file_readline(file);

    if(pmath_string_length(*type_value) == 0){
      pmath_unref(*type_value);
      if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE)
        *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      else
        *type_value = PMATH_C_STRING("\n");

      *eol = TRUE;
    }
    else{
      *eol = pmath_string_length(*type_value) == 1;

      pmath_file_set_textbuffer(
        file,
        pmath_string_part(pmath_ref(*type_value), 1, -1));

      *type_value = pmath_string_part(*type_value, 0, 1);
    }

    return TRUE;
  }

  if(pmath_same(*type_value, PMATH_SYMBOL_WORD)){
    pmath_unref(*type_value);
    *type_value = read_word(file, eol);

    if(pmath_string_length(*type_value) == 0){
      pmath_unref(*type_value);
      if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE)
        *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      else
        *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
    }

    return TRUE;
  }

  if(pmath_same(*type_value, PMATH_SYMBOL_NUMBER)){
    pmath_unref(*type_value);
    *type_value = read_word(file, eol);

    if(pmath_string_length(*type_value) == 0){
      pmath_unref(*type_value);
      if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE)
        *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      else
        *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
    }
    else
      *type_value = word_to_number(*type_value);

    return TRUE;
  }

  if(pmath_same(*type_value, PMATH_SYMBOL_REAL)){
    pmath_unref(*type_value);
    *type_value = read_word(file, eol);

    if(pmath_string_length(*type_value) == 0){
      pmath_unref(*type_value);
      if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE)
        *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      else
        *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
    }
    else{
      *type_value = word_to_number(*type_value);

      if(pmath_is_rational(*type_value))
        *type_value = pmath_approximate(*type_value, -HUGE_VAL, -HUGE_VAL, NULL);
    }

    return TRUE;
  }

  if(pmath_same(*type_value, PMATH_SYMBOL_EXPRESSION)){
    pmath_unref(*type_value);
    *type_value = read_expression(file);

    *eol = TRUE;

    return TRUE;
  }

  if(pmath_is_expr(*type_value)){
    size_t i;

    if(pmath_file_status(file) == PMATH_FILE_ENDOFFILE){
      *eol = TRUE;
      pmath_unref(*type_value);
      *type_value = pmath_ref(PMATH_SYMBOL_ENDOFFILE);
      return TRUE;
    }

    for(i = 1;i <= pmath_expr_length(*type_value);++i){
      pmath_t item = pmath_expr_get_item(*type_value, i);

      if(!_read(file, &item, eol)){
        pmath_unref(*type_value);
        *type_value = item;
        return FALSE;
      }

      *type_value = pmath_expr_set_item(*type_value, i, item);
    }

    return TRUE;
  }

  pmath_message(PMATH_NULL, "readf", 1, *type_value);
  *type_value = pmath_ref(PMATH_SYMBOL_FAILED);
  return FALSE;
}

PMATH_PRIVATE pmath_t builtin_read(pmath_expr_t expr){
/* Read(file, type)
   Read(file)        = Read(file, Expression)
 */
  pmath_expr_t options;
  pmath_t file, type;
  pmath_bool_t eol;
  size_t last_nonoption;

  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 2);
    return expr;
  }

  type = pmath_expr_get_item(expr, 2);
  if(pmath_is_null(type) || _pmath_is_rule(type) || _pmath_is_list_of_rules(type)){
    pmath_unref(type);
    type = pmath_ref(PMATH_SYMBOL_EXPRESSION);
    last_nonoption = 1;
  }
  else
    last_nonoption = 2;

  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)){
    pmath_unref(type);
    return expr;
  }

  file = pmath_expr_get_item(expr, 1);
  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)){
    pmath_unref(file);
    pmath_unref(type);
    pmath_unref(expr);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  pmath_unref(expr);
  pmath_unref(options);

  // locking?
  _read(file, &type, &eol);

  pmath_unref(file);
  return type;
}

PMATH_PRIVATE pmath_t builtin_readlist(pmath_expr_t expr){
/* ReadList(file, type, n)
   ReadList(file, type)    = ReadList(file, type, Infinity)
   ReadList(file)          = ReadList(file, Expression, Infinity)

   options:
    RecordLists->False
 */
  pmath_expr_t options;
  pmath_t file, type, item;
  pmath_bool_t eol;
  pmath_bool_t record_lists = FALSE;
  size_t last_nonoption, i;
  size_t count = SIZE_MAX;

  if(pmath_expr_length(expr) < 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 3);
    return expr;
  }

  type = pmath_expr_get_item(expr, 2);
  if(pmath_is_null(type) || _pmath_is_rule(type) || _pmath_is_list_of_rules(type)){
    pmath_unref(type);
    type = pmath_ref(PMATH_SYMBOL_EXPRESSION);
    last_nonoption = 1;
  }
  else{
    pmath_t n = pmath_expr_get_item(expr, 3);

    if(pmath_is_null(n) || _pmath_is_rule(n) || _pmath_is_list_of_rules(n)){
      last_nonoption = 2;
    }
    else{
      last_nonoption = 3;

      if(pmath_is_int32(n) && PMATH_AS_INT32(n) >= 0){
        count = (size_t)PMATH_AS_INT32(n);
      }
      else if(!pmath_equals(n, _pmath_object_infinity)){
        pmath_message(PMATH_NULL, "intnm", 2, PMATH_FROM_INT32(3), pmath_ref(expr));

        pmath_unref(n);
        pmath_unref(type);
        return expr;
      }
    }

    pmath_unref(n);
  }

  options = pmath_options_extract(expr, last_nonoption);
  if(pmath_is_null(options)){
    pmath_unref(type);
    return expr;
  }

  item = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_RECORDLISTS, options);
  if(pmath_same(item, PMATH_SYMBOL_TRUE)){
    record_lists = TRUE;
  }
  else if(!pmath_same(item, PMATH_SYMBOL_FALSE)){
    pmath_message(PMATH_NULL, "opttf", 2, pmath_ref(PMATH_SYMBOL_RECORDLISTS), item);
    pmath_unref(options);
    pmath_unref(type);
    return expr;
  }
  pmath_unref(item);

  file = pmath_expr_get_item(expr, 1);
  if(pmath_is_string(file)){
    file = pmath_evaluate(pmath_parse_string_args(
      "Try(OpenRead(`1`))", "(o)", file));
  }

  if(!_pmath_file_check(file, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_TEXT)){
    pmath_unref(file);
    pmath_unref(type);
    pmath_unref(expr);
    pmath_unref(options);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }

  pmath_unref(expr);
  pmath_unref(options);

  pmath_gather_begin(PMATH_NULL);

  // locking?
  if(record_lists){
    pmath_bool_t more = pmath_file_status(file) == PMATH_FILE_OK;

    i = 0;
    while(i < count && more){
      pmath_gather_begin(PMATH_NULL);

      eol = FALSE;
      while(i < count && more && !eol){
        item = pmath_ref(type);

        more = _read(file, &item, &eol);

        pmath_emit(item, PMATH_NULL);
        ++i;

        more = more && !pmath_aborting() && pmath_file_status(file) == PMATH_FILE_OK;
      }

      item = pmath_gather_end();
      if(pmath_expr_length(item) == 0)
        pmath_unref(item);
      else
        pmath_emit(item, PMATH_NULL);
    }
  }
  else{
    pmath_bool_t more = pmath_file_status(file) == PMATH_FILE_OK;

    i = 0;
    while(i < count && more){
      item = pmath_ref(type);

      more = _read(file, &item, &eol);

      pmath_emit(item, PMATH_NULL);
      ++i;

      more = more && !pmath_aborting() && pmath_file_status(file) == PMATH_FILE_OK;
    }
  }

  pmath_unref(file);
  pmath_unref(type);
  return pmath_gather_end();
}
