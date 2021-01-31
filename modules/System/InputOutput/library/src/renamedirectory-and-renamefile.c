#include "stdafx.h"


extern pmath_symbol_t pmath_System_DollarFailed;

static pmath_t rename_directory_or_file(pmath_expr_t expr, pmath_bool_t is_directory);

PMATH_PRIVATE pmath_t eval_System_RenameDirectory(pmath_expr_t expr) {
  return rename_directory_or_file(expr, TRUE);
}

PMATH_PRIVATE pmath_t eval_System_RenameFile(pmath_expr_t expr) {
  return rename_directory_or_file(expr, FALSE);
}

static pmath_t rename_directory_or_file(pmath_expr_t expr, pmath_bool_t is_directory) {
  /* RenameDirectory(name1, name2)
     RenameFiley(name1, name2)

     Messages:
      General::fstr
      General::ioarg

      RenameDirectory::dirne
      RenameDirectory::filex
      RenameDirectory::nodir

      RenameFile::fdir
      RenameFile::filex
   */
  pmath_t name1, name2;
  pmath_t head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);

  if(pmath_expr_length(expr) != 2) {
    pmath_message_argxxx(pmath_expr_length(expr), 2, 2);
    return expr;
  }

  name1 = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name1) || pmath_string_length(name1) == 0) {
    pmath_message(PMATH_NULL, "fstr", 1, name1);
    return expr;
  }

  name2 = pmath_expr_get_item(expr, 2);
  if(!pmath_is_string(name2) || pmath_string_length(name2) == 0) {
    pmath_message(PMATH_NULL, "fstr", 1, name2);
    pmath_unref(name1);
    return expr;
  }

#ifdef PMATH_OS_WIN32
  {
    const uint16_t zero = 0;

    name1 = pmath_string_insert_ucs2(name1, INT_MAX, &zero, 1);
    name2 = pmath_string_insert_ucs2(name2, INT_MAX, &zero, 1);

    if( !pmath_is_null(name1) &&
        !pmath_is_null(name2))
    {
      HANDLE h = CreateFileW(
                   (const wchar_t *)pmath_string_buffer(&name1),
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
          CloseHandle(h);

          if( MoveFileExW(
                (const wchar_t *)pmath_string_buffer(&name1),
                (const wchar_t *)pmath_string_buffer(&name2),
                MOVEFILE_COPY_ALLOWED))
          {
            name2 = pmath_string_part(name2, 0, pmath_string_length(name2) - 1);
            name2 = pmath_to_absolute_file_name(name2);
          }
          else {
            switch(GetLastError()) {
              case ERROR_ACCESS_DENIED:
                pmath_message(PMATH_NULL, "privv", 1, expr);
                expr = PMATH_NULL;
                break;

              case ERROR_FILE_EXISTS:
              case ERROR_ALREADY_EXISTS:
                pmath_message(PMATH_NULL, "filex", 1,
                              pmath_string_part(name2, 0, pmath_string_length(name2) - 1));
                name2 = PMATH_NULL;
                break;

              case ERROR_DIR_NOT_EMPTY:
                pmath_message(PMATH_NULL, "dirne", 1,
                              pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                name1 = PMATH_NULL;
                break;

              case ERROR_FILE_NOT_FOUND:
              case ERROR_PATH_NOT_FOUND:
                if(is_directory) {
                  pmath_message(PMATH_NULL, "nodir", 1,
                                pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                  name1 = PMATH_NULL;
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

            pmath_unref(name2);
            name2 = pmath_ref(pmath_System_DollarFailed);
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
                pmath_message(PMATH_NULL, "nodir", 1,
                              pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                name1 = PMATH_NULL;
              }
              else {
                pmath_message(PMATH_NULL, "fdir", 1,
                              pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
                name1 = PMATH_NULL;
              }
          }
          pmath_unref(name2);
          name2 = pmath_ref(pmath_System_DollarFailed);
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
              pmath_message(PMATH_NULL, "nodir", 1,
                            pmath_string_part(name1, 0, pmath_string_length(name1) - 1));
              name1 = PMATH_NULL;
            }
            else {
              pmath_message(PMATH_NULL, "nffil", 1, expr);
              expr = PMATH_NULL;
            }
        }
        pmath_unref(name2);
        name2 = pmath_ref(pmath_System_DollarFailed);
      }
    }
  }
#else
  {
    char   *str1 = pmath_string_to_native(name1, NULL);
    char   *str2 = pmath_string_to_native(name2, NULL);

    if(str1 && str2) {
      struct stat buf;

      if(stat(str2, &buf) == 0) {
        pmath_message(PMATH_NULL, "filex", 1, name2);
        name2 = pmath_ref(pmath_System_DollarFailed);
      }
      else {
        errno = 0;
        if( stat(str1, &buf) == 0 &&
            ((is_directory && S_ISDIR(buf.st_mode)) ||
             (!is_directory && !S_ISDIR(buf.st_mode))))
        {
          errno = 0;
          if(rename(str1, str2) == 0) {
            name2 = pmath_to_absolute_file_name(name2);
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
                pmath_message(PMATH_NULL, "dirne", 1, name2);
                name2 = PMATH_NULL;
                break;

              case ENOENT:
                pmath_message(PMATH_NULL, "nodir", 1, name1);
                name1 = PMATH_NULL;
                break;

              case EISDIR:
              case ENOTDIR:
                pmath_message(PMATH_NULL, "nodir", 1, name2);
                name2 = PMATH_NULL;
                break;

              default:
                pmath_message(PMATH_NULL, "ioarg", 1, expr);
                expr = PMATH_NULL;
                break;
            }

            pmath_unref(name2);
            name2 = pmath_ref(pmath_System_DollarFailed);
          }
        }
        else {
          switch(errno) {
            case EACCES:
            case EPERM:
              pmath_message(PMATH_NULL, "privv", 1, expr);
              expr = PMATH_NULL;
              break;

            default:
              if(is_directory) {
                pmath_message(PMATH_NULL, "nodir", 1, name1);
                name1 = PMATH_NULL;
              }
              else {
                pmath_message(PMATH_NULL, "fdir", 1, name1);
                name1 = PMATH_NULL;
              }
          }
          pmath_unref(name2);
          name2 = pmath_ref(pmath_System_DollarFailed);
        }
      }
    }

    pmath_mem_free(str1);
    pmath_mem_free(str2);
  }
#endif

  pmath_unref(expr);
  pmath_unref(name1);
  return name2;
}
