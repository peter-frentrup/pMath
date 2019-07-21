#include <eval/binding.h>

#include <boxes/graphics/graphicsbox.h>
#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/textsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/document.h>
#include <gui/native-widget.h>
#include <util/spanexpr.h>
#include <util/selection-tracking.h>


using namespace richmath;

//{ pmath functions ...

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_System_SectionGenerated;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_StyleDefinitions;

static pmath_t builtin_addconfigshaper(pmath_expr_t expr) {
  double start = pmath_tickcount();
  Expr filename(pmath_expr_get_item(expr, 1));
  Expr data = Expr(
                pmath_evaluate(
                  pmath_expr_new_extended(
                    pmath_ref(PMATH_SYMBOL_GET), 1,
                    pmath_ref(filename.get()))));
                    
  pmath_unref(expr);
  
  double end = pmath_tickcount();
  
  pmath_debug_print("[%f sec reading ", end - start);
  pmath_debug_print_object("", filename.get(), "]\n");
  
  Application::notify(ClientNotification::AddConfigShaper, data);
  return PMATH_NULL;
}

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
    if(pmath_same(item, PMATH_SYMBOL_TRUE)) {
      pmath_unref(item);
      waiting = true;
    }
    else if(pmath_same(item, PMATH_SYMBOL_FALSE)) {
      pmath_unref(item);
      waiting = false;
    }
    else {
      pmath_message(PMATH_NULL, "bool", 1, item);
      return expr;
    }
  }
  else
    waiting = true;
  
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(Application::is_running_on_gui_thread())
    return item;
  
  if(waiting) {
    return Application::notify_wait(ClientNotification::CallFrontEnd, Expr{item}).release();
  }
  else{
    Application::notify(ClientNotification::CallFrontEnd, Expr{item});
    return PMATH_NULL;
  }
}

static pmath_t builtin_filedialog(pmath_expr_t _expr) {
  return Application::notify_wait(ClientNotification::FileDialog, Expr(_expr)).release();
}

static pmath_t builtin_internalexecutefor(pmath_expr_t _expr) {
  if(pmath_expr_length(_expr) != 4)
    return _expr;
  
  Expr expr {_expr};
  
  pmath_debug_print_object("[", _expr, "]\n");
  
  return Application::internal_execute_for(
           expr[1],
           FrontEndReference::from_pmath_raw(expr[2]),
           FrontEndReference::from_pmath_raw(expr[3]),
           FrontEndReference::from_pmath_raw(expr[4])).release();
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

static pmath_t builtin_documentdelete(pmath_expr_t _expr) {
  Expr expr(_expr);
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  Application::notify_wait(ClientNotification::MenuCommand, expr);
  
  return PMATH_NULL;
}

static pmath_t builtin_documentget(pmath_expr_t _expr) {
  Expr expr(_expr);
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  return Application::notify_wait(ClientNotification::DocumentGet, expr).release();
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

static pmath_t builtin_assign_currentvalue(pmath_expr_t expr) {
  if(pmath_is_expr_of_len(expr, PMATH_SYMBOL_ASSIGN, 2)) {
    pmath_t lhs = pmath_expr_get_item(expr, 1);
    if(pmath_is_expr_of(lhs, PMATH_SYMBOL_CURRENTVALUE)) {
      pmath_unref(lhs);
      return Application::notify_wait(ClientNotification::SetCurrentValue, Expr(expr)).release();
    }
    pmath_unref(lhs);
  }
  return expr;
}

static pmath_t builtin_currentvalue(pmath_expr_t expr) {
  return Application::notify_wait(ClientNotification::CurrentValue, Expr(expr)).release();
}

static pmath_t builtin_internal_dynamicupdated(pmath_expr_t expr) {
  Application::notify(ClientNotification::DynamicUpdate, Expr(expr));
  return PMATH_NULL;
}

static Expr cpp_builtin_feo_options(Expr expr) {
  if(expr[0] == PMATH_SYMBOL_OPTIONS) {
    if(expr.expr_length() == 1) {
      return Application::notify_wait(ClientNotification::GetOptions, expr[1]);
    }
    
    if( expr.expr_length() == 2 &&
        expr[1][0] == richmath_System_FrontEndObject)
    {
      Expr opts = Application::notify_wait(ClientNotification::GetOptions, expr[1]);
      
      expr.set(1, opts);
    }
  }
  else if(expr[0] == PMATH_SYMBOL_SETOPTIONS) {
    if( expr.expr_length() >= 1 &&
        expr[1][0] == richmath_System_FrontEndObject)
    {
      return Application::notify_wait(ClientNotification::SetOptions, expr);
    }
  }
  
  return expr;
}

static pmath_t builtin_feo_options(pmath_expr_t _expr) {
  Expr result(cpp_builtin_feo_options(Expr(_expr)));
  
  return result.release();
}

static pmath_t builtin_frontendtokenexecute(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  Expr cmd = Expr(pmath_expr_get_item(expr, 1));
  pmath_unref(expr);
  
  Application::run_menucommand(cmd);
  
  return PMATH_NULL;
}

static pmath_t builtin_sectionprint(pmath_expr_t expr) {
  if(pmath_expr_length(expr) == 2) {
    pmath_t style = pmath_expr_get_item(expr, 1);
    pmath_t boxes = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    
    boxes = pmath_evaluate(
              pmath_expr_new_extended(
                pmath_ref(PMATH_SYMBOL_TOBOXES), 1,
                boxes));
                
    expr = pmath_expr_new_extended(
             pmath_ref(richmath_System_Section), 3,
             pmath_expr_new_extended(
               pmath_ref(richmath_System_BoxData), 1,
               boxes),
             style,
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_RULE), 2,
               pmath_ref(richmath_System_SectionGenerated),
               pmath_ref(PMATH_SYMBOL_TRUE)));
               
    Application::notify_wait(ClientNotification::PrintSection, Expr(expr));
    
    return PMATH_NULL;
  }
  
  return expr;
}

static pmath_t builtin_evaluationdocument(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return Application::notify_wait(ClientNotification::GetEvaluationDocument, Expr()).release();
}

//} ... pmath functions

//{ menu command availability checkers ...

static MenuCommandStatus can_save(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
  
  if(!doc->get_style(Saveable, true))
    return MenuCommandStatus(false);
  
  // TODO: check whether the document has a filename that has enough permissions...
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_abort(Expr cmd) {
  return MenuCommandStatus(!Application::is_idle());
}

static MenuCommandStatus can_convert_dynamic_to_literal(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || doc->selection_length() == 0)
    return MenuCommandStatus(false);
    
  Box *sel = doc->selection_box();
  if(!sel || !sel->get_style(Editable))
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_copy_cut(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || !doc->can_copy())
    return MenuCommandStatus(false);
    
  if(String(cmd).equals("Cut")) {
    Box *sel = doc->selection_box();
    return MenuCommandStatus(sel && sel->get_style(Editable));
  }
  
  return MenuCommandStatus(true);
}


static MenuCommandStatus can_open_close_group(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || doc->selection_length() == 0)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_do_scoped(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(cmd.expr_length() != 2)
    return MenuCommandStatus(false);
    
  return doc->can_do_scoped(cmd[1], cmd[2]);
}

static MenuCommandStatus can_document_write(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  Box *sel = doc->selection_box();
  return MenuCommandStatus(sel && sel->get_style(Editable));
}

static MenuCommandStatus can_duplicate_previous_input_output(Expr cmd) {
  Document *doc = get_current_document();
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
  Document *doc = get_current_document();
  return MenuCommandStatus(doc && (doc->selection_length() > 0 || doc->selection_box() != doc) && doc->get_style(Editable));
}

static MenuCommandStatus can_edit_style_definitions(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || !doc->get_style(Editable))
    return MenuCommandStatus(false);
  
  if(doc->native()->owner_document())
    return MenuCommandStatus(false);
  
  return MenuCommandStatus(true);
}

static MenuCommandStatus can_expand_selection(Expr cmd) {
  Document *doc = get_current_document();
  
  return MenuCommandStatus(doc && doc->selection_box() && doc->selection_box() != doc);
}

static MenuCommandStatus can_evaluate_in_place(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(!dynamic_cast<MathSequence *>(doc->selection_box()))
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->selection_length() > 0);
}

static MenuCommandStatus can_evaluate_sections(Expr cmd) {
  Document *doc = get_current_document();
  
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
  Box *box = Application::find_current_job();
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
  Document *doc = get_current_document();
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
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  if(dynamic_cast<GraphicsBox *>(doc->selection_box()))
    return MenuCommandStatus(true);
    
  return MenuCommandStatus(doc->selection_length() > 0);
}

static MenuCommandStatus can_remove_from_evaluation_queue(Expr cmd) {
  Document *doc = get_current_document();
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
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->merge_sections(false));
}

static MenuCommandStatus can_section_split(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  return MenuCommandStatus(doc->split_section(false));
}

static bool has_style(Box *box, StyleOptionName name, Expr rhs) {
  if(rhs == PMATH_SYMBOL_INHERITED) {
    if(!box->style)
      return true;
    
    return box->style->get_pmath(name) == rhs;
  }

  return box->get_pmath_style(name) == rhs;
}

static MenuCommandStatus can_set_style(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return MenuCommandStatus(false);
    
  Box *sel = doc->selection_box();
  
  MenuCommandStatus status(sel && sel->get_style(Editable));
  
  StyleOptionName lhs_key = Style::get_key(cmd[1]);
  if(!lhs_key.is_valid())
    return status;
    
  Expr rhs = cmd[2];
  if(status.enabled) {
    if(lhs_key == MathFontFamily) {
      if(rhs.is_string())
         status.enabled = MathShaper::available_shapers.search(String(rhs));
    }
  }

  if(Application::menu_command_scope == MenuCommandScope::Document) {
    status.checked = has_style(doc, lhs_key, rhs);
    
    return status;
  }
  
  if(sel && cmd.is_rule()) {
    int start = doc->selection_start();
    int end   = doc->selection_end();
    
    if(start < end) {
      if(sel == doc) {
        status.checked = true;
        
        for(int i = start; i < end; ++i) {
          status.checked = has_style(sel->item(i), lhs_key, rhs);
          if(!status.checked)
            break;
        }
        
        return status;
      }
    }

    status.checked = has_style(sel, lhs_key, rhs);
  }
  
  return status;
}

static MenuCommandStatus can_similar_section_below(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || !doc->get_style(Editable))
    return MenuCommandStatus(false);
    
  Box *box = doc->selection_box();
  while(box && box->parent() != doc) {
    box = box->parent();
  }
  
  return MenuCommandStatus(nullptr != dynamic_cast<AbstractSequenceSection *>(box));
}

static MenuCommandStatus can_subsession_evaluate_sections(Expr cmd) {
  return MenuCommandStatus(!can_abort(Expr()).enabled && can_evaluate_sections(Expr()).enabled);
}

//} ... menu command availability checkers

//{ menu commands ...

static bool abort_cmd(Expr cmd) {
  Application::abort_all_jobs();
  return true;
}

static bool close_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->native()->close();
  return true;
}

static bool convert_dynamic_to_literal(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || doc->selection_length() == 0)
    return false;
    
  Box *sel = doc->selection_box();
  if(!sel || !sel->get_style(Editable))
    return false;
    
  int start = doc->selection_start();
  int end   = doc->selection_end();
  sel = sel->dynamic_to_literal(&start, &end);
  doc->select(sel, start, end);
  
  return true;
}

static bool copy_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || !doc->can_copy())
    return false;
    
  doc->copy_to_clipboard();
  return true;
}

static bool copy_special_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || !doc->can_copy())
    return false;
    
  String format(cmd[1]);
  if(!format.is_valid())
    return false;
    
  doc->copy_to_clipboard(format);
  return true;
}

static bool cut_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc || !doc->can_copy())
    return false;
    
  doc->cut_to_clipboard();
  return true;
}

static bool do_scoped_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  if(cmd.expr_length() != 2)
    return false;
    
  return doc->do_scoped(cmd[1], cmd[2]);
}

static bool document_apply_cmd(Expr cmd) {
  auto ref = FrontEndReference::from_pmath(cmd[1]);
  auto doc = FrontEndObject::find_cast<Document>(ref);
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

static bool document_delete_cmd(Expr cmd) {
  Document *doc;
  
  if(cmd.expr_length() == 0)
    doc = get_current_document();
  else
    doc = FrontEndObject::find_cast<Document>(FrontEndReference::from_pmath(cmd[1]));
    
  if(!doc)
    return false;
    
  doc->remove_selection();
  
  return true;
}

static bool document_write_cmd(Expr cmd) {
  auto ref = FrontEndReference::from_pmath(cmd[1]);
  auto doc = FrontEndObject::find_cast<Document>(ref);
  
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
  Document *doc = get_current_document();
  
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
  
  bool input = String(cmd).equals("DuplicatePreviousInput");
  
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
  Document *doc = get_current_document();
  
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
    
    Hashtable<LocationReference, SelectionReference> found_loc;
    
    for(int i = a; i < b; ++i) {
      pmath_continue_after_abort();
      
      if(!toggle_edit_section(doc->section(i), old_loc, found_loc))
        doc->native()->beep();
    }
    
    SelectionReference *final_sel_1 = found_loc.search(old_sel.start_reference());
    SelectionReference *final_sel_2 = found_loc.search(old_sel.end_reference());
    
    if(final_sel_1 && final_sel_2) {
      Box *box1 = final_sel_1->get();
      Box *box2 = final_sel_2->get();
      doc->select_range(
        box1, final_sel_1->start, final_sel_1->end, 
        box2, final_sel_2->start, final_sel_2->end);
    }
    else 
      doc->select_range(doc, a, a, doc, b, b);
  }
  
  return true;
}

static bool edit_style_definitions_cmd(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || !doc->get_style(Editable))
    return false;
  
  if(doc->native()->owner_document())
    return false;
  
  Document *style_doc = doc->native()->stylesheet_document();
  if(!style_doc) {
    Expr stylesheet = doc->get_style(StyleDefinitions);
    if(stylesheet[0] != PMATH_SYMBOL_DOCUMENT) {
      stylesheet = Call(
                     Symbol(PMATH_SYMBOL_DOCUMENT),
                     Call(
                       Symbol(richmath_System_Section),
                       Call(
                         Symbol(richmath_System_StyleData),
                         Rule(Symbol(richmath_System_StyleDefinitions), stylesheet))),
                     Rule(Symbol(richmath_System_StyleDefinitions), String("PrivateStyleDefinitions.pmathdoc")));
    }
    
    style_doc = Application::create_document(stylesheet);
    if(!style_doc)
      return false;
    
    if(!doc->native()->stylesheet_document(style_doc))
      doc->native()->beep();
    
    doc->style->set(StyleDefinitions, stylesheet);
    doc->invalidate_options();
    style_doc->invalidate_options();
  }
  
  style_doc->native()->bring_to_front();
  return true;
}

static bool evaluate_in_place_cmd(Expr cmd) {
  Document *doc = get_current_document();
  
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
  Document *doc = get_current_document();
  
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
  
  if(String(cmd).equals("EvaluateSectionsAndReturn")) {
    Application::add_job(new EvaluationJob(Call(Symbol(PMATH_SYMBOL_RETURN))));
  }
  
  return true;
}

static bool evaluator_subsession_cmd(Expr cmd) {
  if(Application::is_idle())
    return false;
    
  Application::async_interrupt(Call(Symbol(PMATH_SYMBOL_DIALOG)));
  
  return true;
}

static bool expand_selection_cmd(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  Box *box  = doc->selection_box();
  int start = doc->selection_start();
  int end   = doc->selection_end();
  
  box = expand_selection(box, &start, &end);
  doc->select_range(box, start, start, box, end, end);
  doc->native()->invalidate();
  return true;
}

static bool find_evaluating_section(Expr cmd) {
  Box *box = Application::find_current_job();
  Document *current_doc = get_current_document();
  
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
  Document *doc = get_current_document();
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
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->graphics_original_size();
  return true;
}

static bool insert_column_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_matrix_column();
  return true;
}

static bool insert_fraction_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_fraction();
  return true;
}

static bool insert_opposite_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->complete_box();
  return true;
}

static bool insert_overscript_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_underoverscript(false);
  return true;
}

static bool insert_radical_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_sqrt();
  return true;
}

static bool insert_row_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_matrix_row();
  return true;
}

static bool insert_subscript_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_subsuperscript(true);
  return true;
}

static bool insert_superscript_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_subsuperscript(false);
  return true;
}

static bool insert_underscript_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->insert_underoverscript(true);
  return true;
}

static bool new_cmd(Expr cmd) {
//  Application::notify(
//    ClientNotification::CreateDocument,
//    Call(Symbol(PMATH_SYMBOL_CREATEDOCUMENT), List()));
  Document *doc = Application::create_document();
  if(!doc)
    return false;
    
  doc->invalidate_options();
  doc->native()->bring_to_front();
  
  return true;
}

extern pmath_symbol_t richmath_FE_FileOpenDialog;
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
    
  if(filenames[0] != PMATH_SYMBOL_LIST)
    return false;
    
  for(size_t i = 1; i <= filenames.expr_length(); ++i) {
    String filename = filenames[i];
    // TODO: canonicalize filename
    Document *doc = Application::find_open_document(filename);
    if(!doc) {
      doc = Application::open_new_document(filename);
      if(!doc)
        continue;
      
      doc->invalidate_options();
    }
    
    doc->native()->bring_to_front();
  }
  
  return true;
}

static bool open_close_group_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->toggle_open_close_current_group();
  return true;
}

static bool paste_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  doc->paste_from_clipboard();
  return true;
}

static bool remove_from_evaluation_queue(Expr cmd) {
  Document *doc = get_current_document();
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
  Document *doc = get_current_document();
  if(!doc)
    return false;
  
  if(!doc->get_style(Saveable, true))
    return false;
  
  Application::notify_wait(ClientNotification::Save, List(Symbol(PMATH_SYMBOL_AUTOMATIC), Symbol(PMATH_SYMBOL_AUTOMATIC)));
  
  // TODO: check whether the document has a filename that has enough permissions...
  return true;
}

static bool saveas_cmd(Expr cmd) {
  Application::notify_wait(ClientNotification::Save, List(Symbol(PMATH_SYMBOL_AUTOMATIC), Symbol(PMATH_SYMBOL_NONE)));
  
  return true;
}

static bool section_merge_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  if(doc->merge_sections(true))
    return true;
    
  doc->native()->beep();
  return false;
}

static bool section_split_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  if(doc->split_section(true))
    return true;
    
  doc->native()->beep();
  return false;
}

static bool select_all_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  if(doc->selectable()) {
    doc->select(doc, 0, doc->length());
    return true;
  }
  
  Box *sel = doc->selection_box();
  if(!sel)
    return false;
    
  Box *next = sel;
  while(next && next->selectable()) {
    sel = next;
    next = next->parent();
  }
  
  if(sel->selectable()) {
    doc->select(sel, 0, sel->length());
    return true;
  }
  
  return false;
}

static bool set_style_cmd(Expr cmd) {
  Document *doc = get_current_document();
  if(!doc)
    return false;
    
  if(Application::menu_command_scope == MenuCommandScope::Document) {
    doc->style->add_pmath(cmd);
    doc->invalidate_options();
    doc->invalidate();
    return true;
  }
  
  doc->set_selection_style(cmd);
  return true;
  
//  if(cmd.is_rule()) {
//    Box *box = doc->selection_box();
//    while(box && box->parent() != doc) {
//      box = box->parent();
//    }
//
//    if(!box)
//      box = doc;
//
//    // see also ClientNotification::GetOptions
//    Expr box_symbol = box->to_pmath_symbol();
//    if(!box_symbol.is_symbol()) // TODO: generate StyleBox inside sequences ...
//      return false;
//
//    cpp_builtin_feo_options(
//      Call(Symbol(PMATH_SYMBOL_SETOPTIONS), box->id().to_pmath(), cmd));
//
//    return true;
//  }
//
//  return false;
}

static bool similar_section_below_cmd(Expr cmd) {
  Document *doc = get_current_document();
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
    Call(Symbol(PMATH_SYMBOL_DIALOG),
         Call(Symbol(PMATH_SYMBOL_FRONTENDTOKENEXECUTE),
              String("EvaluateSectionsAndReturn"))));
              
  return false;
}

//} ... menu commands

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

bool richmath::init_bindings() {
  Application::register_menucommand(String("New"),                        new_cmd);
  Application::register_menucommand(String("Open"),                       open_cmd);
  Application::register_menucommand(String("Save"),                       save_cmd,                            can_save);
  Application::register_menucommand(String("SaveAs"),                     saveas_cmd);
  Application::register_menucommand(String("Close"),                      close_cmd);
  
  Application::register_menucommand(String("Copy"),                       copy_cmd,                            can_copy_cut);
  Application::register_menucommand(String("Cut"),                        cut_cmd,                             can_copy_cut);
  Application::register_menucommand(String("OpenCloseGroup"),             open_close_group_cmd,                can_open_close_group);
  Application::register_menucommand(String("Paste"),                      paste_cmd,                           can_document_write);
  Application::register_menucommand(String("GraphicsOriginalSize"),       graphics_original_size_cmd,          can_graphics_original_size);
  Application::register_menucommand(String("EditBoxes"),                  edit_boxes_cmd,                      can_edit_boxes);
  Application::register_menucommand(String("EditStyleDefinitions"),       edit_style_definitions_cmd,          can_edit_style_definitions);
  Application::register_menucommand(String("ExpandSelection"),            expand_selection_cmd,                can_expand_selection);
  Application::register_menucommand(String("FindMatchingFence"),          find_matching_fence_cmd,             can_find_matching_fence);
  Application::register_menucommand(String("SelectAll"),                  select_all_cmd);
  
  Application::register_menucommand(String("SectionMerge"),               section_merge_cmd,                   can_section_merge);
  Application::register_menucommand(String("SectionSplit"),               section_split_cmd,                   can_section_split);
  
  Application::register_menucommand(String("DuplicatePreviousInput"),     duplicate_previous_input_output_cmd, can_duplicate_previous_input_output);
  Application::register_menucommand(String("DuplicatePreviousOutput"),    duplicate_previous_input_output_cmd, can_duplicate_previous_input_output);
  Application::register_menucommand(String("SimilarSectionBelow"),        similar_section_below_cmd,           can_similar_section_below);
  Application::register_menucommand(String("InsertColumn"),               insert_column_cmd,                   can_document_write);
  Application::register_menucommand(String("InsertFraction"),             insert_fraction_cmd,                 can_document_write);
  Application::register_menucommand(String("InsertOpposite"),             insert_opposite_cmd);
  Application::register_menucommand(String("InsertOverscript"),           insert_overscript_cmd,               can_document_write);
  Application::register_menucommand(String("InsertRadical"),              insert_radical_cmd,                  can_document_write);
  Application::register_menucommand(String("InsertRow"),                  insert_row_cmd,                      can_document_write);
  Application::register_menucommand(String("InsertSubscript"),            insert_subscript_cmd,                can_document_write);
  Application::register_menucommand(String("InsertSuperscript"),          insert_superscript_cmd,              can_document_write);
  Application::register_menucommand(String("InsertUnderscript"),          insert_underscript_cmd,              can_document_write);
  
  
  Application::register_menucommand(String("DynamicToLiteral"),           convert_dynamic_to_literal,          can_convert_dynamic_to_literal);
  Application::register_menucommand(String("EvaluatorAbort"),             abort_cmd,                           can_abort);
  Application::register_menucommand(String("EvaluateInPlace"),            evaluate_in_place_cmd,               can_evaluate_in_place);
  Application::register_menucommand(String("EvaluateSections"),           evaluate_sections_cmd,               can_evaluate_sections);
  Application::register_menucommand(String("EvaluateSectionsAndReturn"),  evaluate_sections_cmd);
  Application::register_menucommand(String("EvaluatorSubsession"),        evaluator_subsession_cmd,            can_abort);
  Application::register_menucommand(String("FindEvaluatingSection"),      find_evaluating_section,             can_find_evaluating_section);
  Application::register_menucommand(String("RemoveFromEvaluationQueue"),  remove_from_evaluation_queue,        can_remove_from_evaluation_queue);
  Application::register_menucommand(String("SubsessionEvaluateSections"), subsession_evaluate_sections_cmd,    can_subsession_evaluate_sections);
  
  Application::register_menucommand(Symbol(PMATH_SYMBOL_DOCUMENTAPPLY),  document_apply_cmd,  can_document_write);
  Application::register_menucommand(Symbol(PMATH_SYMBOL_DOCUMENTDELETE), document_delete_cmd, can_document_write);
  Application::register_menucommand(Symbol(PMATH_SYMBOL_DOCUMENTWRITE),  document_write_cmd,  can_document_write);
  
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

  BIND_DOWN(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED,  builtin_internal_dynamicupdated)
  
  BIND_DOWN(PMATH_SYMBOL_CURRENTVALUE,             builtin_currentvalue)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTAPPLY,            builtin_documentapply_or_documentwrite)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTDELETE,           builtin_documentdelete)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTGET,              builtin_documentget)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTREAD,             builtin_documentread)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTWRITE,            builtin_documentapply_or_documentwrite)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTSAVE,             builtin_documentsave)
  BIND_DOWN(PMATH_SYMBOL_EVALUATIONDOCUMENT,       builtin_evaluationdocument)
  BIND_DOWN(PMATH_SYMBOL_FRONTENDTOKENEXECUTE,     builtin_frontendtokenexecute)
  BIND_DOWN(PMATH_SYMBOL_SECTIONPRINT,             builtin_sectionprint)
  
  BIND_UP(PMATH_SYMBOL_CURRENTVALUE,               builtin_assign_currentvalue)
  BIND_UP(richmath_System_FrontEndObject,          builtin_feo_options)
  
  BIND_DOWN(richmath_FE_AddConfigShaper,     builtin_addconfigshaper)
  BIND_DOWN(richmath_FE_InternalExecuteFor,  builtin_internalexecutefor)
  BIND_DOWN(richmath_FE_CallFrontEnd,        builtin_callfrontend)
  BIND_DOWN(richmath_FE_FileOpenDialog,      builtin_filedialog)
  BIND_DOWN(richmath_FE_FileSaveDialog,      builtin_filedialog)
  
  Application::register_menucommand(Symbol(richmath_FE_CopySpecial),   copy_special_cmd, can_copy_cut);
  Application::register_menucommand(Symbol(PMATH_SYMBOL_RULE),         set_style_cmd,    can_set_style);
  Application::register_menucommand(Symbol(richmath_FE_ScopedCommand), do_scoped_cmd,    can_do_scoped);
  
  if(!richmath::impl::init_document_functions())
    goto FAIL;
  
  return true;
  
FAIL:
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           pmath_unref( SYM ); SYM = PMATH_NULL;
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL
  return false;
}

void richmath::done_bindings() {
  richmath::impl::done_document_functions();
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           pmath_unref( SYM ); SYM = PMATH_NULL;
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL
}
