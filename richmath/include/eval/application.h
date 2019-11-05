#ifndef RICHMATH__EVAL__APPLICATION_H__INCLUDED
#define RICHMATH__EVAL__APPLICATION_H__INCLUDED

#include <util/frontendobject.h>
#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  enum class ClientNotification {
    End,
    PrintSection,
    Return,
    ReturnBox,
    StartSession,
    EndSession,
    
    CallFrontEnd,
    MenuCommand,
    DynamicUpdate,
    CurrentValue,
    SetCurrentValue,
    GetEvaluationDocument,
    DocumentGet,
    DocumentRead,
    FileDialog,
    Save,
  };
  
  class MenuCommandStatus {
    public:
      MenuCommandStatus(bool _enabled)
        : enabled(_enabled),
          checked(false)
      {
      }
      
    public:
      bool enabled;
      bool checked;
  };
  
  enum class MenuCommandScope {
    Selection,
    Document
  };
  
  class FrontEndObject;
  class Box;
  class Document;
  class Job;
  
  class Application: public Base {
    public:
      static bool is_running_on_gui_thread();
      
      static void notify(     ClientNotification type, Expr data); // callable from non-GUI thread
      static Expr notify_wait(ClientNotification type, Expr data); // callable from non-GUI thread
      
      static Expr current_value(Expr item);
      static Expr current_value(FrontEndObject *obj, Expr item);
      static bool set_current_value(FrontEndObject *obj, Expr item, Expr rhs);
      
      static void run_menucommand(Expr cmd) { // callable from non-GUI thread
        notify(ClientNotification::MenuCommand, cmd);
      }
      
      // bad design:
      static bool run_recursive_menucommand(Expr cmd);
      
      static MenuCommandStatus test_menucommand_status(Expr cmd);
      static Expr generate_dynamic_submenu(Expr cmd);
      static bool remove_dynamic_submenu_item(Expr submenu_cmd, Expr item_cmd);
      
      static void register_menucommand(
        Expr cmd,
        bool              (*func)(Expr cmd),
        MenuCommandStatus (*test)(Expr cmd) = nullptr);
      
      static void register_dynamic_submenu(Expr cmd, Expr (*func)(Expr cmd));
      static void register_submenu_item_deleter(Expr submenu_cmd, bool (*func)(Expr submenu_cmd, Expr item_cmd));
      
      static bool register_currentvalue_provider(
        Expr   item,
        Expr (*get)(FrontEndObject *obj, Expr item),
        bool (*set)(FrontEndObject *obj, Expr item, Expr rhs) = nullptr);
        
      static void gui_print_section(Expr expr);
      static Expr save(Document *doc);
      //static void update_control_active(bool value);
      static void activated_control(Box *box);
      static void deactivated_control(Box *box);
      static void deactivated_all_controls();
      
      static void init();
      static void doevents();
      static int run();
      static void done();
      
      static void add_job(SharedPtr<Job> job);
      static Box *find_current_job();
      static bool remove_job(Box *input_box, bool only_check_possibility);
      static void abort_all_jobs();
      
      static Box      *get_evaluation_box();
      static Expr      run_filedialog(Expr data);
      
      /* These may return nullptr (no gui available ...)
         The document will not be visible, call invslidate_options() to
         recognize the "Visible" style option.
      */
      static Document *create_document();
      static Document *create_document(Expr data);
      
      // you should have normalized the filename with FileSystem::to_absolute_file_name()
      static Document *find_open_document(String filename);
      static Document *open_new_document(String filename);
      
      static bool is_idle();
      static bool is_running_job_for(Box *box);
      
      static void async_interrupt(Expr expr);
      
      static Expr interrupt_wait(Expr expr, double seconds);
      static Expr interrupt_wait(Expr expr);
      
      static Expr interrupt_wait_cached(Expr expr, double seconds);
      static Expr interrupt_wait_cached(Expr expr);
      
      static Expr interrupt_wait_for(Expr expr, Box *box, double seconds);
      static Expr interrupt_wait_for(Expr expr, Box *box);
      
      // callable from non-gui thread:
      static Expr internal_execute_for(
                    Expr              expr, 
                    FrontEndReference doc, 
                    FrontEndReference sect, 
                    FrontEndReference box);
      
      static void delay_dynamic_updates(bool delay);
      
    public:
      static double edit_interrupt_timeout;
      static double interrupt_timeout;
      static double button_timeout;
      static double dynamic_timeout;
      static double min_dynamic_update_interval;
      static String application_filename;
      static String application_directory; // without trailing (back)slash
      static String stylesheet_path_base; // includes trailing (back)slash
      static Expr   palette_search_path;
      static MenuCommandScope menu_command_scope;
      
      static Hashtable<Expr, Expr> eval_cache;
      
    private:
      Application()
        : Base()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
      }
  };
  
  struct AutoGuiWait {
    AutoGuiWait();
    ~AutoGuiWait();
    
    AutoGuiWait(const AutoGuiWait &) = delete;
    AutoGuiWait &operator=(const AutoGuiWait &) = delete;
  };
}

#endif // RICHMATH__EVAL__APPLICATION_H__INCLUDED
