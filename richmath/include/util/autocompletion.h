#ifndef __UTIL__AUTOCOMPLETION_H__
#define __UTIL__AUTOCOMPLETION_H__

#include <boxes/box.h>
#include <graphics/context.h>


namespace richmath {
  class Document;
  
  class AutoCompletion {
    public:
      AutoCompletion(Document *_document);
      
      bool next(LogicalDirection direction);
      void stop();
    
    protected:
      bool continue_completion(LogicalDirection direction);
      
      bool start_alias(LogicalDirection direction);
      bool start_filename(LogicalDirection direction);
      bool start_symbol(LogicalDirection direction);
    
    protected:
      Document *document;
      Expr current_boxes_list;
      int current_index;
    
    public:
      SelectionReference range;
  };
}

#endif // __UTIL__AUTOCOMPLETION_H__
