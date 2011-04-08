#include <pmath-core/strings-private.h>

#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/formating-private.h>

#include <string.h>

PMATH_PRIVATE
pmath_t _pmath_expand_string( // result is string or expression
  pmath_string_t string // will be freed
){
  const uint16_t *buf = pmath_string_buffer(&string);
  int len = pmath_string_length(string);
  
  while(len-- > 0)
    if(*buf++ == PMATH_CHAR_LEFT_BOX)
      goto HAVE_STH_TO_EXPAND;
  
  return string;
  
 HAVE_STH_TO_EXPAND:
  {
    int start, i;
    buf = pmath_string_buffer(&string);
    len = pmath_string_length(string);
    
    start = i = 0;
    
    pmath_gather_begin(PMATH_NULL);
    
    while(i < len){
      if(buf[i] == PMATH_CHAR_LEFT_BOX){
        int k;
        
        if(i > start){
          pmath_emit(
            pmath_string_part(pmath_ref(string), start, i - start),
            PMATH_NULL);
        }
        
        start = ++i;
        k = 1;
        while(i < len){
          if(buf[i] == PMATH_CHAR_LEFT_BOX){
            ++k;
          }
          else if(buf[i] == PMATH_CHAR_RIGHT_BOX){
            if(--k == 0)
              break;
          }
          ++i;
        }
        
        pmath_emit(
          pmath_parse_string(
            pmath_string_part(
              pmath_ref(string), 
              start, 
              i - start)),
          PMATH_NULL);
        
        if(i < len)
          ++i;
          
        start = i;
      }
      else
        ++i;
    }
    
    if(start < len){
      pmath_emit(
        pmath_string_part(pmath_ref(string), start, len - start),
        PMATH_NULL);
    }
    
    pmath_unref(string);
    
    {
      pmath_expr_t result = pmath_gather_end();
      
      if(pmath_expr_length(result) == 1){
        string = pmath_expr_get_item(result, 1);
        pmath_unref(result);
        return string;
      }
      
      return result;
    }
  }
}

static pmath_bool_t has_param_placeholders(
  pmath_t box // wont be freed
){
  if(pmath_is_expr(box)){
    size_t i = pmath_expr_length(box);
    for(;i > 0;--i){
      pmath_t item = pmath_expr_get_item(box, i);
      pmath_bool_t result = has_param_placeholders(item);
      pmath_unref(item);
      
      if(result)
        return result;
    }
  }
  else if(pmath_is_string(box)){
    const uint16_t *buf = pmath_string_buffer(&box);
    int len = pmath_string_length(box);
    int i;
    
    for(i = 0;i < len - 1;++i)
      if(buf[i] == '`'){
        do{
          ++i;
        }while(i < len - 1 && buf[i] >= '0' && buf[i] <= '9');
          
        if(buf[i] == '`')
          return TRUE;
      }
  }
  
  return FALSE;
}

typedef struct{
  pmath_expr_t formated_params;
  size_t             current_param;
}_format_data_t;

static pmath_t string_form(
  pmath_t  format,  // will be freed
  _format_data_t *data
){
  if(!has_param_placeholders(format))
    return format;
  
  if(pmath_is_string(format)){
    format = pmath_string_expand_boxes(format);
    
    if(pmath_is_string(format)){
      const uint16_t *buf = pmath_string_buffer(&format);
      int len = pmath_string_length(format);
      int start = 0;
      int i = 0;
      
      pmath_gather_begin(PMATH_NULL);
      
      while(i < len - 1){
        if(buf[i] == '`'){
          if(buf[i + 1] == '`'){
            if(start < i){
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
            
            start = i+= 2;
          }
          else{
            #define NUMLEN 20
            int k = i;
            
            do{
              ++k;
            }while(k < len - 1 && buf[k] >= '0' && buf[k] <= '9');
            
            if(buf[k] == '`' && i + 1 < k && k <= i + NUMLEN + 1){
              char num[NUMLEN];
              int end = i;
              size_t param;
              
              if(start < i){
                pmath_emit(
                  pmath_string_part(
                    pmath_ref(format),
                    start,
                    end - start),
                  PMATH_NULL);
              }
              
              start = 0;
              do{
                num[start++] = (char)(unsigned char)buf[++i];
              }while(i < k - 1);
              num[start] = '\0';
              
              param = (size_t)atoi(num);
              
              if(param > 0 && param <= pmath_expr_length(data->formated_params)){
                data->current_param = param;
                
                pmath_emit(
                  pmath_expr_get_item(
                    data->formated_params, 
                    data->current_param++),
                  PMATH_NULL);
                
                start = i = k + 1;
              }
              else{
                start = end;
                i = k+1;
              }
            }
            else
              ++i;
          }
        }
        else
          ++i;
      }
      
      if(start < len){
        pmath_emit(
          pmath_string_part(
            pmath_ref(format),
            start,
            len - start),
          PMATH_NULL);
      }
      
      pmath_unref(format);
      format = pmath_gather_end();
      if(pmath_expr_length(format) == 1){
        pmath_t result = pmath_expr_get_item(format, 1);
        
        pmath_unref(format);
        return result;
      }
      
      return format;
    }
  }
    
  if(pmath_is_expr(format)){
    size_t i;
    for(i = 1;i < pmath_expr_length(format);++i)
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
  pmath_expr_t            stringform, // wont be freed
  pmath_write_options_t   options,
  void                  (*write)(void*,const uint16_t*,int),
  void                   *user
){
  _format_data_t  data;
  pmath_t  result;
  pmath_string_t  format;
  size_t i;
  
  if(pmath_is_null(stringform))
    return FALSE;
  
  assert(pmath_is_expr(stringform));
  
  format = pmath_expr_get_item(stringform, 1);
  if(!pmath_is_string(format)){
    pmath_unref(format);
    return FALSE;
  }
  
  data.formated_params = pmath_expr_get_item_range(stringform, 2, SIZE_MAX);
  
  for(i = 1;i <= pmath_expr_length(data.formated_params);++i){
    pmath_t item = pmath_expr_get_item(data.formated_params, i);
    pmath_string_t str = PMATH_NULL;
    
    pmath_write(
      item,
      options,
      (void(*)(void*,const uint16_t*,int))_pmath_write_to_string,
      &str);
    
    pmath_unref(item);
    
    if(pmath_is_null(str))
      str = pmath_string_new(0);
    
    data.formated_params = pmath_expr_set_item(
      data.formated_params, i, 
      str);
  }
  
  data.current_param = 1;
  
  result = string_form(format, &data);
  
  pmath_unref(data.formated_params);
  
  if(pmath_is_string(result)){
    pmath_write(
      result,
      options,
      write,
      user);
  }
  else if(pmath_is_expr(result)){
    size_t i;
    
    for(i = 1;i <= pmath_expr_length(result);++i){
      pmath_t item = pmath_expr_get_item(result, i);
      
      pmath_write(
        item,
        options,
        write,
        user);
      
      pmath_unref(item);
    }
  }
  
  pmath_unref(result);
  return TRUE;
}

PMATH_PRIVATE 
pmath_t _pmath_stringform_to_boxes(
  pmath_expr_t  stringform // wont be freed
){
  _format_data_t  data;
  pmath_t  result;
  pmath_string_t  format;
  size_t i;

  if(pmath_is_null(stringform))
    return PMATH_NULL;
  
  assert(pmath_is_expr(stringform));
  
  format = pmath_expr_get_item(stringform, 1);
  if(!pmath_is_string(format)){
    pmath_unref(format);
    return PMATH_NULL;
  }
  
  data.formated_params = pmath_expr_get_item_range(stringform, 2, SIZE_MAX);
  
  for(i = 1;i <= pmath_expr_length(data.formated_params);++i){
    pmath_t item = pmath_expr_get_item(data.formated_params, i);
    data.formated_params = pmath_expr_set_item(
      data.formated_params, i, PMATH_NULL);
    
    item = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_LIST), 1,
      pmath_evaluate(
        pmath_expr_new_extended(
          pmath_ref(PMATH_SYMBOL_MAKEBOXES), 1,
          item)));
    
    data.formated_params = pmath_expr_set_item(
      data.formated_params, i, item);
  }
  
  data.current_param = 1;
  
  result = string_form(format, &data);
  
  pmath_unref(data.formated_params);
  
  if(pmath_is_expr(result)){
    struct _pmath_string_t *all;
    uint16_t *buf;
    int rlen = 2;
    
    for(i = 1;i <= pmath_expr_length(result);++i){
      pmath_t item = pmath_expr_get_item(result, i);
      pmath_string_t nitem = PMATH_NULL;
      
      if(pmath_is_string(item)){
        nitem = _pmath_string_escape(PMATH_NULL, item, PMATH_NULL, FALSE);
      }
      else{
        static const uint16_t left  = PMATH_CHAR_LEFT_BOX;
        static const uint16_t right = PMATH_CHAR_RIGHT_BOX;
        nitem = pmath_string_insert_ucs2(PMATH_NULL, 0, &left, 1);
        
        if(pmath_is_expr_of_len(item, PMATH_SYMBOL_LIST, 1)){
          pmath_expr_t tmp = item;
          item = pmath_expr_get_item(tmp, 1);
          pmath_unref(tmp);
        }
        
        pmath_write(
          item,
            PMATH_WRITE_OPTIONS_FULLSTR
          | PMATH_WRITE_OPTIONS_INPUTEXPR,
          (void(*)(void*,const uint16_t*,int))_pmath_write_to_string,
          &nitem);
        
        nitem = pmath_string_insert_ucs2(
          nitem, 
          pmath_string_length(nitem), 
          &right, 
          1);
        
        pmath_unref(item);
      }
      
      rlen+= pmath_string_length(nitem);
      result = pmath_expr_set_item(result, i, nitem);
    }
    
    all = _pmath_new_string_buffer(rlen);
    if(all){
      buf = AFTER_STRING(all);
      buf[0] = '"';
      
      rlen = 1;
      for(i = 1;i <= pmath_expr_length(result);++i){
        pmath_string_t si = pmath_expr_get_item(result, i);
        
        memcpy(
          buf + rlen,
          pmath_string_buffer(&si),
          sizeof(uint16_t) * pmath_string_length(si));
        
        rlen+= pmath_string_length(si);
        
        pmath_unref(si);
      }
      
      assert(rlen + 1 == all->length);
      
      buf[rlen] = '"';
      pmath_unref(result);
      result = _pmath_from_buffer(all);
    }
  }
  else if(pmath_is_string(result)){
    pmath_string_t quote = PMATH_C_STRING("\"");
    
    result = _pmath_string_escape(
      pmath_ref(quote),
      result,
      pmath_ref(quote),
      FALSE);
      
    pmath_unref(quote);
  }
    
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_INTERPRETATIONBOX), 2,
    result,
    pmath_ref(stringform));
}

PMATH_PRIVATE
pmath_string_t _pmath_string_escape(
  pmath_string_t  prefix,   // will be freed
  pmath_string_t  string,   // will be freed
  pmath_string_t  postfix,  // will be freed
  pmath_bool_t    two_times
){
  struct _pmath_string_t *result;
  const uint16_t *buf;
  uint16_t *rbuf;
  int len, rlen, i, k;
  
  buf = pmath_string_buffer(&string);
  len = pmath_string_length(string);
  
  rlen = 0;
  k = 0;
  for(i = 0;i < len;++i){
    if(k == 0 && (buf[i] == '"' || buf[i] == '\\')){
      rlen+= two_times ? 4 : 2;
    }
    else{
      ++rlen;
      
      if(buf[i] == PMATH_CHAR_LEFT_BOX)
        ++k;
      else if(buf[i] == PMATH_CHAR_RIGHT_BOX)
        --k;
    }
  }
  
  if(rlen == len)
    return pmath_string_concat(pmath_string_concat(prefix, string), postfix);
  
  result = _pmath_new_string_buffer(
    rlen + 
    pmath_string_length(prefix) + 
    pmath_string_length(postfix));
    
  if(!result){
    pmath_unref(string);
    pmath_unref(prefix);
    pmath_unref(postfix);
    return PMATH_NULL;
  }
  
  rlen = pmath_string_length(prefix);
  
  rbuf = AFTER_STRING(result);
  memcpy(
    rbuf, 
    pmath_string_buffer(&postfix), 
    sizeof(uint16_t) * rlen);
  pmath_unref(prefix);
  
  k = 0;
  for(i = 0;i < len;++i){
    if(k == 0 && (buf[i] == '"' || buf[i] == '\\')){
      rbuf[rlen++] = '\\';
      
      if(two_times){
        rbuf[rlen++] = '\\';
        rbuf[rlen++] = '\\';
      }
    }
    else if(buf[i] == PMATH_CHAR_LEFT_BOX)
      ++k;
    else if(buf[i] == PMATH_CHAR_RIGHT_BOX)
      --k;
      
    rbuf[rlen++] = buf[i];
  }
  
  pmath_unref(string);
  
  assert(rlen + pmath_string_length(postfix) == result->length);
  
  memcpy(
    rbuf + rlen, 
    pmath_string_buffer(&postfix), 
    sizeof(uint16_t) * pmath_string_length(postfix));
  pmath_unref(postfix);
  
  return _pmath_from_buffer(result);
}
