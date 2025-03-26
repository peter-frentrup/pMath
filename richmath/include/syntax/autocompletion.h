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
      
      bool is_active() const;
      bool has_popup() const;
      bool has_popup(FrontEndReference id) const;
      
      bool handle_key_backspace();
      bool handle_key_escape();
      bool handle_key_press(uint32_t unichar);
      bool handle_key_tab(LogicalDirection direction);
      bool handle_key_up_down(LogicalDirection direction);
      void stop();
    
    public:
      SelectionReference range;
    
    private:
      class Private;
      Private *priv;
    
    public:
      class AccessToken final {
        friend class AutoCompletion;
        
        explicit AccessToken() = default;
      };
  };
}

#endif // RICHMATH__SYNTAX__AUTOCOMPLETION_H__INCLUDED
