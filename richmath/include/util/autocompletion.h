#ifndef RICHMATH__UTIL__AUTOCOMPLETION_H__INCLUDED
#define RICHMATH__UTIL__AUTOCOMPLETION_H__INCLUDED

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

#endif // RICHMATH__UTIL__AUTOCOMPLETION_H__INCLUDED
