#include <pmath-core/strings.h>
#include <pmath-core/objects-private.h>

#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/language-private.h>

#include <limits.h>
#include <string.h>

extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_MakeBoxes;
extern pmath_symbol_t pmath_System_StringBox;

static pmath_bool_t has_param_placeholders(
  pmath_t box // wont be freed
) {
  if(pmath_is_expr(box)) {
    size_t i = pmath_expr_length(box);
    for(; i > 0; --i) {
      pmath_t item = pmath_expr_get_item(box, i);
      pmath_bool_t result = has_param_placeholders(item);
      pmath_unref(item);
      
      if(result)
        return result;
    }
  }
  else if(pmath_is_string(box)) {
    const uint16_t *buf = pmath_string_buffer(&box);
    int len = pmath_string_length(box);
    int i;
    
    for(i = 0; i < len - 1; ++i)
      if(buf[i] == '`') {
        do {
          ++i;
        } while(i < len - 1 && buf[i] >= '0' && buf[i] <= '9');
        
        if(buf[i] == '`')
          return TRUE;
      }
  }
  
  return FALSE;
}

typedef struct {
  pmath_expr_t formated_params;
  size_t       current_param;
} _format_data_t;

static pmath_t string_form(
  pmath_t  format,  // will be freed
  _format_data_t *data
) {
  if(!has_param_placeholders(format))
    return format;
    
  if(pmath_is_string(format)) {
    format = pmath_string_expand_boxes(format);
    
    if(pmath_is_string(format)) {
      const uint16_t *buf = pmath_string_buffer(&format);
      int len = pmath_string_length(format);
      int start = 0;
      int i = 0;
      
      pmath_gather_begin(PMATH_NULL);
      
      while(i < len - 1) {
        if(buf[i] == '`') {
          if(buf[i + 1] == '`') {
            if(start < i) {
              pmath_emit(
                pmath_string_part(
                  pmath_ref(format),
                  start,
                  i - start),
                PMATH_NULL);
            }
            
            pmath_emit(
              pmath_expr_get_item(
                data->formated_params,
                data->current_param++),
              PMATH_NULL);
              
            start = i += 2;
          }
          else {
#define NUMLEN 20
            int k = i;
            
            do {
              ++k;
            } while(k < len - 1 && buf[k] >= '0' && buf[k] <= '9');
            
            if(buf[k] == '`' && i + 1 < k && k <= i + NUMLEN + 1) {
              char num[NUMLEN];
              int end = i;
              size_t param;
              
              if(start < i) {
                pmath_emit(
                  pmath_string_part(
                    pmath_ref(format),
                    start,
                    end - start),
                  PMATH_NULL);
              }
              
              start = 0;
              do {
                num[start++] = (char)(unsigned char)buf[++i];
              } while(i < k - 1);
              num[start] = '\0';
              
              param = (size_t)atoi(num);
              
              if(param > 0 && param <= pmath_expr_length(data->formated_params)) {
                data->current_param = param;
                
                pmath_emit(
                  pmath_expr_get_item(
                    data->formated_params,
                    data->current_param++),
                  PMATH_NULL);
                  
                start = i = k + 1;
              }
              else {
                start = end;
                i = k + 1;
              }
            }
            else
              ++i;
          }
        }
        else
          ++i;
      }
      
      if(start < len) {
        pmath_emit(
          pmath_string_part(
            pmath_ref(format),
            start,
            len - start),
          PMATH_NULL);
      }
      
      pmath_unref(format);
      format = pmath_gather_end();
      if(pmath_expr_length(format) == 1) {
        pmath_t result = pmath_expr_get_item(format, 1);
        
        pmath_unref(format);
        return result;
      }
      
      return format;
    }
  }
  
  if(pmath_is_expr(format)) {
    size_t i;
    for(i = 1; i <= pmath_expr_length(format); ++i)
      format = pmath_expr_set_item(
                 format, i,
                 string_form(
                   pmath_expr_get_item(
                     format, i),
                   data));
                   
    return format;
  }
  
  return format;
}

PMATH_PRIVATE
pmath_bool_t _pmath_stringform_write(
  struct pmath_write_ex_t *info,
  pmath_expr_t             stringform // wont be freed
) {
  _format_data_t  data;
  pmath_t  result;
  pmath_string_t  format;
  size_t i;
  
  if(pmath_is_null(stringform))
    return FALSE;
    
  assert(pmath_is_expr(stringform));
  
  format = pmath_expr_get_item(stringform, 1);
  if(!pmath_is_string(format)) {
    pmath_unref(format);
    return FALSE;
  }
  
  data.formated_params = pmath_expr_get_item_range(stringform, 2, SIZE_MAX);
  data.current_param = 1;
  
  result = string_form(format, &data);
  
  pmath_unref(data.formated_params);
  
  if(pmath_is_string(result)) {
    _pmath_write_impl(info, result);
  }
  else if(pmath_is_expr(result)) {
    for(i = 1; i <= pmath_expr_length(result); ++i) {
      pmath_t item = pmath_expr_get_item(result, i);
      
      _pmath_write_impl(info, item);
      
      pmath_unref(item);
    }
  }
  
  pmath_unref(result);
  return TRUE;
}

PMATH_PRIVATE
pmath_t _pmath_stringform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   stringform // wont be freed
) {
  _format_data_t  data;
  pmath_t         result;
  pmath_string_t  format;
  size_t i;
  
  if(pmath_is_null(stringform))
    return PMATH_NULL;
    
  assert(pmath_is_expr(stringform));
  
  format = pmath_expr_get_item(stringform, 1);
  if(!pmath_is_string(format)) {
    pmath_unref(format);
    return PMATH_NULL;
  }
  
  data.formated_params = pmath_expr_get_item_range(stringform, 2, SIZE_MAX);
  
  for(i = 1; i <= pmath_expr_length(data.formated_params); ++i) {
    pmath_t item = pmath_expr_get_item(data.formated_params, i);
    data.formated_params = pmath_expr_set_item(
                             data.formated_params, i, PMATH_NULL);
                             
    item = pmath_expr_new_extended(
             pmath_ref(pmath_System_List), 1,
             pmath_evaluate(
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_MakeBoxes), 1,
                 item)));
                 
    data.formated_params = pmath_expr_set_item(
                             data.formated_params, i, item);
  }
  
  data.current_param = 1;
  
  result = string_form(format, &data);
  
  pmath_unref(data.formated_params);
  
  if(pmath_is_expr(result)){
    size_t len;
    pmath_t part;
    
    len = pmath_expr_length(result);
    for(i = len; i > 0; --i) {
      part = pmath_expr_extract_item(result, i);
      
      if(pmath_is_string(part)) {
        part = _pmath_escape_string(
                 PMATH_NULL, 
                 part, 
                 PMATH_NULL, 
                 thread->boxform >= BOXFORM_INPUT);
      }
      
      result = pmath_expr_set_item(result, i, part);
    }
    
    part = pmath_expr_extract_item(result, len);
    if(pmath_is_string(part)) {
      part = pmath_string_insert_latin1(part, INT_MAX, "\"", 1);
      result = pmath_expr_set_item(result, len, part);
    }
    else{
      result = pmath_expr_set_item(result, len, part);
      result = pmath_expr_append(result, 1, PMATH_C_STRING("\""));
    }
    
    
    part = pmath_expr_extract_item(result, 1);
    if(pmath_is_string(part)) {
      part = pmath_string_insert_latin1(part, 0, "\"", 1);
      result = pmath_expr_set_item(result, 1, part);
    }
    else{
      pmath_t tmp;
      result = pmath_expr_set_item(result, 1, part);
      tmp = pmath_expr_set_item(result, 0, PMATH_C_STRING("\""));
      result = pmath_expr_get_item_range(tmp, 0, len + 1);
      pmath_unref(tmp);
    }
    
    result = pmath_expr_set_item(result, 0, pmath_ref(pmath_System_StringBox));
  }
  else if(pmath_is_string(result)) {
    result = _pmath_escape_string(
      PMATH_C_STRING("\""), 
      result, 
      PMATH_C_STRING("\""), 
      thread->boxform >= BOXFORM_INPUT);
      
    result = pmath_expr_new_extended(
             pmath_ref(pmath_System_StringBox), 1,
             result);
  }
  
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_InterpretationBox), 2,
           result,
           pmath_ref(stringform));
}
