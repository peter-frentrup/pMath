#include <pmath-util/files/filesystem.h>

#include <pmath-builtins/io-private.h>


#ifdef PMATH_OS_WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOGDI
#  include <Windows.h>
#else
#  include <errno.h>
#  include <limits.h>
#  include <pwd.h>
#  include <string.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif


#ifndef PMATH_OS_WIN32
static int first_slash(const uint16_t *buf, int len) {
  int i = 0;
  for(i = 0; i < len; ++i)
    if(buf[i] == '/')
      return i;

  return len;
}

static int last_slash(const uint16_t *buf, int len) {
  int i = 0;
  for(i = len - 1; i >= 0; --i)
    if(buf[i] == '/')
      return i;

  return -1;
}
#endif

PMATH_API
pmath_string_t pmath_to_absolute_file_name(pmath_string_t relname) {
  if(pmath_string_length(relname) == 0)
    return relname;

#ifdef PMATH_OS_WIN32
  {
    DWORD reslen;
    wchar_t *buffer;
    pmath_string_t result;

    relname = pmath_string_insert_latin1(relname, INT_MAX, "", 1);
    if(pmath_is_null(relname))
      return PMATH_NULL;

    reslen = GetFullPathNameW(pmath_string_buffer(&relname), 0, NULL, NULL);
    if((int)reslen <= 0) {
      pmath_unref(relname);
      return PMATH_NULL;
    }

    result = pmath_string_new_raw(reslen);
    if(pmath_string_begin_write(&result, &buffer, NULL)) {
      reslen = GetFullPathNameW(
        pmath_string_buffer(&relname),
        reslen,
        buffer,
        NULL);
      
      pmath_string_end_write(&result, &buffer);
    }
    else
      reslen = 0;
    
    pmath_unref(relname);
    return pmath_string_part(result, 0, reslen);
  }
#else
  {
    pmath_string_t result;
    pmath_string_t dir;
    int dirlen, namelen, enddir, startname;
    int             rellen = pmath_string_length(relname);
    const uint16_t *ibuf   = pmath_string_buffer(&relname);
    uint16_t       *obuf;

    if(ibuf[0] == '/') {
      dir = PMATH_C_STRING("");//pmath_string_part(pmath_ref(relname), 0, 1);
      relname = pmath_string_part(relname, 1, INT_MAX);
    }
    else if(ibuf[0] == '~') {
      const char *prefix = 0;

      int slashpos = 1;
      while(slashpos < rellen && ibuf[slashpos] != '/')
        ++slashpos;

      if(slashpos == 1) { // "~/rest/of/path"
        prefix = getenv("HOME");
        if(!prefix) {
          struct passwd *pw = getpwuid(getuid());
          if(pw)
            prefix = pw->pw_dir;
        }
      }
      else { // "~user/rest/of/path"
        pmath_string_t user = pmath_string_part(pmath_ref(relname), 1, slashpos - 1);
        char *user_s = pmath_string_to_native(user, 0);

        if(user_s) {
          struct passwd *pw = getpwnam(user_s);
          if(pw)
            prefix = pw->pw_dir;
        }

        pmath_mem_free(user_s);
        pmath_unref(user);
      }

      if(prefix) {
        dir = pmath_string_from_native(prefix, -1);
        relname = pmath_string_part(relname, rellen + 1, INT_MAX); // all after "/"
      }
      else {
        dir = PMATH_C_STRING("");
        relname = pmath_string_part(relname, rellen + 1, INT_MAX); // all after "/"
      }
    }
    else
      dir = _pmath_get_directory();

    if(pmath_is_null(dir)) {
      pmath_unref(relname);
      return PMATH_NULL;
    }

    dirlen  = pmath_string_length(dir);
    namelen = pmath_string_length(relname);

    result = pmath_string_new_raw(dirlen + namelen + 1);
    if(pmath_string_begin_write(&result, &obuf, NULL)) {
      memcpy(obuf, pmath_string_buffer(&dir), sizeof(uint16_t) * dirlen);
      obuf[dirlen] = '/';
      memcpy(obuf + dirlen + 1, pmath_string_buffer(&relname), sizeof(uint16_t) * namelen);
      
      enddir = startname = dirlen + 1;
      namelen = dirlen + namelen + 1;

      while(startname < namelen) {
        int next = startname + first_slash(obuf + startname, namelen - startname);

        if( next == startname + 2      &&
            obuf[startname]     == '.' &&
            obuf[startname + 1] == '.')
        {
          enddir = last_slash(obuf, enddir - 1) + 1;
          if(enddir < 0)
            enddir = 0;
        }
        else if(next != startname + 1 || obuf[startname] != '.') {
          memmove(obuf + enddir, obuf + startname, (next - startname) * sizeof(uint16_t));
          enddir += next - startname;
          if(enddir < namelen) {
            obuf[enddir] = '/';
            ++enddir;
          }
        }

        startname = next + 1;
      }

      while(enddir > 1 && obuf[enddir - 1] == '/')
        --enddir;
        
      pmath_string_end_write(&result, &obuf);
    }
    else
      enddir = 0;
    
    pmath_unref(dir);
    pmath_unref(relname);
    
    return pmath_string_part(result, 0, enddir);
  }
#endif
}
