#ifndef RICHMATH__EVAL__APPLICATION_H__INCLUDED
#define RICHMATH__EVAL__APPLICATION_H__INCLUDED

#include <util/frontendobject.h>
#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>
#include <boxes/section.h> // for SectionKind


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
    DocumentRead,
    FileDialog,
    Save,
    AskInterrupt,
  };
  
  class FrontEndObject;
  class FrontEndSession;
  class Box;
  class Document;
  class Job;
  
  class Application: public Base {
    public:
      static bool is_running_on_gui_thread();
      
      static void notify(     ClientNotification type, Expr data); // callable from non-GUI thread
      static Expr notify_wait(ClientNotification type, Expr data); // callable from non-GUI thread
      
      static void click_button_async(FrontEndReference button_id); // callable from non-GUI thread
      
      static Section *try_make_output(SectionKind kind);
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
      static FrontEndObject *find_current_job();
      static bool remove_job(Box *input_box, bool only_check_possibility);
      static void abort_all_jobs();
      
      static FrontEndObject *get_evaluation_object();
      static Expr            run_filedialog(Expr data);
      
      /* These may return nullptr (no gui available ...)
         The document will not be visible, call its on_style_changed(true) to
         recognize the "Visible" style option.
      */
      static Document *try_create_document();
      static Document *try_create_document(Expr data);
      
      // you should have normalized the filename with FileSystem::to_absolute_file_name()
      static Document *find_open_document(String filename);
      static Document *open_new_document(String filename);
      
      static bool is_idle();
      static bool is_running_job_for(Box *section_or_document);
      
      static void async_interrupt(Expr expr);
      
      static void with_evaluation_box(FrontEndObject *obj, void(*callback)(void*), void *arg);
      
      template<typename Func>
      static void with_evaluation_box(FrontEndObject *obj, Func func) {
        with_evaluation_box(obj, [](void *_func) { (*(Func*)_func)(); }, &func);
      }
      
      static Expr interrupt_wait(Expr expr, double seconds);
      static Expr interrupt_wait(Expr expr);
      
      static Expr interrupt_wait_cached(Expr expr, double seconds);
      static Expr interrupt_wait_cached(Expr expr);
      
      static Expr interrupt_wait_for(Expr expr, FrontEndObject *obj, double seconds);
      static Expr interrupt_wait_for_interactive(Expr expr, FrontEndObject *obj, double seconds);
            
      static void delay_dynamic_updates(bool delay);
      
    public:
      static FrontEndSession *front_end_session;
      
      static double edit_interrupt_timeout;
      static double interrupt_timeout;
      static double button_timeout;
      static double dynamic_timeout;
      static double min_dynamic_update_interval;
      static String application_filename;
      static String application_directory; // without trailing (back)slash
      static String stylesheet_path_base; // includes trailing (back)slash
      static Expr   palette_search_path;
      static Expr   session_id;
      static pmath_atomic_uint8_t track_dynamic_update_causes;
      
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
