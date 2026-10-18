#include <gui/fltk/mfltk-filedialog.h>

#include <util/array.h>

#include <FL/Fl.H>
#include <FL/Fl_ask.H>
#include <FL/Fl_Native_File_Chooser.H>


using namespace richmath;

namespace {
  class FileDialogFilter {
    public:
      Array<char> filter_str;
    
    public:
      void add_all(Expr filter);
    
    private:
      void add_extension(ArrayView<char> ext);
      void add_filter(String caption, Expr extensions);
      
    private:
      static void mask_control_chars(ArrayView<char> buf);
  };
}

extern pmath_symbol_t richmath_System_DollarCanceled;
extern pmath_symbol_t richmath_System_List;

//{ class MathFltkFileDialog ...

MathFltkFileDialog::MathFltkFileDialog(bool to_save)
: _to_save{to_save}
{
}

MathFltkFileDialog::~MathFltkFileDialog() {
}

Expr MathFltkFileDialog::show_dialog() {
  Fl_Native_File_Chooser native;
  
  int title_utf8_len;
  char *title_utf8 = pmath_string_to_utf8(_title.get(), &title_utf8_len);
  
  int initialfile_utf8_len = 0;
  char *initialfile_utf8 = nullptr;
  if(_initialfile)
    initialfile_utf8 = pmath_string_to_utf8(_initialfile.get(), &initialfile_utf8_len);
  
  FileDialogFilter filter;
  filter.add_all(_filter);
  
  native.type(_to_save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);
  
  if(_to_save)
    native.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::NEW_FOLDER | Fl_Native_File_Chooser::USE_FILTER_EXT );
  
  if(title_utf8)
    native.title(title_utf8);
  
  if(initialfile_utf8)
    native.preset_file(initialfile_utf8);
  
  if(filter.filter_str.length()) {
    filter.filter_str.add('\0');
    pmath_debug_print("[filter: '%s']\n", filter.filter_str.items());
    native.filter(filter.filter_str.items());
  }
  else {
    pmath_debug_print("[no filter]\n");
  }
  
  pmath_mem_free(initialfile_utf8);
  pmath_mem_free(title_utf8);
  
  int answer = native.show();
  if(answer < 0) {
    pmath_debug_print("[Fl_Native_File_Chooser failed: %s]", native.errmsg());
    return Symbol(richmath_System_DollarCanceled);
  }
  
  if(answer == 0) {
    if(native.filename()) {
      return String::FromUtf8(native.filename());
    }
  }
  
  // 1 = cancelled
  return Symbol(richmath_System_DollarCanceled);
}

//} ... class MathFltkFileDialog


//{ class FileDialogFilter ...

void FileDialogFilter::add_all(Expr filter) {
  if(filter.expr_length() == 0 || !filter.item_equals(0, richmath_System_List))
    return;
  
  for(Expr rule : filter.items()) {
    if(rule.is_rule()) {
      if(String(rule[2]).equals("*.*")) {
        // FLTK adds "All files (*.*)" itself
        continue; 
      }
      add_filter(String(rule[1]), rule[2]);
    }
  }
}

void FileDialogFilter::add_filter(String caption, Expr extensions) {
  if(caption.length() == 0)
    return;
  
  int caption_utf8_len;
  char *caption_utf8 = pmath_string_to_utf8(caption.get(), &caption_utf8_len);
  if(!caption_utf8)
    return;
  
  ArrayView<char> caption_buf = ArrayView<char>(caption_utf8_len, caption_utf8);
  mask_control_chars(caption_buf);
  
  if(extensions.is_string()) {
    int ext_utf8_len;
    if(char *ext_utf8 = pmath_string_to_utf8(extensions.get(), &ext_utf8_len)) {
      ArrayView<char> ext_buf = ArrayView<char>(ext_utf8_len, ext_utf8);
  
      if(filter_str.length())
        filter_str.add('\n');
      
      // Strip suffix " (*.ext)" from caption
      if(caption_buf.length() > 2 + ext_buf.length() 
      && caption_buf.take_last(1) == string_view(")") 
      && caption_buf.drop_last(1).take_last(ext_buf.length()) == ext_buf
      && caption_buf.drop_last(1 + ext_buf.length()).take_last(1) == string_view("(")) {
        caption_buf = caption_buf.drop_last(2 + ext_buf.length());
        if(caption_buf.length() > 1 && caption_buf.take_last(1) == string_view(" "))
          caption_buf = caption_buf.drop_last(1);
      }
      
      filter_str.add_all(caption_buf);
      filter_str.add_all(string_view("\t"));
      add_extension(ext_buf);
      
      pmath_mem_free(ext_utf8);
    }
  }
  else if(extensions.expr_length() >= 1 && extensions.item_equals(0, richmath_System_List)) {
    bool all_strings = true;
    for(Expr ext : extensions.items()) {
      if(!ext.is_string()) {
        all_strings = false;
        break;
      }
    }
    
    if(all_strings) {
      if(filter_str.length())
        filter_str.add('\n');
      
      filter_str.add_all(caption_buf);
      filter_str.add_all(string_view("\t{"));
      
      bool first = true;
      for(String ext : extensions.items()) {
        int ext_utf8_len;
        if(char *ext_utf8 = pmath_string_to_utf8(ext.get(), &ext_utf8_len)) {
          ArrayView<char> ext_buf = ArrayView<char>(ext_utf8_len, ext_utf8);
          
          if(first) 
            first = false;
          else
            filter_str.add(',');
          
          add_extension(ext_buf);
          
          pmath_mem_free(ext_utf8);
        }
      }
      
      filter_str.add('}');
    }
  }
  
  pmath_mem_free(caption_utf8);
}

void FileDialogFilter::add_extension(ArrayView<char> ext) {
//  int len = filter_str.length();
//  filter_str.length(len + ext.length());
//  filter_str.length(len);
  
  for(char ch : ext) {
    switch(ch) {
      case '{': case '}': case '[': case ']': case ',': case '|': case '\\': 
        filter_str.add('\\');
        filter_str.add(ch);
        break;
      
      default:
        if((unsigned char)ch < ' ')
          filter_str.add('?');
        else 
          filter_str.add(ch);
        break;
    }
  }
}

void FileDialogFilter::mask_control_chars(ArrayView<char> buf) {
  for(int i = 0; i < buf.length(); ++i) {
    if((unsigned char)buf[i] < ' ')
      buf[i] = ' ';
  }
}

//} ... class FileDialogFilter
