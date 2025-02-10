#ifndef RICHMATH__SYNTAX__AUTOCOMPLETION_H__INCLUDED
#define RICHMATH__SYNTAX__AUTOCOMPLETION_H__INCLUDED

#include <boxes/box.h>
#include <graphics/context.h>


namespace richmath {
  class Document;
  
  class AutoCompletion {
    public:
      explicit AutoCompletion(Document *_document);
      ~AutoCompletion();
      
      AutoCompletion(const AutoCompletion &) = delete;
      
      bool next(LogicalDirection direction);
      void stop();
    
    public:
      SelectionReference range;
    
    private:
      class Private;
      Private *priv;
  };
}

#endif // RICHMATH__SYNTAX__AUTOCOMPLETION_H__INCLUDED
