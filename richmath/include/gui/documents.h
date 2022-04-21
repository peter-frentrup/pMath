#ifndef RICHMATH__GUI__DOCUMENTS_H__INCLUDED
#define RICHMATH__GUI__DOCUMENTS_H__INCLUDED


#include <eval/observable.h>

#include <util/pmath-extra.h>


namespace richmath {
  class Document;
  class DocumentsImpl;
  
  class Documents {
      friend class DocumentsImpl;
      using Impl = DocumentsImpl;
    public:
      static bool init();
      static void done();
      
      static ObservableValue<FrontEndReference> selected_document_id;
      static Document *selected_document();
      static void selected_document(Document *document);
      
      static Expr make_section_boxes(Expr boxes, Document *doc);
      static bool locate_document_from_command(Expr item_cmd);
  };
  
}

#endif // RICHMATH__GUI__DOCUMENTS_H__INCLUDED
