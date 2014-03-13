#ifndef __UTIL__AUTOCOMPLETION_H__
#define __UTIL__AUTOCOMPLETION_H__

#include <boxes/box.h>
#include <graphics/context.h>


namespace richmath {
  class Document;
  
  class AutoCompletion {
    public:
      AutoCompletion(Document *_document);
      ~AutoCompletion();
      
      bool next(LogicalDirection direction);
      void stop();
    
    public:
      SelectionReference range;
    
    private:
      class Private;
      Private *priv;
  };
}

#endif // __UTIL__AUTOCOMPLETION_H__
