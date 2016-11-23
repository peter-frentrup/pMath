#ifndef __EVAL__CLIENT_H__
#define __EVAL__CLIENT_H__

#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>


namespace richmath {
  typedef enum {
    CNT_END,
    CNT_PRINTSECTION,
    CNT_RETURN,
    CNT_RETURNBOX,
    CNT_STARTSESSION,
    CNT_ENDSESSION,
    
    CNT_GETDOCUMENTS,
    CNT_MENUCOMMAND,
    CNT_ADDCONFIGSHAPER,
    CNT_GETOPTIONS,
    CNT_SETOPTIONS,
    CNT_DYNAMICUPDATE,
    CNT_CREATEDOCUMENT,
    CNT_CURRENTVALUE,
    CNT_GETEVALUATIONDOCUMENT,
    CNT_DOCUMENTGET,
    CNT_DOCUMENTREAD,
    CNT_COLORDIALOG,
    CNT_FONTDIALOG,
    CNT_FILEDIALOG,
    CNT_SAVE
  } ClientNotificationType;
  
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
  
  class Box;
  class Document;
  class Job;
  
  class Application: public Base {
    public:
      static void notify(     ClientNotificationType type, Expr data); // callable from non-GUI thread
      static Expr notify_wait(ClientNotificationType type, Expr data); // callable from non-GUI thread
      
      static void run_menucommand(Expr cmd) { // callable from non-GUI thread
        notify(CNT_MENUCOMMAND, cmd);
      }
      
      // bad design:
      static bool run_recursive_menucommand(Expr cmd);
      
      static MenuCommandStatus test_menucommand_status(Expr cmd);
      
      static void register_menucommand(
        Expr cmd,
        bool              (*func)(Expr cmd),
        MenuCommandStatus (*test)(Expr cmd) = 0);
        
      static void gui_print_section(Expr expr);
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
      
      static bool is_idle();
      static bool is_running_job_for(Box *box);
      
      static Expr interrupt(Expr expr, double seconds);
      static Expr interrupt(Expr expr);
      
      static Expr interrupt_cached(Expr expr, double seconds);
      static Expr interrupt_cached(Expr expr);
      
      static void execute_for(Expr expr, Box *box, double seconds);
      static void execute_for(Expr expr, Box *box);
      
      // callable from non-gui thread:
      static Expr internal_execute_for(Expr expr, int doc, int sect, int box);
      
      static void delay_dynamic_updates(bool delay);
      
    public:
      static double edit_interrupt_timeout;
      static double interrupt_timeout;
      static double button_timeout;
      static double dynamic_timeout;
      static double min_dynamic_update_interval;
      static String application_filename;
      static String application_directory; // without trailing (back)slash
      
      static Hashtable<Expr, Expr, object_hash> eval_cache;
      
    private:
      Application() {}
  };
}

#endif // __EVAL__CLIENT_H__
