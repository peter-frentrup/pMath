#ifndef RICHMATH__UTIL__FILESYSTEM_H__INCLUDED
#define RICHMATH__UTIL__FILESYSTEM_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  struct FileSystem {
    static String to_existing_absolute_file_name(String filename);
    static String to_possibly_nonexisting_absolute_file_name(String filename);
    
    static String file_name_join(String dir, String name);
    static bool is_filename_without_directory(String filename);

    static String extract_directory_path(String *filename);

    static String get_directory_path(String filename) { return extract_directory_path(&filename); }
  };
}

#endif // RICHMATH__UTIL__FILESYSTEM_H__INCLUDED
