#include <pmath-util/files/text-from-binary.h>
#include <pmath-util/memory.h>

#include <errno.h>
#include <iconv.h>
#include <limits.h>


struct _bintext_extra_t {
  pmath_t binfile;
  
  pmath_string_t rest; // input buffer
  pmath_bool_t skip_nl;
  
  iconv_t  in_cd;
  iconv_t  out_cd;
};

static void destroy_bintext_extra(void *closure) {
  struct _bintext_extra_t *extra = closure;
  
  pmath_file_close_if_unused(extra->binfile);
  pmath_unref(extra->rest);
  
  if(extra->in_cd != (iconv_t) - 1)
    iconv_close(extra->in_cd);
    
  if(extra->out_cd != (iconv_t) - 1)
    iconv_close(extra->out_cd);
    
  pmath_mem_free(extra);
}

static pmath_files_status_t bintext_extra_status(void *closure) {
  struct _bintext_extra_t *extra = closure;
  pmath_files_status_t result = pmath_file_status(extra->binfile);
  
  if( result == PMATH_FILE_ENDOFFILE &&
      pmath_string_length(extra->rest) > 0)
  {
    result = PMATH_FILE_OK;
  }
  
  return result;
}

pmath_string_t bintext_extra_readln(void *closure) {
  struct _bintext_extra_t *extra = closure;
  pmath_string_t result = PMATH_NULL;
  
  uint16_t out[100];
  char in[100];
  
  int len, i;
  
  len = pmath_string_length(extra->rest);
  
  if(len) {
    const uint16_t *buf;
    
    buf = pmath_string_buffer(&extra->rest);
    
    i = 0;
    while(i < len) {
      if(buf[i] == '\n') {
        result = pmath_string_part(pmath_ref(extra->rest), 0, i);
        
        if(i == len - 1) {
          pmath_unref(extra->rest);
          extra->rest = PMATH_NULL;
        }
        else
          extra->rest = pmath_string_part(extra->rest, i + 1, -1);
          
        return result;
      }
      
      if(buf[i] == '\r') {
        result = pmath_string_part(pmath_ref(extra->rest), 0, i);
        
        if(i + 1 < len) {
          if(buf[i + 1] == '\n')
            ++i;
            
          if(i == len - 1) {
            pmath_unref(extra->rest);
            extra->rest = PMATH_NULL;
          }
          else
            extra->rest = pmath_string_part(extra->rest, i + 1, -1);
        }
        else {
          pmath_unref(extra->rest);
          extra->rest = PMATH_NULL;
          extra->skip_nl = TRUE;
        }
        
        return result;
      }
      
      ++i;
    }
    
    result = extra->rest;
    extra->rest = PMATH_NULL;
  }
  
  for(;;) {
    char *inbuf;
    char *outbuf;
    
    size_t inbytesleft;
    size_t outbytesleft;
    
    int chars_read;
    
    inbytesleft = pmath_file_read(extra->binfile, in, sizeof(in), FALSE);
    
    if(inbytesleft == 0)
      break;
      
    outbytesleft = sizeof(out);
    inbuf = in;
    outbuf = (char *)out;
    
    len = pmath_string_length(result);
    
    do {
      size_t ret = iconv(extra->in_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
      
      if(ret == (size_t) - 1) {
        if(errno == EILSEQ) { // invalid input byte
          if(outbytesleft > 1) {
            *((uint16_t *)outbuf) = *(uint8_t *)inbuf;
            
            outbuf += 2;
            outbytesleft -= 2;
          }
          
          ++inbuf;
          --inbytesleft;
        }
        else if(errno == EINVAL) { // incomplete input
          size_t more = 0;
          
          memmove(in, inbuf, inbytesleft);
          
          more = pmath_file_read(
                   extra->binfile,
                   in + inbytesleft,
                   1/*sizeof(inbuf) - inbytesleft*/,
                   FALSE);
                   
          inbuf = in;
          
          if(more == 0) {
            in[inbytesleft] = '\0';
            more = 1;
//            while(inbytesleft > 0){
//              *((uint16_t*)outbuf) = *(uint8_t*)inbuf;
//
//              outbuf+= 2;
//              outbytesleft-= 2;
//
//              ++inbuf;
//              ++inbytesleft;
//            }
          }
          
          inbytesleft += more;
        }
        else if(errno == E2BIG) { // output buffer too small
          chars_read = (int)(((size_t)outbuf - (size_t)out) / 2);
          
          if(chars_read > 0) {
            const uint16_t *str = out;
            
            if(extra->skip_nl) {
              extra->skip_nl = FALSE;
              
              if(str[0] == '\n') {
                ++str;
                --chars_read;
              }
            }
            
            result = pmath_string_insert_ucs2(result, INT_MAX, str, chars_read);
          }
          
          outbuf = (char *)out;
          outbytesleft = sizeof(out);
        }
      }
    } while(inbytesleft > 0);
    
    chars_read = (int)(((size_t)outbuf - (size_t)out) / 2);
    
    if(chars_read > 0) {
      const uint16_t *str = out;
      
      if(extra->skip_nl) {
        extra->skip_nl = FALSE;
        
        if(str[0] == '\n') {
          ++str;
          --chars_read;
        }
      }
      
      result = pmath_string_insert_ucs2(result, INT_MAX, str, chars_read);
      
      str = pmath_string_buffer(&result);
      chars_read = pmath_string_length(result);
      
      for(i = len; i < chars_read; ++i) {
        if(str[i] == '\n') {
          chars_read -= i + 1;
          if(chars_read)
            extra->rest = pmath_string_part(pmath_ref(result), i + 1, chars_read);
            
          return pmath_string_part(result, 0, i);
        }
        
        if(str[i] == '\r') {
          chars_read -= i + 1;
          if(chars_read > 0) {
            if(str[i + 1] == '\n') {
              if(chars_read > 1)
                extra->rest = pmath_string_part(pmath_ref(result), i + 2, chars_read - 1);
            }
            else
              extra->rest = pmath_string_part(pmath_ref(result), i + 1, chars_read);
          }
          else
            extra->skip_nl = TRUE;
            
          return pmath_string_part(result, 0, i);
        }
      }
    }
  }
  
  return result;
}

static pmath_bool_t bintext_extra_write(
  void           *closure,
  const uint16_t *str,
  int             len
) {
  struct _bintext_extra_t *extra = closure;
  pmath_bool_t result = TRUE;
  char buf[100];
  
  if(len < 0) {
    len = 0;
    while(str[len])
      ++len;
  }
  
  while(len > 0) {
    int restlen = len;
    
    len = 0;
    while(len < restlen && str[len] != '\r' && str[len] != '\n')
      ++len;
      
    restlen -= len;
    
    {
      char *inbuf = (char *)str;
      char *outbuf = buf;
      
      size_t inbytesleft = sizeof(uint16_t) * (size_t)len;
      size_t outbytesleft = sizeof(buf);
      
      while(inbytesleft > 0) {
        size_t ret = iconv(extra->out_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
        
        if(ret == (size_t) - 1) {
          if(errno == EILSEQ) { // invalid input byte
            if(outbytesleft > 1) {
              *outbuf = '?';
              
              ++outbuf;
              --outbytesleft;
            }
            
            if(inbytesleft == 1) {
              ++inbuf;
              --inbytesleft;
            }
            else {
              inbuf += 2;
              inbytesleft -= 2;
            }
          }
          else if(errno == EINVAL) { // incomplete input
            if(outbytesleft > 1) {
              *outbuf = '?';
              
              ++outbuf;
              --outbytesleft;
            }
            
            inbytesleft = 0;
          }
          else if(errno == E2BIG) { // output buffer too small
            result = pmath_file_write(
                       extra->binfile,
                       buf,
                       sizeof(buf)) || result;
                       
            outbuf = buf;
            outbytesleft = sizeof(buf);
          }
        }
      }
      
      if(outbytesleft < sizeof(buf)) {
        result = pmath_file_write(
                   extra->binfile,
                   buf,
                   sizeof(buf) - outbytesleft) || result;
      }
    }
    
    str += len;
    len = restlen;
    if(len > 0) {
      if(*str == '\r') {
        ++str;
        --len;
      }
      
      if(len > 0 && *str == '\n') {
        ++str;
        --len;
      }
      
#ifdef PMATH_OS_WIN32
      result = pmath_file_write(extra->binfile, "\r\n", 2) || result;
#else
      result = pmath_file_write(extra->binfile, "\n", 1) || result;
#endif
    }
  }
  
  return result;
}

static void bintext_extra_flush(void *closure) {
  struct _bintext_extra_t *extra = closure;
  
  pmath_file_flush(extra->binfile);
}

/*static int64_t bintext_extra_get_pos(void *closure) {
  struct _bintext_extra_t *extra = closure;

  return pmath_file_get_position(extra->binfile);
}*/

PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
PMATH_ATTRIBUTE_NONNULL(2)
pmath_symbol_t pmath_file_create_text_from_binary(
  pmath_t      binfile,
  const char  *encoding
) {
  struct _bintext_extra_t *extra;
  pmath_text_file_api_t api;
  pmath_t result;
  
  if(!pmath_file_test(binfile, PMATH_FILE_PROP_BINARY)) {
    pmath_unref(binfile);
    return PMATH_NULL;
  }
  
  extra = (struct _bintext_extra_t *)pmath_mem_alloc(sizeof(struct _bintext_extra_t));
  
  if(!extra) {
    pmath_unref(binfile);
    return PMATH_NULL;
  }
  
  extra->binfile = binfile;
  extra->rest    = PMATH_NULL;
  extra->skip_nl = FALSE;
  extra->in_cd   = (iconv_t) - 1;
  extra->out_cd  = (iconv_t) - 1;
  
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  if(pmath_file_test(binfile, PMATH_FILE_PROP_BINARY | PMATH_FILE_PROP_READ)) {
    extra->in_cd = iconv_open(PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE", encoding);
    
    if(extra->in_cd == (iconv_t) - 1) {
      pmath_unref(binfile);
      pmath_mem_free(extra);
      return PMATH_NULL;
    }
    
    api.status_function  = bintext_extra_status;
    api.readln_function  = bintext_extra_readln;
    //api.get_pos_function = bintext_extra_get_pos;
  }
  
  if(pmath_file_test(binfile, PMATH_FILE_PROP_BINARY | PMATH_FILE_PROP_WRITE)) {
    extra->out_cd = iconv_open(encoding, PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
    
    if(extra->out_cd == (iconv_t) - 1) {
      pmath_unref(binfile);
      
      if(extra->in_cd != (iconv_t) - 1)
        iconv_close(extra->in_cd);
        
      pmath_mem_free(extra);
      return PMATH_NULL;
    }
    else {
      /* For some strange reason, we have to call iconv once, otherwise writing
         the byte order mark (see below) will fail (if encoding is utf8/...).
       */
      static const uint16_t inbuf = 'a';
      char outbuf[10];
      char *in  = (char *)&inbuf;
      char *out = outbuf;
      size_t inleft  = sizeof(inbuf);
      size_t outleft = sizeof(outbuf);
      
      iconv(extra->out_cd, &in, &inleft, &out, &outleft);
    }
    
    api.write_function   = bintext_extra_write;
    api.flush_function   = bintext_extra_flush;
    //api.get_pos_function = bintext_extra_get_pos;
  }
  
  result = pmath_file_create_text(
              extra,
              destroy_bintext_extra,
              &api);
              
  if(!pmath_is_null(result)) {
    if(api.readln_function) { // skip byte order mark
      pmath_string_t line = pmath_file_readline(result);
      
      if( pmath_string_length(line) > 0 &&
          *pmath_string_buffer(&line) == 0xFEFF)
      {
        line = pmath_string_part(line, 1, -1);
      }
      
      pmath_file_set_textbuffer(result, line);
    }
  }
  
  return result;
}
