#include <gui/fltk/mfltk-recent-documents.h>
#include <gui/recent-documents.h>

#include <util/base.h>
#include <util/array.h>
#include <util/filesystem.h>

#include <FL/Fl.H>
#include <FL/Fl_Preferences.H>
#include <FL/filename.H>

namespace richmath {
  class MathFltkRecentDocuments::Impl: public Base {
    public:
      Impl();
      ~Impl();
      
      void load();
      void save();
      
      void add(String path);
      bool remove(String path);
      Expr as_menu_list();
      
    public:
      static Impl *impl;
    
    private:
      Array<String> files;
  };
}

using namespace richmath;

#define MAX_RECENT_ENTRIES  20
#define MAX_FILENAME_SIZE   2048

//{ class MathFltkRecentDocuments ...

void MathFltkRecentDocuments::add(String path) {
  Impl::impl->add(path);
}

Expr MathFltkRecentDocuments::as_menu_list() {
  return Impl::impl->as_menu_list();
}

bool MathFltkRecentDocuments::remove(String path) {
  return Impl::impl->remove(path);
}

void MathFltkRecentDocuments::init() {
  Impl::impl = new Impl();
  Impl::impl->load();
}

void MathFltkRecentDocuments::done() {
  Impl::impl->save();
  delete Impl::impl;
  Impl::impl = nullptr;
}

//} ... class MathFltkRecentDocuments

//{ class MathFltkRecentDocuments::Impl ...

MathFltkRecentDocuments::Impl *MathFltkRecentDocuments::Impl::impl = nullptr;

MathFltkRecentDocuments::Impl::Impl() {
}

MathFltkRecentDocuments::Impl::~Impl() {
}

void MathFltkRecentDocuments::Impl::load() {
  Fl_Preferences prefs(Fl_Preferences::USER_L, "Peter_Frentrup", "pMath.RichMath");
  Fl_Preferences recent_group(&prefs, "RecentFiles");
  
  files.length(0);
  for(int i = 0; i < MAX_RECENT_ENTRIES; ++i) {
    char key[] = "File_A";
    key[sizeof(key) - 2] += i; // File_A, File_B, ...
    
    char value[MAX_FILENAME_SIZE];
    recent_group.get(key, value, "", sizeof(value));
    
    if(*value) {
      files.add(String::FromUtf8(value));
    }
  }
}

void MathFltkRecentDocuments::Impl::save() {
  Fl_Preferences prefs(Fl_Preferences::USER_L, "Peter_Frentrup", "pMath.RichMath");
  Fl_Preferences recent_group(&prefs, "RecentFiles");
  
  for(int i = 0; i < MAX_RECENT_ENTRIES; ++i) {
    char key[] = "File_A";
    key[sizeof(key) - 2] += i; // File_A, File_B, ...
    
    if(i < files.length()) {
      int len = 0;
      if(char *value = pmath_string_to_utf8(files[i].get(), &len)) {
        recent_group.set(key, value);
        
        pmath_mem_free(value);
      }
      else {
        recent_group.set(key, "");
      }
    }
    else {
      recent_group.set(key, "");
    }
  }
  
  recent_group.flush();
}

void MathFltkRecentDocuments::Impl::add(String path) {
  int end = 0;
  while(end < files.length() && files[end] != path)
    ++end;
  
  if(end >= MAX_RECENT_ENTRIES)
    end = MAX_RECENT_ENTRIES - 1;
  
  if(end >= files.length())
    files.add({});
  
  for(int k = end; k > 0 ;--k)
    files[k] = files[k - 1];
  
  files[0] = path;
}

bool MathFltkRecentDocuments::Impl::remove(String path) {
  for(int i = 0; i < files.length(); ++i) {
    if(files[i] == path) {
      files.remove(i, 1);
      return true;
    }
  }
  return false;
}

Expr MathFltkRecentDocuments::Impl::as_menu_list() {
  Gather g;
  for(int i = 0; i < 9 && i < files.length(); ++i) {
    String path = files[i];
    
    String fname = path;
    FileSystem::extract_directory_path(&fname);
    
    Gather::emit(RecentDocuments::open_document_menu_item(Expr(i + 1).to_string() + " " + fname, path));
  }
  return g.end();
}

//} ... class MathFltkRecentDocuments::Impl
