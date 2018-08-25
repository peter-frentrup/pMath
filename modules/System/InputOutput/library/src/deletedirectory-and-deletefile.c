#include "stdafx.h"


extern pmath_symbol_t pmath_System_DeleteContents;
extern pmath_symbol_t pmath_System_Failed;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_True;

#ifndef PMATH_OS_WIN32
static pmath_bool_t delete_file_or_dir(
  const char       *name,
  pmath_bool_t   recursive
) {
  pmath_bool_t result = TRUE;

  if(pmath_aborting())
    return FALSE;

  if(recursive) {
    DIR *dir = opendir(name);
    if(dir) {
      size_t len = strlen(name);
      size_t new_size = len + 2;
      char *new_name = pmath_mem_alloc(new_size);

      if(new_name) {
        struct dirent *entry;

        strcpy(new_name, name);

        new_name[len] = '/';
        new_name[++len] = '\0';

        while(0 != (entry = readdir(dir))) {
          if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            size_t elen = strlen(entry->d_name);

            if(new_size < len + elen + 1) {
              new_name = pmath_mem_realloc(new_name, len + elen + 1);
              new_size = len + elen + 1;
            }

            if(!new_name) {
              result = FALSE;
              break;
            }

            strcpy(new_name + len, entry->d_name);

            result = delete_file_or_dir(new_name, TRUE);
            if(!result)
              break;
          }
        }
      }
      else
        result = FALSE;

      pmath_mem_free(new_name);
    }
  }

  if(result)
    return remove(name) == 0;
  return FALSE;
}
#endif

static pmath_t delete_directory_or_file(pmath_expr_t expr, pmath_bool_t is_directory);

PMATH_PRIVATE pmath_t eval_System_DeleteDirectory(pmath_expr_t expr) {
  return delete_directory_or_file(expr, TRUE);
}

PMATH_PRIVATE pmath_t eval_System_DeleteFile(pmath_expr_t expr) {
  return delete_directory_or_file(expr, FALSE);
}

static pmath_t delete_directory_or_file(pmath_expr_t expr, pmath_bool_t is_directory) {
  /* DeleteDirectory(name)
     DeleteFile(name)

     Options:
      DeleteContents

     Messages:
      General::fstr
      General::ioarg
      General::opttf

      DeleteDirectory::dirne
      DeleteDirectory::nodir

      DeleteFile::fdir
   */
  pmath_bool_t delete_contents = FALSE;
  pmath_t name;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);

  if(pmath_expr_length(expr) != 1) {
    if( !is_directory || pmath_expr_length(expr) < 1)
    {
      pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
      return expr;
    }
  }

  if(is_directory) {
    pmath_t options = pmath_options_extract(expr, 1);
    if(pmath_is_null(options))
      return expr;

    name = pmath_option_value(PMATH_NULL, pmath_System_DeleteContents, options);
    pmath_unref(options);
    if(pmath_same(name, pmath_System_True)) {
      delete_contents = TRUE;
    }
    else if(!pmath_same(name, pmath_System_False)) {
      pmath_message(PMATH_NULL, "opttf", 2, pmath_ref(pmath_System_DeleteContents), name);
      return expr;
    }

    pmath_unref(name);
  }

  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name) || pmath_string_length(name) == 0) {
    pmath_message(PMATH_NULL, "fstr", 1, name);
    return expr;
  }
  
  name = pmath_to_absolute_file_name(name);

#ifdef PMATH_OS_WIN32
  {
    static const uint16_t zerozero[2] = {0, 0};
    pmath_string_t abs_name = pmath_ref(name);

    if(!pmath_is_null(abs_name)) {
      abs_name = pmath_string_insert_ucs2(abs_name, INT_MAX, zerozero, 2);
    }

    if(!pmath_is_null(abs_name)) {
      HANDLE h = CreateFileW(
                   (const wchar_t *)pmath_string_buffer(&abs_name),
                   0,
                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                   NULL,
                   OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS,
                   NULL);

      if(h != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info;

        if( GetFileInformationByHandle(h, &info) &&
            ((is_directory && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ||
             (!is_directory && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))))
        {
          DWORD err = 0;
          CloseHandle(h);

          if(delete_contents) {
            SHFILEOPSTRUCTW op;
            memset(&op, 0, sizeof(op));
            op.wFunc  = FO_DELETE;
            op.pFrom  = (const wchar_t *)pmath_string_buffer(&abs_name);
            op.pTo    = (const wchar_t *)zerozero;
            op.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT;

            err = (DWORD)SHFileOperationW(&op);
          }
          else if(is_directory) {
            if(!RemoveDirectoryW((const wchar_t *)pmath_string_buffer(&abs_name)))
              err = GetLastError();
          }
          else {
            if(!DeleteFileW((const wchar_t *)pmath_string_buffer(&abs_name)))
              err = GetLastError();
          }

          switch(err) {
            case 0:
              pmath_unref(name);
              name = PMATH_NULL;
              break;

            case ERROR_ACCESS_DENIED:
              pmath_message(PMATH_NULL, "privv", 1, expr);
              expr = PMATH_NULL;
              pmath_unref(name);
              name = pmath_ref(pmath_System_Failed);

            case ERROR_DIR_NOT_EMPTY:
              pmath_message(PMATH_NULL, "dirne", 1, name);
              name = PMATH_NULL;
              break;

            default:
              pmath_message(PMATH_NULL, "ioarg", 1, expr);
              expr = PMATH_NULL;
              pmath_unref(name);
              name = pmath_ref(pmath_System_Failed);
          }
        }
        else {
          switch(GetLastError()) {
            case ERROR_ACCESS_DENIED:
              pmath_message(PMATH_NULL, "privv", 1, expr);
              expr = PMATH_NULL;
              break;

            default:
              if(is_directory) {
                pmath_message(PMATH_NULL, "nodir", 1, name);
                name = PMATH_NULL;
              }
              else {
                pmath_message(PMATH_NULL, "fdir", 1, name);
                name = PMATH_NULL;
              }
          }
          pmath_unref(name);
          name = pmath_ref(pmath_System_Failed);
          CloseHandle(h);
        }
      }
      else {
        switch(GetLastError()) {
          case ERROR_ACCESS_DENIED:
            pmath_message(PMATH_NULL, "privv", 1, expr);
            expr = PMATH_NULL;
            break;

          default:
            if(is_directory) {
              pmath_message(PMATH_NULL, "nodir", 1, name);
              name = PMATH_NULL;
            }
            else {
              pmath_message(PMATH_NULL, "nffil", 1, expr);
              expr = PMATH_NULL;
            }
        }
        pmath_unref(name);
        name = pmath_ref(pmath_System_Failed);
      }
    }

    pmath_unref(abs_name);
  }
#else
  {
    char *str = pmath_string_to_native(name, NULL);

    if(str) {
      errno = 0;
      if(delete_file_or_dir(str, delete_contents)) {
        pmath_unref(name);
        name = PMATH_NULL;
      }
      else {
        switch(errno) {
          case EACCES:
          case EPERM:
            pmath_message(PMATH_NULL, "privv", 1, expr);
            expr = PMATH_NULL;
            break;

          case ENOTEMPTY:
          case EEXIST:
            pmath_message(PMATH_NULL, "dirne", 1, name);
            name = PMATH_NULL;
            break;

          case ENOENT:
          case ENOTDIR:
            if(is_directory) {
              pmath_message(PMATH_NULL, "nodir", 1, name);
              name = PMATH_NULL;
            }
            else {
              pmath_message(PMATH_NULL, "nffil", 1, expr);
              expr = PMATH_NULL;
            }
            break;

          default:
            pmath_message(PMATH_NULL, "ioarg", 1, expr);
            expr = PMATH_NULL;
            break;
        }

        pmath_unref(name);
        name = pmath_ref(pmath_System_Failed);
      }

      pmath_mem_free(str);
    }
  }
#endif

  pmath_unref(expr);
  return name;
}
