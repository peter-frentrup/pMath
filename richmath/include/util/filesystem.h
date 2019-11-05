#ifndef RICHMATH__UTIL__FILESYSTEM_H__INCLUDED
#define RICHMATH__UTIL__FILESYSTEM_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  struct FileSystem {
    /// different from pmath_to_absolute_file_name(), this only succeeds for existing files, and not for directories
    static String to_absolute_file_name(String filename);

    static String extract_directory_path(String *filename);

    static String get_directory_path(String filename) { return extract_directory_path(&filename); }
  };
}

#endif // RICHMATH__UTIL__FILESYSTEM_H__INCLUDED
