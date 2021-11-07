#include <eval/binding.h>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/textsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/clipboard.h>
#include <gui/common-document-windows.h>
#include <gui/documents.h>
#include <gui/document.h>
#include <gui/menus.h>
#include <gui/messagebox.h>
#include <gui/native-widget.h>
#include <gui/recent-documents.h>

#include <syntax/spanexpr.h>

#include <util/selection-tracking.h>


using namespace richmath;

//{ pmath functions ...

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_Dialog;
extern pmath_symbol_t richmath_System_DocumentApply;
extern pmath_symbol_t richmath_System_DocumentSave;
extern pmath_symbol_t richmath_System_False;
extern pmath_symbol_t richmath_System_FrontEndTokenExecute;
extern pmath_symbol_t richmath_System_Interrupt;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_MakeBoxes;
extern pmath_symbol_t richmath_System_Return;
extern pmath_symbol_t richmath_System_Row;
extern pmath_symbol_t richmath_System_Rule;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_System_SectionGenerated;
extern pmath_symbol_t richmath_System_SectionPrint;
extern pmath_symbol_t richmath_System_Stack;
extern pmath_symbol_t richmath_System_StyleDefinitions;
extern pmath_symbol_t richmath_System_True;

extern pmath_symbol_t richmath_FE_FileOpenDialog;
extern pmath_symbol_t richmath_FrontEnd_KernelExecute;

extern Expr richmath_eval_FrontEnd_KernelExecute(Expr expr);

static pmath_t builtin_callfrontend(pmath_expr_t expr);
static pmath_t builtin_filedialog(pmath_expr_t _expr);
static pmath_t builtin_documentapply_or_documentwrite(pmath_expr_t _expr);
static pmath_t builtin_documentread(pmath_expr_t _expr);
static pmath_t builtin_documentsave(pmath_expr_t _expr);
static pmath_t builtin_internal_dynamicupdated(pmath_expr_t expr);
static pmath_t builtin_frontendtokenexecute(pmath_expr_t expr);
static pmath_t builtin_sectionprint(pmath_expr_t expr);
static pmath_t builtin_interrupt(pmath_expr_t expr);
//} ... pmath functions

//{ menu command availability checkers ...
static MenuCommandStatus can_save(Expr cmd);
static MenuCommandStatus can_abort_or_interrupt(Expr cmd);
static MenuCommandStatus can_convert_dynamic_to_literal(Expr cmd);
static MenuCommandStatus can_copy_cut(Expr cmd);
static MenuCommandStatus can_open_close_group(Expr cmd);
static MenuCommandStatus can_do_scoped(Expr cmd);
static MenuCommandStatus can_document_write(Expr cmd);
static MenuCommandStatus can_duplicate_previous_input_output(Expr cmd);
static MenuCommandStatus can_edit_boxes(Expr cmd);
static MenuCommandStatus can_expand_selection(Expr cmd);
static MenuCommandStatus can_evaluate_in_place(Expr cmd);
static MenuCommandStatus can_evaluate_sections(Expr cmd);
static MenuCommandStatus can_find_evaluating_section(Expr cmd);
static MenuCommandStatus can_find_matching_fence(Expr cmd);
static MenuCommandStatus can_graphics_original_size(Expr cmd);
static MenuCommandStatus can_insert_inline_section_cmd(Expr cmd);
static MenuCommandStatus can_insert_opposite_cmd(Expr cmd);
static MenuCommandStatus can_remove_from_evaluation_queue(Expr cmd);
static MenuCommandStatus can_section_merge(Expr cmd);
static MenuCommandStatus can_section_split(Expr cmd);
static MenuCommandStatus can_set_style(Expr cmd);
static MenuCommandStatus can_similar_section_below(Expr cmd);
static MenuCommandStatus can_subsession_evaluate_sections(Expr cmd);
static MenuCommandStatus can_toggle_character_code(Expr cmd);

static bool has_style(ActiveStyledObject *box, StyleOptionName name, Expr rhs);
//} ... menu command availability checkers

//{ menu commands ...
static bool abort_cmd(Expr cmd);
static bool close_cmd(Expr cmd);
static bool convert_dynamic_to_literal(Expr cmd);
static bool copy_cmd(Expr cmd);
static bool copy_special_cmd(Expr cmd);
static bool cut_cmd(Expr cmd);
static bool do_kernelexecute_cmd(Expr cmd);
static bool do_scoped_cmd(Expr cmd);
static bool document_apply_cmd(Expr cmd);
static bool document_write_cmd(Expr cmd);
static bool duplicate_previous_input_output_cmd(Expr cmd);
static bool edit_boxes_cmd(Expr cmd);
static bool evaluate_in_place_cmd(Expr cmd);
static bool evaluate_sections_cmd(Expr cmd);
static bool evaluator_subsession_cmd(Expr cmd);
static bool expand_selection_cmd(Expr cmd);
static bool find_evaluating_section(Expr cmd);
static bool find_matching_fence_cmd(Expr cmd);
static bool graphics_original_size_cmd(Expr cmd);
static bool insert_column_cmd(Expr cmd);
static bool insert_fraction_cmd(Expr cmd);
static bool insert_inline_section_cmd(Expr cmd);
static bool insert_opposite_cmd(Expr cmd);
static bool insert_overscript_cmd(Expr cmd);
static bool insert_radical_cmd(Expr cmd);
static bool insert_row_cmd(Expr cmd);
static bool insert_subscript_cmd(Expr cmd);
static bool insert_superscript_cmd(Expr cmd);
static bool insert_underscript_cmd(Expr cmd);
static bool interrupt_cmd(Expr cmd);
static bool new_cmd(Expr cmd);
static bool open_cmd(Expr cmd);
static bool open_close_group_cmd(Expr cmd);
static bool paste_cmd(Expr cmd);
static bool remove_from_evaluation_queue(Expr cmd);
static bool save_cmd(Expr cmd);
static bool saveas_cmd(Expr cmd);
static bool section_merge_cmd(Expr cmd);
static bool section_split_cmd(Expr cmd);
static bool select_all_cmd(Expr cmd);
static bool set_style_cmd(Expr cmd);
static bool similar_section_below_cmd(Expr cmd);
static bool subsession_evaluate_sections_cmd(Expr cmd);
static bool toggle_character_code(Expr cmd);
//} ... menu commands

//{ strings.inc ...

namespace richmath {
  namespace strings {
#   define RICHMATH_DECLARE_STRING(SYM, CONTENT)        extern String SYM;
#     include "strings.inc"
#   undef RICHMATH_DECLARE_STRING
  }
}

#   define RICHMATH_DECLARE_STRING(SYM, CONTENT)        String richmath::strings::SYM;
#     include "strings.inc"
#   undef RICHMATH_DECLARE_STRING

static bool init_strings() {
#  define RICHMATH_DECLARE_STRING(SYM, CONTENT)  if(!(richmath::strings::SYM = String(CONTENT))) goto FAIL;
#    include "strings.inc"
#  undef RICHMATH_DECLARE_STRING
  
  return true;
FAIL:
  return false;
}

static void done_strings() {
#  define RICHMATH_DECLARE_STRING(SYM, CONTENT)  richmath::strings::SYM = String();
#    include "strings.inc"
#  undef RICHMATH_DECLARE_STRING
}
 
//} ... strings.inc

//{ symbols.inc ...
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           PMATH_PRIVATE pmath_symbol_t SYM = PMATH_STATIC_NULL;
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  
#define RICHMATH_IMPL_GUI(SYM, NAME, CPPFUNC) \
    extern Expr CPPFUNC(Expr expr); \
    static pmath_t raw_ ## CPPFUNC(pmath_expr_t expr) { \
      if(!Application::is_running_on_gui_thread()) \
        return expr; \
      return CPPFUNC( Expr{expr} ).release(); \
    }
    
#  include "symbols.inc"
#undef RICHMATH_IMPL_GUI
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL

static bool init_symbols() {
#define VERIFY(X)  do{ pmath_t tmp = (X); if(pmath_is_null(tmp)) goto FAIL; }while(0);
#define NEW_SYMBOL(name)     pmath_symbol_get(PMATH_C_STRING(name), TRUE)

#define BIND(SYMBOL, FUNC, USE)  if(!pmath_register_code((SYMBOL), (FUNC), (USE))) goto FAIL;
#define BIND_DOWN(SYMBOL, FUNC)   BIND((SYMBOL), (FUNC), PMATH_CODE_USAGE_DOWNCALL)
#define BIND_UP(SYMBOL, FUNC)     BIND((SYMBOL), (FUNC), PMATH_CODE_USAGE_UPCALL)
    
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           VERIFY( SYM = NEW_SYMBOL(NAME) )
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  pmath_symbol_set_attributes( (SYM), (ATTR) );
#define RICHMATH_IMPL_GUI(SYM, NAME, CPPFUNC)        BIND_DOWN(SYM, raw_ ## CPPFUNC)
#  include "symbols.inc"
#undef RICHMATH_IMPL_GUI
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL

  BIND_DOWN(richmath_Internal_DynamicUpdated,  builtin_internal_dynamicupdated)
  
  BIND_DOWN(richmath_System_DocumentApply,         builtin_documentapply_or_documentwrite)
  BIND_DOWN(richmath_System_DocumentRead,          builtin_documentread)
  BIND_DOWN(richmath_System_DocumentWrite,         builtin_documentapply_or_documentwrite)
  BIND_DOWN(richmath_System_DocumentSave,          builtin_documentsave)
  BIND_DOWN(richmath_System_FrontEndTokenExecute,  builtin_frontendtokenexecute)
  BIND_DOWN(richmath_System_Interrupt,             builtin_interrupt)
  BIND_DOWN(richmath_System_SectionPrint,          builtin_sectionprint)
  
  BIND_DOWN(richmath_FE_CallFrontEnd,        builtin_callfrontend)
  BIND_DOWN(richmath_FE_FileOpenDialog,      builtin_filedialog)
  BIND_DOWN(richmath_FE_FileSaveDialog,      builtin_filedialog)
  
  return true;
FAIL:
  return false;
}

static void done_symbols() {
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           pmath_unref( SYM ); SYM = PMATH_NULL;
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL
}
//} ... symbols.inc

bool richmath::init_bindings() {
  if(!init_strings()) goto FAIL_STRINGS;
  if(!init_symbols()) goto FAIL_SYMBOLS;
  
  Menus::init();
  
  Menus::register_command(Symbol(richmath_System_DollarFailed), [](Expr cmd) { return false;},       [](Expr cmd) { return MenuCommandStatus(false); });
  Menus::register_command(String("New"),                        new_cmd);
  Menus::register_command(String("Open"),                       open_cmd);
  Menus::register_command(String("Save"),                       save_cmd,                            can_save);
  Menus::register_command(String("SaveAs"),                     saveas_cmd);
  Menus::register_command(String("Close"),                      close_cmd);
  
  Menus::register_command(strings::Copy,                        copy_cmd,                            can_copy_cut);
  Menus::register_command(strings::Cut,                         cut_cmd,                             can_copy_cut);
  Menus::register_command(String("OpenCloseGroup"),             open_close_group_cmd,                can_open_close_group);
  Menus::register_command(String("Paste"),                      paste_cmd,                           can_document_write);
  Menus::register_command(String("GraphicsOriginalSize"),       graphics_original_size_cmd,          can_graphics_original_size);
  Menus::register_command(String("EditBoxes"),                  edit_boxes_cmd,                      can_edit_boxes);
  Menus::register_command(String("ExpandSelection"),            expand_selection_cmd,                can_expand_selection);
  Menus::register_command(String("FindMatchingFence"),          find_matching_fence_cmd,             can_find_matching_fence);
  Menus::register_command(String("SelectAll"),                  select_all_cmd);
  
  Menus::register_command(String("SectionMerge"),               section_merge_cmd,                   can_section_merge);
  Menus::register_command(String("SectionSplit"),               section_split_cmd,                   can_section_split);
  
  Menus::register_command(strings::DuplicatePreviousInput,      duplicate_previous_input_output_cmd, can_duplicate_previous_input_output);
  Menus::register_command(String("DuplicatePreviousOutput"),    duplicate_previous_input_output_cmd, can_duplicate_previous_input_output);
  Menus::register_command(String("SimilarSectionBelow"),        similar_section_below_cmd,           can_similar_section_below);
  Menus::register_command(String("InsertColumn"),               insert_column_cmd,                   can_document_write);
  Menus::register_command(String("InsertFraction"),             insert_fraction_cmd,                 can_document_write);
  Menus::register_command(String("InsertInlineSection"),        insert_inline_section_cmd,           can_insert_inline_section_cmd);
  Menus::register_command(String("InsertOpposite"),             insert_opposite_cmd,                 can_insert_opposite_cmd);
  Menus::register_command(String("InsertOverscript"),           insert_overscript_cmd,               can_document_write);
  Menus::register_command(String("InsertRadical"),              insert_radical_cmd,                  can_document_write);
  Menus::register_command(String("InsertRow"),                  insert_row_cmd,                      can_document_write);
  Menus::register_command(String("InsertSubscript"),            insert_subscript_cmd,                can_document_write);
  Menus::register_command(String("InsertSuperscript"),          insert_superscript_cmd,              can_document_write);
  Menus::register_command(String("InsertUnderscript"),          insert_underscript_cmd,              can_document_write);
  
  
  Menus::register_command(String("DynamicToLiteral"),           convert_dynamic_to_literal,          can_convert_dynamic_to_literal);
  Menus::register_command(String("EvaluatorAbort"),             abort_cmd,                           can_abort_or_interrupt);
  Menus::register_command(String("EvaluatorInterrupt"),         interrupt_cmd,                       can_abort_or_interrupt);
  Menus::register_command(String("EvaluateInPlace"),            evaluate_in_place_cmd,               can_evaluate_in_place);
  Menus::register_command(String("EvaluateSections"),           evaluate_sections_cmd,               can_evaluate_sections);
  Menus::register_command(strings::EvaluateSectionsAndReturn,   evaluate_sections_cmd);
  Menus::register_command(String("EvaluatorSubsession"),        evaluator_subsession_cmd,            can_abort_or_interrupt);
  Menus::register_command(String("FindEvaluatingSection"),      find_evaluating_section,             can_find_evaluating_section);
  Menus::register_command(String("RemoveFromEvaluationQueue"),  remove_from_evaluation_queue,        can_remove_from_evaluation_queue);
  Menus::register_command(String("SubsessionEvaluateSections"), subsession_evaluate_sections_cmd,    can_subsession_evaluate_sections);
  Menus::register_command(String("ToggleCharacterCode"),        toggle_character_code);
  
  Menus::register_command(Symbol(richmath_System_DocumentApply),  document_apply_cmd,  can_document_write);
  Menus::register_command(Symbol(richmath_System_DocumentWrite),  document_write_cmd,  can_document_write);
  
  Menus::register_command(Symbol(richmath_FE_CopySpecial),   copy_special_cmd, can_copy_cut);
  Menus::register_command(Symbol(richmath_System_Rule),      set_style_cmd,    can_set_style);
  Menus::register_command(Symbol(richmath_FE_ScopedCommand), do_scoped_cmd,    can_do_scoped);
  
  Menus::register_command(Symbol(richmath_FrontEnd_KernelExecute), do_kernelexecute_cmd);
  
  if(!Documents::init())
    goto FAIL_DOCUMENTS;
  
  return true;
  
FAIL_DOCUMENTS:
FAIL_SYMBOLS:
  done_symbols();
FAIL_STRINGS:
  done_strings();
  return false;
}

void richmath::done_bindings() {
  Documents::done();
  Menus::done();
  done_symbols();
  done_strings();
}

//{ pmath functions ...

static pmath_t builtin_callfrontend(pmath_expr_t expr) {
  /* FE`CallFrontEnd(expr)  ===  FE`CallFrontEnd(expr, True)
  */
  size_t exprlen = pmath_expr_length(expr);
  pmath_t item;
  bool waiting;

  if(exprlen < 1 || exprlen > 2) {
    pmath_message_argxxx(exprlen, 1, 2);
    return expr;
  }
  
  if(exprlen == 2) {
    item = pmath_expr_get_item(expr, 2);
    if(pmath_same(item, richmath_System_True)) {
      pmath_unref(item);
      waiting = true;
    }
    else if(pmath_same(item, richmath_System_False)) {
      pmath_unref(item);
      waiting = false;
    }
    else {
      pmath_unref(item);
      pmath_message(PMATH_NULL, "bool", 2, pmath_ref(expr), PMATH_FROM_INT32(2));
      return expr;
    }
  }
  else
    waiting = true;
  
  if(Application::is_running_on_gui_thread()) {
    item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return item;
  }
  
  expr = pmath_expr_set_item(expr, 0, pmath_integer_new_siptr(pmath_dynamic_get_current_tracker_id()));
  
  if(waiting) {
    return Application::notify_wait(ClientNotification::CallFrontEnd, Expr{expr}).release();
  }
  else{
    Application::notify(ClientNotification::CallFrontEnd, Expr{expr});
    return PMATH_NULL;
  }
}

static pmath_t builtin_filedialog(pmath_expr_t _expr) {
  return Application::notify_wait(ClientNotification::FileDialog, Expr(_expr)).release();
}

static pmath_t builtin_documentapply_or_documentwrite(pmath_expr_t _expr) {
  Expr expr(_expr);
  if(expr.expr_length() != 2) {
    pmath_message_argxxx(expr.expr_length(), 2, 2);
    return expr.release();
  }
  
  Application::notify_wait(ClientNotification::MenuCommand, expr);
  
  return PMATH_NULL;
}

static pmath_t builtin_documentread(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  return Application::notify_wait(ClientNotification::DocumentRead, expr).release();
}

static pmath_t builtin_documentsave(pmath_expr_t _expr) {
  if(pmath_expr_length(_expr) > 2) {
    pmath_message_argxxx(pmath_expr_length(_expr), 0, 2);
    return _expr;
  }
  
  return Application::notify_wait(ClientNotification::Save, Expr(_expr)).release();
}

static pmath_t builtin_internal_dynamicupdated(pmath_expr_t expr) {
  Application::notify(ClientNotification::DynamicUpdate, Expr(expr));
  return PMATH_NULL;
}

static pmath_t builtin_frontendtokenexecute(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  Expr cmd = Expr(pmath_expr_get_item(expr, 1));
  pmath_unref(expr);
  
  Menus::run_command_async(cmd);
  
  return PMATH_NULL;
}

static pmath_t builtin_sectionprint(pmath_expr_t expr) {
  size_t exprlen = pmath_expr_length(expr);
  pmath_t sections;
  
  if(exprlen == 0)
    return expr;
  
  if(exprlen >= 2) {
    pmath_t style = pmath_expr_get_item(expr, 1);
    pmath_t boxes;
    
    if(exprlen == 2) {
      boxes = pmath_expr_get_item(expr, 2);
    }
    else {
      boxes = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
      boxes = pmath_expr_set_item(boxes, 0, pmath_ref(richmath_System_List));
      boxes = pmath_expr_new_extended(pmath_ref(richmath_System_Row), 2, boxes, PMATH_C_STRING(" "));
    }
    
    boxes = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(richmath_System_MakeBoxes), 1, // ToBoxes instead?
                boxes));
    
    sections = pmath_expr_new_extended(
                 pmath_ref(richmath_System_Section), 3,
                 pmath_expr_new_extended(
                   pmath_ref(richmath_System_BoxData), 1,
                   boxes),
                 style,
                 pmath_expr_new_extended(
                   pmath_ref(richmath_System_Rule), 2,
                   pmath_ref(richmath_System_SectionGenerated),
                   pmath_ref(richmath_System_True)));
  }
  else
    sections = pmath_expr_get_item(expr, 1);
  
  pmath_unref(expr);
  expr = PMATH_NULL;
  
  if(!pmath_is_expr_of(sections, richmath_System_List))
    sections = pmath_expr_new_extended(pmath_ref(richmath_System_List), 1, sections);
  
  for(size_t i = 1; i <= pmath_expr_length(sections); ++i) {
    pmath_t sect = pmath_expr_get_item(sections, i);
    
    if(!pmath_is_expr_of(sect, richmath_System_Section)) {
      pmath_t content = sect;
      sect = PMATH_NULL;
      
      if(!pmath_is_string(content)) {
        content = pmath_evaluate(
                    pmath_expr_new_extended(
                      pmath_ref(richmath_System_MakeBoxes), 1, // ToBoxes instead?
                      content));
        content = pmath_expr_new_extended(
                    pmath_ref(richmath_System_BoxData), 1,
                    content);
      }
      
      sect = pmath_expr_new_extended(
               pmath_ref(richmath_System_Section), 2,
               content,
               pmath_expr_new_extended(
                 pmath_ref(richmath_System_Rule), 2,
                 pmath_ref(richmath_System_SectionGenerated),
                 pmath_ref(richmath_System_True)));
    }
    
    Application::notify_wait(ClientNotification::PrintSection, Expr(sect));
  }
  
  pmath_unref(sections);
  return expr;
}

static pmath_t builtin_interrupt(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr); 
  expr = pmath_evaluate(pmath_expr_new(pmath_ref(richmath_System_Stack), 0));
  
  if(Application::is_running_on_gui_thread()) 
    expr = ask_interrupt(Expr(expr)).release();
  else
    expr = Application::notify_wait(ClientNotification::AskInterrupt, Expr(expr)).release();
  
  pmath_t ex = pmath_catch();
  if(!pmath_same(ex, PMATH_UNDEFINED)) {
    // e.g. a System`Pause`stop$xxx exception used internally by Pause()
    
    expr = pmath_evaluate(expr);
    pmath_throw(ex);
  }
  
  return expr;
}

//} ... pmath functions

//{ menu command availability checkers ...

static MenuCommandStatus can_save(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
  
  if(!doc->get_style(Saveable, true))
    return MenuCommandStatus(false);
  
  // TODO: check whether the document has a filename that has enough permissions...
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_abort_or_interrupt(Expr cmd) {
  return MenuCommandStatus(!Application::is_idle());
}

static MenuCommandStatus can_convert_dynamic_to_literal(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || doc->selection_length() == 0)
    return MenuCommandStatus(false);
    
  Box *sel = doc->selection_box();
  if(!sel || !sel->get_style(Editable))
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_copy_cut(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->can_copy())
    return MenuCommandStatus(false);
    
  if(cmd == strings::Cut) {
    Box *sel = doc->selection_box();
    return MenuCommandStatus(sel && sel->get_style(Editable));
  }
  
  return MenuCommandStatus(true);
}


static MenuCommandStatus can_open_close_group(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || doc->selection_length() == 0)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_do_scoped(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(cmd.expr_length() != 2)
    return MenuCommandStatus(false);
    
  return doc->can_do_scoped(cmd[1], cmd[2]);
}

static MenuCommandStatus can_document_write(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  Box *sel = doc->selection_box();
  return MenuCommandStatus(sel && sel->get_style(Editable));
}

static MenuCommandStatus can_duplicate_previous_input_output(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(doc->selection_box() == doc && doc->selection_length() > 0)
    return MenuCommandStatus(false);
    
  Box *box = doc->selection_box();
  int a = doc->selection_start();
  while(box && box != doc) {
    a = box->index();
    box = box->parent();
  }
  
  bool input = String(cmd).equals("DuplicatePreviousInput");
  
  for(int i = a - 1; i >= 0; --i) {
    auto math = dynamic_cast<MathSection*>(doc->item(i));
    
    if(math &&
        (( input && math->get_style(Evaluatable)) ||
         (!input && math->get_style(SectionGenerated))))
    {
      return MenuCommandStatus(true);
    }
  }
  
  return MenuCommandStatus(false);
}

static MenuCommandStatus can_edit_boxes(Expr cmd) {
  Document *doc = Documents::current();
  return MenuCommandStatus(doc && (doc->selection_length() > 0 || doc->selection_box() != doc) && doc->get_style(Editable));
}

static MenuCommandStatus can_expand_selection(Expr cmd) {
  Document *doc = Documents::current();
  
  return MenuCommandStatus(doc && doc->selection_box() && doc->selection_box() != doc);
}

static MenuCommandStatus can_evaluate_in_place(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(!dynamic_cast<MathSequence *>(doc->selection_box()))
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->selection_length() > 0);
}

static MenuCommandStatus can_evaluate_sections(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc)
    return MenuCommandStatus(false);
    
  Box *box = doc->selection_box();
  if(box == doc) {
    for(int i = doc->selection_start(); i < doc->selection_end(); ++i) {
      auto math = dynamic_cast<MathSection*>(doc->item(i));
      
      if(math && math->get_style(Evaluatable))
        return MenuCommandStatus(true);
    }
  }
  else {
    while(box && !dynamic_cast<MathSection *>(box))
      box = box->parent();
      
    auto math = dynamic_cast<MathSection*>(box);
    if(math && math->get_style(Evaluatable))
      return MenuCommandStatus(true);
  }
  
  return MenuCommandStatus(false);
}

static MenuCommandStatus can_find_evaluating_section(Expr cmd) {
  Box *box = Box::find_nearest_box(Application::find_current_job());
  if(!box)
    return MenuCommandStatus(false);
  
  Section *sect = box->find_parent<Section>(true);
  if(!sect)
    return MenuCommandStatus(false);
    
  Document *doc = sect->find_parent<Document>(false);
  if(!doc)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_find_matching_fence(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(auto seq = dynamic_cast<MathSequence *>(doc->selection_box())) {
    int pos = doc->selection_end() - 1;
    int match = seq->matching_fence(pos);
    
    if(match < 0 && doc->selection_start() == doc->selection_end()) {
      ++pos;
      match = seq->matching_fence(pos);
    }
    
    return MenuCommandStatus(match >= 0);
  }
  
  return MenuCommandStatus(false);
}

static MenuCommandStatus can_graphics_original_size(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(dynamic_cast<GraphicsBox *>(doc->selection_box()))
    return MenuCommandStatus(true);
    
  return MenuCommandStatus(doc->selection_length() > 0);
}

static MenuCommandStatus can_insert_inline_section_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(auto seq = dynamic_cast<AbstractSequence *>(doc->selection_box())) 
    return MenuCommandStatus(seq->get_style(Editable));
  
  return MenuCommandStatus(false);
}

static MenuCommandStatus can_insert_opposite_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  return MenuCommandStatus(doc->complete_box(false));
}

static MenuCommandStatus can_remove_from_evaluation_queue(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  int start = doc->selection_start();
  int end   = doc->selection_end();
  Box *box  = doc->selection_box();
  while(box && box != doc) {
    start = box->index();
    end   = start + 1;
    box   = box->parent();
  }
  
  if(!box || start >= end)
    return MenuCommandStatus(false);
    
  for(int i = end - 1; i >= start; --i) {
    if(Application::remove_job(doc->section(i), true))
      return MenuCommandStatus(true);
  }
  
  return MenuCommandStatus(false);
}

static MenuCommandStatus can_section_merge(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->merge_sections(false));
}

static MenuCommandStatus can_section_split(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->split_section(false));
}

static bool has_style(ActiveStyledObject *obj, StyleOptionName name, Expr rhs) {
  if(rhs == richmath_System_Inherited) {
    if(!obj->style)
      return true;
    
    return obj->style->get_pmath(name) == rhs;
  }

  return obj->get_pmath_style(name) == rhs;
}

static MenuCommandStatus can_set_style(Expr cmd) {
  StyleOptionName lhs_key = Style::get_key(cmd[1]);
  if(!lhs_key.is_valid())
    return MenuCommandStatus(false);
  
  MenuCommandStatus status(true);
  
  Expr rhs = cmd[2];
  if(lhs_key == MathFontFamily) {
    if(rhs.is_string())
       status.enabled = MathShaper::available_shapers.search(String(rhs));
  }
  
  Document *doc = Documents::current();
  ActiveStyledObject *obj;
  if(Menus::current_scope == MenuCommandScope::FrontEndSession)
    obj = Application::front_end_session;
  else if(doc)
    obj = doc->selection_box();
  else
    return MenuCommandStatus(false);
  
  if(!obj)
    return MenuCommandStatus(false);
  
  if(status.enabled)
    status.enabled = obj->get_style(Editable);
  
  if(Menus::current_scope == MenuCommandScope::Document) {
    status.checked = has_style(doc, lhs_key, rhs);
    return status;
  }
  
  if(obj && cmd.is_rule()) {
    int start = doc->selection_start();
    int end   = doc->selection_end();
    
    if(start < end) {
      if(obj == doc) {
        status.checked = true;
        
        for(int i = start; i < end; ++i) {
          status.checked = has_style(doc->item(i), lhs_key, rhs);
          if(!status.checked)
            break;
        }
        
        return status;
      }
    }

    status.checked = has_style(obj, lhs_key, rhs);
  }
  
  return status;
}

static MenuCommandStatus can_similar_section_below(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->get_style(Editable))
    return MenuCommandStatus(false);
    
  Box *box = doc->selection_box();
  while(box && box->parent() != doc) {
    box = box->parent();
  }
  
  return MenuCommandStatus(nullptr != dynamic_cast<AbstractSequenceSection *>(box));
}

static MenuCommandStatus can_subsession_evaluate_sections(Expr cmd) {
  return MenuCommandStatus(!can_abort_or_interrupt(Expr()).enabled && can_evaluate_sections(Expr()).enabled);
}

static MenuCommandStatus can_toggle_character_code(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->get_style(Editable))
    return MenuCommandStatus(false);
  
  if(!dynamic_cast<AbstractSequence*>(doc->selection_box()))
    return false;
  
  if(doc->selection_end() == 0)
    return false;
  
  return true;
}

//} ... menu command availability checkers

//{ menu commands ...

static bool abort_cmd(Expr cmd) {
  Application::abort_all_jobs();
  return true;
}

static bool close_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->native()->close();
  return true;
}

static bool convert_dynamic_to_literal(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || doc->selection_length() == 0)
    return false;
    
  VolatileSelection sel = doc->selection_now();
  if(!sel || !sel.box->get_style(Editable))
    return false;
    
  sel.dynamic_to_literal();
  doc->select(sel);
  return true;
}

static bool copy_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->can_copy())
    return false;
    
  doc->copy_to_clipboard(Clipboard::std);
  return true;
}

static bool copy_special_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->can_copy())
    return false;
    
  String format(cmd[1]);
  if(!format.is_valid())
    return false;
    
  doc->copy_to_clipboard(Clipboard::std, format);
  return true;
}

static bool cut_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc || !doc->can_copy())
    return false;
    
  doc->cut_to_clipboard(Clipboard::std);
  return true;
}

static bool do_kernelexecute_cmd(Expr cmd) {
  cmd = richmath_eval_FrontEnd_KernelExecute(std::move(cmd));
  return cmd != richmath_System_DollarFailed;
}

static bool do_scoped_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(cmd.expr_length() != 2)
    return false;
    
  return doc->do_scoped(cmd[1], cmd[2]);
}

static bool document_apply_cmd(Expr cmd) {
  Document *doc = nullptr;
  if(auto ref = FrontEndReference::from_pmath(cmd[1])) {
    doc = FrontEndObject::find_cast<Document>(ref);
  }
  else if(cmd[1] == richmath_System_Automatic) {
    doc = Documents::current();
  }
  
  if(!doc) 
    return false;
    
  Expr boxes = cmd[2];
  if(boxes[0] == richmath_System_Section || boxes[0] == richmath_System_SectionGroup) {
    Box *box = doc->selection_box();
    int i = doc->selection_end();
    while(box && box != doc) {
      i = box->index() + 1;
      box = box->parent();
    }
    
    if(!box) {
      box = doc;
      i = doc->length();
    }
    doc->insert_pmath(&i, boxes);
    doc->move_to(box, i);
    return true;
  }
  
  AbstractSequence *seq;
  if(dynamic_cast<TextSequence *>(doc->selection_box()))
    seq = new TextSequence;
  else
    seq = new MathSequence;
    
  seq->load_from_object(boxes, BoxInputFlags::Default);
  doc->insert_box(seq, true);
  
  return true;
}

static bool document_write_cmd(Expr cmd) {
  Document *doc = nullptr;
  if(auto ref = FrontEndReference::from_pmath(cmd[1])) {
    doc = FrontEndObject::find_cast<Document>(ref);
  }
  else if(cmd[1] == richmath_System_Automatic) {
    doc = Documents::current();
  }
  
  if(!doc)
    return false;
    
  AbstractSequence *seq;
  
  if(dynamic_cast<TextSequence *>(doc->selection_box()))
    seq = new TextSequence;
  else
    seq = new MathSequence;
    
  seq->load_from_object(cmd[2], BoxInputFlags::Default);
  doc->insert_box(seq, false);
  
  return true;
}

static bool duplicate_previous_input_output_cmd(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc)
    return false;
    
  if(doc->selection_box() == doc && doc->selection_length() > 0)
    return false;
    
  Box *box = doc->selection_box();
  int a = doc->selection_start();
  while(box && box != doc) {
    a = box->index();
    box = box->parent();
  }
  
  bool input = cmd == strings::DuplicatePreviousInput;
  
  for(int i = a - 1; i >= 0; --i) {
    auto math = dynamic_cast<MathSection*>(doc->item(i));
    
    if( math &&
        (( input && math->get_style(Evaluatable)) ||
         (!input && math->get_style(SectionGenerated))))
    {
      MathSequence *seq = new MathSequence;
      seq->load_from_object(Expr(math->content()->to_pmath(BoxOutputFlags::Default)), BoxInputFlags::Default);
      doc->insert_box(seq);
      
      return true;
    }
  }
  
  doc->native()->beep();
  return true;
}

static bool edit_boxes_cmd(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc || !doc->selectable(-1))
    return false;
    
  int a, b;
  a = doc->selection_start();
  b = doc->selection_end();
  Box *box = doc->selection_box();
  
  while(box) {
    if(box == doc)
      break;
    a = box->index();
    b = a + 1;
    box = box->parent();
  }
  
  if(box == doc) {
    SelectionReference old_sel = doc->selection();
    doc->select(nullptr, 0, 0);
    
    Array<LocationReference> old_loc;
    old_loc.add(old_sel.start_reference());
    old_loc.add(old_sel.end_reference());
    
    VolatileSelection tmp = old_sel.get_all();
    tmp.expand();
    bool is_at_word_start = tmp.start >= old_sel.start;
    
    Hashtable<LocationReference, SelectionReference> found_loc;
    
    for(int i = a; i < b; ++i) {
      pmath_continue_after_abort();
      
      if(!toggle_edit_section(doc->section(i), old_loc, found_loc))
        doc->native()->beep();
    }
    
    SelectionReference *final_sel_1 = found_loc.search(old_sel.start_reference());
    SelectionReference *final_sel_2 = found_loc.search(old_sel.end_reference());
    
    if(final_sel_1 && final_sel_2) {
      if(final_sel_1->id == final_sel_2->id && final_sel_1->end <= final_sel_2->start) {
        Box *box = final_sel_1->get();
        doc->select(box, final_sel_1->end, final_sel_2->start);
        return true;
      }
      
      if(old_sel.length() == 0 && *final_sel_1 == *final_sel_2) {
        Box *box = final_sel_1->get();
        if(is_at_word_start)
          doc->select(box, final_sel_1->end, final_sel_1->end);
        else
          doc->select(box, final_sel_1->start, final_sel_1->start);
        return true;
      }
      
      doc->select_range(final_sel_1->get_all(), final_sel_2->get_all());
    }
    else 
      doc->select_range(VolatileSelection(doc, a), VolatileSelection(doc, b));
  }
  
  return true;
}

static bool evaluate_in_place_cmd(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc)
    return false;
    
  auto seq = dynamic_cast<MathSequence*>(doc->selection_box());
  
  if(seq && doc->selection_length() > 0) {
    Application::add_job(new ReplacementJob(
                           seq,
                           doc->selection_start(),
                           doc->selection_end()));
  }
  
  return true;
}

static bool evaluate_sections_cmd(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc)
    return false;
    
  Box *box = doc->selection_box();
  
  if(box == doc) {
    bool found_any = false;
    int start = doc->selection_start();
    int end = doc->selection_end();
    for(int i = start; i < end; ++i) {
      auto math = dynamic_cast<MathSection*>(doc->item(i));
      if(math && math->get_style(Evaluatable)) {
        Application::add_job(new InputJob(math));
        found_any = true;
      }
    }
    if(!found_any)
      return false;
  }
  else if(box) {
    auto math = box->find_parent<MathSection>(true);
    if(math && math->get_style(Evaluatable)) {
      Application::add_job(new InputJob(math));
    }
    else {
      if(dynamic_cast<AbstractSequence *>(box))
        doc->insert_string("\n", false);
        
      return false;
    }
  }
  
  if(cmd == strings::EvaluateSectionsAndReturn) {
    Application::add_job(new EvaluationJob(Call(Symbol(richmath_System_Return))));
  }
  
  return true;
}

static bool evaluator_subsession_cmd(Expr cmd) {
  if(Application::is_idle())
    return false;
    
  Application::async_interrupt(Call(Symbol(richmath_System_Dialog)));
  
  return true;
}

static bool expand_selection_cmd(Expr cmd) {
  Document *doc = Documents::current();
  
  if(!doc)
    return false;
    
  VolatileSelection sel = doc->selection_now();
  sel.expand();
  doc->select_range(sel.start_only(), sel.end_only());
  doc->native()->invalidate();
  return true;
}

static bool find_evaluating_section(Expr cmd) {
  Document *current_doc = Documents::current();

  Box *box = Box::find_nearest_box(Application::find_current_job());
  if(!box) {
    if(current_doc)
      current_doc->native()->beep();
    return false;
  }
  
  auto sect = box->find_parent<Section>(true);
  if(!sect) {
    if(current_doc)
      current_doc->native()->beep();
    return false;
  }
  
  auto doc = sect->find_parent<Document>(false);
  if(!doc) {
    if(current_doc)
      current_doc->native()->beep();
    return false;
  }
  
  doc->native()->bring_to_front();
  doc->select(sect->parent(), sect->index(), sect->index() + 1);
  return true;
}

static bool find_matching_fence_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(auto seq = dynamic_cast<MathSequence *>(doc->selection_box())) {
    int pos = doc->selection_start();
    int match = seq->matching_fence(pos);
    
    if(match < 0 && doc->selection_length() == 0 && pos > 0) {
      --pos;
      match = seq->matching_fence(pos);
    }
    
    if(match >= 0) {
      doc->select(seq, match, match + 1);
    }
  }
  
  return true;
}

static bool graphics_original_size_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->graphics_original_size();
  return true;
}

static bool insert_column_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_matrix_column();
  return true;
}

static bool insert_fraction_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_fraction();
  return true;
}

static bool insert_inline_section_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  auto seq = dynamic_cast<AbstractSequence*>(doc->selection_box());
  if(!seq)
    return false;
  
  InlineSequenceBox *new_box = nullptr;
  switch(seq->kind()) {
    case LayoutKind::Math: new_box = new InlineSequenceBox(new TextSequence); break;
    case LayoutKind::Text: new_box = new InlineSequenceBox(new MathSequence); break;
  }
  
  if(!new_box)
    return false;
  
  new_box->has_explicit_head(true);
  new_box->content()->insert(0, PMATH_CHAR_SELECTIONPLACEHOLDER);
  doc->insert_box(new_box, true);
  
  return true;
}

static bool insert_opposite_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(doc->complete_box(true))
    return true;
  
  doc->native()->beep();
  return false;
}

static bool insert_overscript_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_underoverscript(false);
  return true;
}

static bool insert_radical_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_sqrt();
  return true;
}

static bool insert_row_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_matrix_row();
  return true;
}

static bool insert_subscript_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_subsuperscript(true);
  return true;
}

static bool insert_superscript_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_subsuperscript(false);
  return true;
}

static bool insert_underscript_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->insert_underoverscript(true);
  return true;
}

static bool interrupt_cmd(Expr cmd) {
  Application::async_interrupt(Call(Symbol(richmath_System_Interrupt)));
  return true;
}

static bool new_cmd(Expr cmd) {
  Document *doc = Application::try_create_document();
  if(!doc)
    return false;
      
  if(Document *cur = Documents::current()) {
    doc->native()->try_set_menubar(cur->native()->has_menubar());
  }
  doc->on_style_changed(true);
  doc->native()->bring_to_front();
  
  return true;
}

static bool open_cmd(Expr cmd) {
  Expr filter = List(
                  Rule(String("pMath Documents (*.pmathdoc)"), String("*.pmathdoc")),
                  Rule(String("All Files (*.*)"),              String("*.*")));
                  
  Expr filenames = Application::run_filedialog(
                     Call(
                       Symbol(richmath_FE_FileOpenDialog),
                       filter));
                       
  if(filenames.is_string())
    filenames = List(filenames);
    
  if(filenames[0] != richmath_System_List)
    return false;
    
  for(size_t i = 1; i <= filenames.expr_length(); ++i) {
    String filename = filenames[i];
    // TODO: canonicalize filename
    Document *doc = Application::find_open_document(filename);
    if(!doc) {
      doc = Application::open_new_document(filename);
      if(!doc)
        continue;
      
      RecentDocuments::add(filename);
    }
    
    doc->native()->bring_to_front();
  }
  
  return true;
}

static bool open_close_group_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->toggle_open_close_current_group();
  return true;
}

static bool paste_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  doc->paste_from_clipboard(Clipboard::std);
  return true;
}

static bool remove_from_evaluation_queue(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  int start = doc->selection_start();
  int end   = doc->selection_end();
  Box *box  = doc->selection_box();
  while(box && box != doc) {
    start = box->index();
    end   = start + 1;
    box   = box->parent();
  }
  
  if(!box || start >= end) {
    doc->native()->beep();
    return false;
  }
  
  bool found_any = false;
  for(int i = end - 1; i >= start; --i) {
    if(Application::remove_job(doc->section(i), false))
      found_any = true;
  }
  
  if(!found_any) {
    doc->native()->beep();
    return false;
  }
  
  return true;
}

static bool save_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  if(!doc->get_style(Saveable, true))
    return false;
  
  Application::notify_wait(ClientNotification::Save, List(Symbol(richmath_System_Automatic), Symbol(richmath_System_Automatic)));
  
  // TODO: check whether the document has a filename that has enough permissions...
  return true;
}

static bool saveas_cmd(Expr cmd) {
  Application::notify_wait(ClientNotification::Save, List(Symbol(richmath_System_Automatic), Symbol(richmath_System_None)));
  
  return true;
}

static bool section_merge_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(doc->merge_sections(true))
    return true;
    
  doc->native()->beep();
  return false;
}

static bool section_split_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(doc->split_section(true))
    return true;
    
  doc->native()->beep();
  return false;
}

static bool select_all_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  Box *sel = doc->selection_box();
  if(!sel) {
    if(doc->selectable()) {
      doc->select(doc, 0, doc->length());
      return true;
    }
    
    return false;
  }
    
  while(sel && sel->selection_exitable(true)) {
    Box *next = sel->parent();
    if(!next || !next->selectable())
      break;
    sel = next;
  }
  
  if(sel->selectable()) {
    doc->select(sel, 0, sel->length());
    return true;
  }
  
  return false;
}

static bool set_style_cmd(Expr cmd) {
  if(Menus::current_scope == MenuCommandScope::FrontEndSession) {
    Application::front_end_session->style->add_pmath(cmd);
    for(auto win : CommonDocumentWindow::All) {
      win->content()->on_style_changed(true);
    }
    return true;
  }
  
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  if(Menus::current_scope == MenuCommandScope::Document) {
    if(cmd.is_rule() && cmd[1] == richmath_System_StyleDefinitions) {
      if(Expr style_def = doc->get_own_style(StyleDefinitions)) {
        if(style_def[0] == richmath_System_Document) {
          if(ask_remove_private_style_definitions(doc) != YesNoCancel::Yes)
            return false;
          
          if(auto style_doc = doc->native()->stylesheet_document())
            style_doc->native()->close();
        }
      }
    }
    
    doc->style->add_pmath(cmd);
    doc->on_style_changed(true);
    return true;
  }
  
  doc->set_selection_style(cmd);
  return true;
}

static bool similar_section_below_cmd(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
    
  Box *box = doc->selection_box();
  while(box && box->parent() != doc) {
    box = box->parent();
  }
  
  if(dynamic_cast<AbstractSequenceSection *>(box)) {
    SharedPtr<Style> style = new Style;
    style->merge(static_cast<Section *>(box)->style);
    style->remove(SectionLabel);
    style->remove(SectionGenerated);
    
    Section *section;
    //if(!dynamic_cast<TextSection *>(box))
    if(box->get_own_style(LanguageCategory).equals("pMath"))
      section = new MathSection(style);
    else
      section = new TextSection(style);
      
    doc->insert(box->index() + 1, section);
    doc->move_to(doc, box->index() + 1);
    doc->move_horizontal(LogicalDirection::Forward, false);
    return true;
  }
  
  doc->native()->beep();
  return false;
}

static bool subsession_evaluate_sections_cmd(Expr cmd) {
  Application::async_interrupt(
    Call(Symbol(richmath_System_Dialog),
         Call(Symbol(richmath_System_FrontEndTokenExecute),
              strings::EvaluateSectionsAndReturn)));
              
  return false;
}

static bool toggle_character_code(Expr cmd) {
  Document *doc = Documents::current();
  if(!doc)
    return false;
  
  AbstractSequence *seq = nullptr;
  auto sel = doc->selection();
  if(auto box = sel.get()) {
    if(!box->edit_selection(sel))
      return false;
    
    seq = dynamic_cast<AbstractSequence*>(sel.get());
  }
  
  if(!seq)
    return false;
  
  ArrayView<const uint16_t> buf = buffer_view(seq->text());
  
  if(sel.length() == 0 && sel.start > 0) {
    --sel.start;
    if(pmath_char_is_hexdigit(buf[sel.start])) {
      while(sel.start > 0 && pmath_char_is_hexdigit(buf[sel.start - 1]))
        --sel.start;
    }
    else if(sel.start > 0 && is_utf16_low(buf[sel.start]) && is_utf16_high(buf[sel.start - 1])) {
      --sel.start;
    }
  }
  
  if(sel.length() == 0)
    return false;
  
  uint32_t unichar = 0;
  if(pmath_char_is_hexdigit(buf[sel.end - 1])) {
    if(sel.length() > 6)
      return false;
    
    for(int i = sel.start; i < sel.end; ++i) {
      auto digit = buf[i];
      if('0' <= digit && digit <= '9')
        unichar = 16 * unichar + (digit - '0');
      else if('a' <= digit && digit <= 'f')
        unichar = 16 * unichar + (digit - 'a' + 10);
      else if('A' <= digit && digit <= 'F')
        unichar = 16 * unichar + (digit - 'A' + 10);
      else
        return false;
    }
    
    if(unichar < ' ' || unichar == PMATH_CHAR_BOX || unichar > 0x10FFFF)
      return false;
    
    if(unichar <= 0xFFFF) {
      if(is_utf16_high(unichar) || is_utf16_low(unichar))
        return false;
    }
    
    doc->native()->on_editing();
    int pos = seq->insert(sel.end, unichar);
    seq->remove(sel.start, sel.end);
    doc->select(seq, sel.start + pos - sel.end, sel.start + pos - sel.end);
    return true;
  }
  
  unichar = buf[sel.end - 1];
  if(unichar == PMATH_CHAR_BOX)
    return false;
  
  if(is_utf16_low(unichar) && sel.start <= sel.end - 2 && is_utf16_high(buf[sel.end - 2])) {
    uint32_t hi = buf[sel.end - 2];
    uint32_t lo = unichar;
    unichar = 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
  }
  
  const char hex[] = "0123456789ABCDEF";
  char digits_buf[6];
  digits_buf[5] = hex[ unichar        & 0xF];
  digits_buf[4] = hex[(unichar >>  4) & 0xF];
  digits_buf[3] = hex[(unichar >>  8) & 0xF];
  digits_buf[2] = hex[(unichar >> 12) & 0xF];
  digits_buf[1] = hex[(unichar >> 16) & 0xF];
  digits_buf[0] = hex[(unichar >> 20) & 0xF];
  
  char *digits = digits_buf;
  int num_digits = sizeof(digits_buf);
  while(num_digits > 1 && *digits == '0') {
    ++digits;
    --num_digits;
  }
  
  doc->native()->on_editing();
  seq->insert(sel.end, digits, num_digits);
  seq->remove(sel.start, sel.end);
  doc->select(seq, sel.start, sel.start + num_digits);
  return true;
}

//} ... menu commands
