#include <pmath-language/scanner.h>

#include <pmath-util/debug.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/files.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/io-private.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <iconv.h>

#ifdef PMATH_OS_WIN32
#  define NOGDI
#  define WIN32_LEAN_AND_MEAN
#  include <fcntl.h>
#  include <io.h>
#  include <windows.h>
#else
#  include <langinfo.h>
#  include <locale.h>
#endif

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


static pmath_bool_t eq_caseless(const char *s1, const char *s2) {
  while(*s1 && *s2) {
    if(tolower(*s1++) != tolower(*s2++))
      return FALSE;
  }
  
  return *s1 == *s2;
}

PMATH_PRIVATE pmath_t builtin_open(pmath_expr_t expr);

enum open_kind {
  OPEN_READ,
  OPEN_WRITE,
  OPEN_APPEND
};

static void bin_file_destroy(void *h) {
  FILE *file = h;
  
  fclose(file);
}

static pmath_files_status_t bin_file_status(void *h) {
  FILE *file = h;
  
  if(feof(file))
    return PMATH_FILE_ENDOFFILE;
    
  if(ferror(file))
    return PMATH_FILE_OTHERERROR;
    
  return PMATH_FILE_OK;
}

static size_t bin_file_read(void *h, void *buffer, size_t buffer_size) {
  FILE *file = h;
  
#ifdef _MSC_VER
  return _fread_nolock(buffer, 1, buffer_size, file);
#elif !defined( PMATH_OS_WIN32 )
  return fread_unlocked(buffer, 1, buffer_size, file);
#else
  return fread(buffer, 1, buffer_size, file);
#endif
}

static size_t bin_file_write(void *h, const void *buffer, size_t buffer_size) {
  FILE *file = h;
  
#ifdef _MSC_VER
  return _fwrite_nolock(buffer, 1, buffer_size, file);
#elif !defined( PMATH_OS_WIN32 )
  return fwrite_unlocked(buffer, 1, buffer_size, file);
#else
  return fwrite(buffer, 1, buffer_size, file);
#endif
}

static void bin_file_flush(void *h) {
  FILE *file = h;
  
#ifdef _MSC_VER
  _fflush_nolock(file);
#elif !defined( PMATH_OS_WIN32 )
  fflush_unlocked(file);
#else
  fflush(file);
#endif
}

static int64_t bin_file_get_pos(void *h) {
  FILE *file = h;
  
#ifdef _MSC_VER
  return _ftelli64_nolock(file);
#else
  return ftello64(file);
#endif
}

static pmath_t open_bin_file(
  pmath_string_t name, // will be freed
  enum open_kind kind
) {
  FILE *f = NULL;
  pmath_binary_file_api_t api;
  
  memset(&api, 0, sizeof(api));
  api.struct_size = sizeof(api);
  
  switch(kind) {
    case OPEN_READ:
      api.status_function  = bin_file_status;
      api.read_function    = bin_file_read;
      api.get_pos_function = bin_file_get_pos;
      break;
      
    case OPEN_WRITE:
    case OPEN_APPEND:
      api.write_function   = bin_file_write;
      api.flush_function   = bin_file_flush;
      api.get_pos_function = bin_file_get_pos;
      break;
  }
  
#ifdef PMATH_OS_WIN32
  {
    static const uint16_t zero = 0;
    const wchar_t *fmode = L"";
    int fdmode = -1;
    DWORD haccess = 0;
    DWORD hshare  = 0;
    DWORD hcredis = 0;
    DWORD hflags  = 0;
    int fd = -1;
    HANDLE h;
    
    name = pmath_string_insert_ucs2(
             name,
             pmath_string_length(name),
             &zero,
             1);
             
    if(pmath_is_null(name)) {
      pmath_unref(name);
      return PMATH_NULL;
    }
    
//    f = _wfopen((const wchar_t*)pmath_string_buffer(&name), mode);
    // _wfopen cannot open pipes.
    // But CreateFileW, _open_osfhandle, _wfdopen works.
    
    switch(kind) {
      case OPEN_READ:
        haccess = GENERIC_READ;
        hshare  = FILE_SHARE_READ;
        hcredis = OPEN_EXISTING;
        hflags = FILE_ATTRIBUTE_NORMAL;
        fdmode = _O_RDONLY;
        fmode = L"rb";
        break;
        
      case OPEN_WRITE:
        haccess = GENERIC_WRITE;
        hshare  = FILE_SHARE_READ;
        hcredis = CREATE_ALWAYS;
        hflags = FILE_ATTRIBUTE_NORMAL;
        fdmode = 0;
        fmode = L"wb";
        break;
        
      case OPEN_APPEND:
        haccess = GENERIC_WRITE;
        hshare  = FILE_SHARE_READ;
        hcredis = OPEN_ALWAYS;
        hflags = FILE_ATTRIBUTE_NORMAL;
        fdmode = _O_APPEND;
        fmode = L"ab";
        break;
    }
    
    h = CreateFileW(
          (const wchar_t *)pmath_string_buffer(&name),
          haccess,
          hshare,
          NULL,
          hcredis,
          hflags,
          NULL);
          
    fd = _open_osfhandle((intptr_t)h, fdmode);
    if(fd < 0)
      CloseHandle(h);
    else
      f = _wfdopen(fd, fmode);
  }
#else
  {
    char *fn = pmath_string_to_native(name, NULL);
  
    if(fn) {
      const char *mode = "";
      switch(kind) {
        case OPEN_READ:       mode = "rb"; break;
        case OPEN_WRITE:      mode = "wb"; break;
        case OPEN_APPEND:     mode = "ab"; break;
      }
  
      f = fopen(fn, mode);
  
      pmath_mem_free(fn);
    }
  }
#endif
  
  pmath_unref(name);
  
  if(!f)
    return PMATH_NULL;
    
  return pmath_file_create_binary(f, (void( *)(void *))bin_file_destroy, &api);
}

PMATH_PRIVATE pmath_bool_t _pmath_file_check(pmath_t file, int properties) {
  if(pmath_is_expr_of(file, PMATH_SYMBOL_LIST) &&
      (properties & OPEN_READ) == 0)
  {
    pmath_bool_t result = TRUE;
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(file); ++i) {
      pmath_t item = pmath_expr_get_item(file, i);
      
      result = _pmath_file_check(item, properties) || result;
      
      pmath_unref(item);
    }
    
    return result;
  }
  
  if(!pmath_file_test(file, properties)) {
    if(!pmath_file_test(file, 0)) {
      pmath_message(PMATH_NULL, "invio", 1, pmath_ref(file));
    }
    else if((properties & PMATH_FILE_PROP_READ) != 0 &&
            !pmath_file_test(file, PMATH_FILE_PROP_READ))
    {
      pmath_message(PMATH_NULL, "ior", 1, pmath_ref(file));
    }
    else if((properties & PMATH_FILE_PROP_WRITE) != 0 &&
            !pmath_file_test(file, PMATH_FILE_PROP_WRITE))
    {
      pmath_message(PMATH_NULL, "iow", 1, pmath_ref(file));
    }
    else if((properties & PMATH_FILE_PROP_BINARY) != 0 &&
            !pmath_file_test(file, PMATH_FILE_PROP_BINARY))
    {
      pmath_message(PMATH_NULL, "iob", 1, pmath_ref(file));
    }
    
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE pmath_bool_t _pmath_open_tmp(
  pmath_t *streams,
  int      properties
) {
  if( pmath_is_expr_of(*streams, PMATH_SYMBOL_LIST) &&
      (properties & OPEN_READ) == 0)
  {
    pmath_bool_t result = TRUE;
    size_t i;
    
    for(i = 1; i <= pmath_expr_length(*streams); ++i) {
      pmath_t item = pmath_expr_get_item(*streams, i);
      
      result = _pmath_open_tmp(&item, properties) || result;
      
      *streams = pmath_expr_set_item(*streams, i, item);
    }
    
    return result;
  }
  
  if(pmath_is_string(*streams)) {
    pmath_t head;
    pmath_t expr;
    
    if(properties & PMATH_FILE_PROP_READ)
      head = pmath_ref(PMATH_SYMBOL_OPENREAD);
    else
      head = pmath_ref(PMATH_SYMBOL_OPENWRITE);
      
    expr = pmath_expr_new_extended(
             head, 2,
             *streams,
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_RULE), 2,
               pmath_ref(PMATH_SYMBOL_BINARYFORMAT),
               pmath_ref((properties & PMATH_FILE_PROP_BINARY) ? PMATH_SYMBOL_TRUE : PMATH_SYMBOL_FALSE)));
               
    *streams = builtin_open(expr);
    
    if(pmath_same(*streams, PMATH_SYMBOL_FAILED))
      return FALSE;
      
    return TRUE;
  }
  
  if(!pmath_file_test(*streams, properties)) {
    if(!pmath_file_test(*streams, 0)) {
      pmath_message(PMATH_NULL, "invio", 1, pmath_ref(*streams));
    }
    else if((properties & PMATH_FILE_PROP_READ) != 0 &&
            !pmath_file_test(*streams, PMATH_FILE_PROP_READ))
    {
      pmath_message(PMATH_NULL, "ior", 1, pmath_ref(*streams));
    }
    else if((properties & PMATH_FILE_PROP_WRITE) != 0 && !pmath_file_test(*streams, PMATH_FILE_PROP_WRITE)) {
      pmath_message(PMATH_NULL, "iow", 1, pmath_ref(*streams));
    }
    else if((properties & PMATH_FILE_PROP_BINARY) != 0 &&
            !pmath_file_test(*streams, PMATH_FILE_PROP_BINARY))
    {
      pmath_message(PMATH_NULL, "iob", 1, pmath_ref(*streams));
    }
    
    return FALSE;
  }
  
  return TRUE;
}

PMATH_PRIVATE const char *_pmath_native_encoding = "ISO-8859-1";
PMATH_PRIVATE pmath_bool_t _pmath_native_encoding_is_utf8 = FALSE;

PMATH_PRIVATE void _init_pmath_native_encoding(void) {
  iconv_t cd;
  
#ifdef PMATH_OS_WIN32
  {
    DWORD codepage = GetACP();
    
    switch(codepage) {
      case 1200: _pmath_native_encoding = "UTF-16LE"; break;
      case 1201: _pmath_native_encoding = "UTF-16BE"; break;
      
      case 12000: _pmath_native_encoding = "UTF-32LE"; break;
      case 12001: _pmath_native_encoding = "UTF-32BE"; break;
      
      case 28591: _pmath_native_encoding = "ISO-8859-1"; break;
      case 28592: _pmath_native_encoding = "ISO-8859-2"; break;
      case 28593: _pmath_native_encoding = "ISO-8859-3"; break;
      case 28594: _pmath_native_encoding = "ISO-8859-4"; break;
      case 28595: _pmath_native_encoding = "ISO-8859-5"; break;
      case 28596: _pmath_native_encoding = "ISO-8859-6"; break;
      case 28597: _pmath_native_encoding = "ISO-8859-7"; break;
      case 28598: _pmath_native_encoding = "ISO-8859-8"; break;
      case 28599: _pmath_native_encoding = "ISO-8859-9"; break;
      
      case 28603: _pmath_native_encoding = "ISO-8859-13"; break;
      case 28605: _pmath_native_encoding = "ISO-8859-14"; break;
      
      case 65000: _pmath_native_encoding = "UTF-7"; break;
      
      case 65001:
        _pmath_native_encoding = "UTF-8";
        _pmath_native_encoding_is_utf8 = TRUE;
        break;
        
      default: {
          static char encoding[20];
          
          snprintf(encoding, sizeof(encoding), "CP%d", (int)codepage);
          
          _pmath_native_encoding = encoding;
        }
    }
    
    
    if(!SetConsoleCP(codepage)) {
      pmath_debug_print("SetConsoleCP failed.\n");
    }
    
    if(!SetConsoleOutputCP(codepage)) {
      pmath_debug_print("SetConsoleOutputCP failed.\n");
    }
  }
#else
  {
    const char *old_locale = setlocale(LC_CTYPE, "");
    _pmath_native_encoding = nl_langinfo(CODESET);
    setlocale(LC_CTYPE, old_locale);
  
    _pmath_native_encoding_is_utf8 = strcmp(_pmath_native_encoding, "UTF-8") == 0;
  }
#endif
  
  // ensure that iconv knows the encoding ...
  cd = iconv_open(
         _pmath_native_encoding,
         PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
         
  if(cd == (iconv_t)(-1)) {
    pmath_debug_print("unknown encoding %s\n", _pmath_native_encoding);
    _pmath_native_encoding = "ASCII";
  }
  else {
    iconv_close(cd);
    
    cd = iconv_open(
           PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE",
           _pmath_native_encoding);
           
    if(cd == (iconv_t)(-1)) {
      pmath_debug_print("unknown encoding %s\n", _pmath_native_encoding);
      _pmath_native_encoding = "ASCII";
    }
    else {
      iconv_close(cd);
    }
  }
}

PMATH_PRIVATE pmath_t builtin_close(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  if(!pmath_file_close(pmath_expr_get_item(expr, 1))) {
    pmath_message(
      PMATH_NULL, "invio", 1,
      pmath_expr_get_item(expr, 1));
    return expr;
  }
  
  pmath_unref(expr);
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_open(pmath_expr_t expr) {
  /* OpenAppend(filename)
     OpenRead(filename)
     OpenWrite(filename)
  
     OpenRead(binarystream)
     OpenWrite(binarystream)
   */
  pmath_expr_t options;
  pmath_t filename;
  pmath_t file;
  
  pmath_bool_t binary_format = TRUE;
  pmath_bool_t unbuffered = FALSE;
  pmath_string_t encoding = PMATH_NULL;
  pmath_t page_width = PMATH_NULL;
  enum open_kind kind;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(0, 1, SIZE_MAX);
    return expr;
  }
  
  filename = pmath_expr_get_item(expr, 1);
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) {
    pmath_unref(filename);
    return expr;
  }
  
  { // BinaryFormat
    pmath_t value = pmath_option_value(PMATH_NULL, PMATH_SYMBOL_BINARYFORMAT, options);
    
    if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
      binary_format = TRUE;
    }
    else if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
      binary_format = FALSE;
    }
    else {
      pmath_unref(filename);
      pmath_unref(options);
      pmath_message(
        PMATH_NULL, "opttf", 2,
        pmath_ref(PMATH_SYMBOL_BINARYFORMAT),
        value);
      return expr;
    }
    
    pmath_unref(value);
  }
  
  { // kind = OPEN_APPEND / OPEN_WRITE / OPEN_READ
    pmath_t head = pmath_expr_get_item(expr, 0);
    pmath_unref(head);
    
    if(pmath_same(head, PMATH_SYMBOL_OPENAPPEND))
      kind = OPEN_APPEND;
    else if(pmath_same(head, PMATH_SYMBOL_OPENWRITE))
      kind = OPEN_WRITE;
    else
      kind = OPEN_READ;
  }
  
  if(pmath_is_symbol(filename)) {
    if(kind == OPEN_APPEND) {
      pmath_message(PMATH_NULL, "fstr", 1, filename);
      return expr;
    }
    
    if(kind == OPEN_WRITE) {
      if(!pmath_file_test(filename, PMATH_FILE_PROP_WRITE)) {
        pmath_message(PMATH_NULL, "iow", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      if(!pmath_file_test(filename, PMATH_FILE_PROP_WRITE | PMATH_FILE_PROP_BINARY)) {
        pmath_message(PMATH_NULL, "iob", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      if(binary_format) {
        pmath_message(PMATH_NULL, "text", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
    else if(kind == OPEN_READ) {
      if(!pmath_file_test(filename, PMATH_FILE_PROP_READ)) {
        pmath_message(PMATH_NULL, "ior", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      if(!pmath_file_test(filename, PMATH_FILE_PROP_READ | PMATH_FILE_PROP_BINARY)) {
        pmath_message(PMATH_NULL, "iob", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
      
      if(binary_format) {
        pmath_message(PMATH_NULL, "text", 1, filename);
        pmath_unref(expr);
        return pmath_ref(PMATH_SYMBOL_FAILED);
      }
    }
  }
  else if(pmath_is_string(filename) && pmath_string_length(filename) >= 1) {
    filename = _pmath_canonical_file_name(filename);
  }
  else {
    pmath_message(PMATH_NULL, "fstr", 1, filename);
    return expr;
  }
  
  { // CharacterEncoding
    encoding = pmath_evaluate(
                 pmath_option_value(PMATH_NULL, PMATH_SYMBOL_CHARACTERENCODING, options));
                 
    if( !pmath_same(encoding, PMATH_SYMBOL_AUTOMATIC) &&
        !pmath_is_string(encoding))
    {
      pmath_message(PMATH_NULL, "charcode", 1, encoding);
      pmath_unref(filename);
      pmath_unref(options);
      return expr;
    }
  }
  
  if(kind != OPEN_READ) { // PageWidth
    page_width = pmath_evaluate(
                   pmath_option_value(PMATH_NULL, PMATH_SYMBOL_PAGEWIDTH, options));
  }
  
  if(kind == OPEN_READ && pmath_is_string(filename)) {
    pmath_t type = pmath_evaluate(
                     pmath_expr_new_extended(
                       pmath_ref(PMATH_SYMBOL_FILETYPE), 1,
                       pmath_ref(filename)));
                       
    pmath_unref(type);
    if(!pmath_same(type, PMATH_SYMBOL_FILE))
      unbuffered = TRUE;
  }
  
  if( !binary_format     &&
      !unbuffered        &&
      kind != OPEN_WRITE &&
      pmath_same(encoding, PMATH_SYMBOL_AUTOMATIC))
  { // check for byte order mark
    size_t count;
    uint8_t buf[4];
    
    memset(buf, 0, sizeof(buf));
    
    if(pmath_is_string(filename)) {
      pmath_t tmpfile;
      
      tmpfile = open_bin_file(pmath_ref(filename), OPEN_READ);
      count = pmath_file_read(tmpfile, buf, sizeof(buf), FALSE);
      pmath_file_close(tmpfile);
    }
    else {
      count = pmath_file_read(filename, buf, sizeof(buf), TRUE); /* preserve internal buffer */
    }
    
    pmath_unref(encoding);
    encoding = PMATH_NULL;
    if(buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
      encoding = PMATH_C_STRING("UTF-8");
    }
    else if(buf[0] == 0xFE && buf[1] == 0xFF) {
      encoding = PMATH_C_STRING("UTF-16BE");
    }
    else if(buf[0] == 0xFF && buf[1] == 0xFE) {
      if(count == 4 && buf[2] == 0 && buf[3] == 0)
        encoding = PMATH_C_STRING("UTF-32LE");
      else
        encoding = PMATH_C_STRING("UTF-16LE");
    }
    else if(count == 4     &&
            buf[0] == 0    &&
            buf[1] == 0    &&
            buf[2] == 0xFE &&
            buf[3] == 0xFF)
    {
      encoding = PMATH_C_STRING("UTF-32BE");
    }
  }
  
  if(!pmath_is_string(encoding)) {
    pmath_unref(encoding);
    encoding = pmath_evaluate(pmath_ref(PMATH_SYMBOL_CHARACTERENCODINGDEFAULT));
    
    if(!pmath_is_string(encoding)) {
      pmath_unref(encoding);
      encoding = PMATH_C_STRING("ASCII");
    }
  }
  
  if(pmath_is_string(filename)) {
    file = open_bin_file(pmath_ref(filename), kind);
  }
  else {
    file = filename;
    filename = pmath_evaluate(
                 pmath_parse_string_args(
                   "File.Replace(Join(Options(`1`), File->/\\/))",
                   "(0)",
                   pmath_ref(file)));
  }
  
  pmath_unref(options);
  
  if(pmath_is_null(file)) {
    pmath_message(
      PMATH_NULL, "noopen", 1,
      pmath_expr_get_item(expr, 1));
      
    pmath_unref(encoding);
    pmath_unref(page_width);
    pmath_unref(expr);
    return pmath_ref(PMATH_SYMBOL_FAILED);
  }
  
  if(unbuffered)
    pmath_file_set_binbuffer(file, 0);
    
  if(!binary_format) {
    const uint16_t *buf;
    char *str;
    int i, len;
    
    buf = pmath_string_buffer(&encoding);
    len = pmath_string_length(encoding);
    
    str = (char *)pmath_mem_alloc(len + 1);
    if(str) {
      for(i = 0; i < len; ++i) {
        str[i] = (char)(unsigned char)buf[i];
      }
      
      str[len] = '\0';
      
      file = pmath_file_create_text_from_binary(file, str);
      
      if(kind == OPEN_WRITE) {
        if( eq_caseless(str, "utf-8")  || // TODO: add Option to control UTF-8 BOM usage
            eq_caseless(str, "utf-16") ||
            eq_caseless(str, "utf-32"))
        {
          static const uint16_t byte_order_mark = 0xFEFF;
          
          pmath_file_writetext(file, &byte_order_mark, 1);
        }
      }
      
      pmath_mem_free(str);
    }
    else {
      pmath_unref(file);
      pmath_unref(encoding);
      pmath_unref(page_width);
      pmath_unref(expr);
      return PMATH_NULL;
    }
    
    if(pmath_is_null(file)) {
      pmath_message(
        PMATH_NULL, "noopen", 1,
        pmath_expr_get_item(expr, 1));
        
      pmath_unref(encoding);
      pmath_unref(page_width);
      pmath_unref(expr);
      return pmath_ref(PMATH_SYMBOL_FAILED);
    }
  }
  else {
    pmath_unref(encoding);
    encoding = PMATH_NULL;
  }
  
  PMATH_RUN_ARGS(
    "Unprotect(`1`);"
    "Options(`1`):= Union("
    " Options(`1`),"
    " If(`2` =!= /\\/, {File->`2`}, {}),"
    " If(`3` =!= /\\/, {PageWidth->`3`}, {}),"
    " If(`4` =!= /\\/, {CharacterEncoding->`4`}, {}));"
    "Protect(`1`)",
    "(oooo)",
    pmath_ref(file),
    pmath_ref(filename),
    pmath_ref(page_width),
    pmath_ref(encoding));
    
  pmath_unref(encoding);
  pmath_unref(filename);
  pmath_unref(page_width);
  pmath_unref(expr);
  return file;
}
