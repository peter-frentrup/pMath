#include <eval/binding.h>

#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <boxes/textsequence.h>
#include <eval/application.h>
#include <eval/job.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

Hashtable<int, Void, cast_hash> richmath::all_document_ids;

static int current_document_id = 0;

//{ pmath functions ...

static pmath_t builtin_addconfigshaper(pmath_expr_t expr) {
  Expr data = Expr(
                pmath_evaluate(
                  pmath_expr_new_extended(
                    pmath_ref(PMATH_SYMBOL_GET), 1,
                    pmath_expr_get_item(expr, 1))));
                    
  pmath_unref(expr);
  
  Application::notify(CNT_ADDCONFIGSHAPER, data);
  return PMATH_NULL;
}

static pmath_t builtin_colordialog(pmath_expr_t _expr) {
  return Application::notify_wait(CNT_COLORDIALOG, Expr(_expr)).release();
}

static pmath_t builtin_filedialog(pmath_expr_t _expr) {
  return Application::notify_wait(CNT_FILEDIALOG, Expr(_expr)).release();
}

static pmath_t builtin_fontdialog(pmath_expr_t _expr) {
  return Application::notify_wait(CNT_FONTDIALOG, Expr(_expr)).release();
}

static pmath_t builtin_internalexecutefor(pmath_expr_t expr) {
  if(pmath_expr_length(expr) != 4)
    return expr;
    
  pmath_debug_print_object("[", expr, "]\n");
  
  pmath_t code        = pmath_expr_get_item(expr, 1);
  pmath_t document_id = pmath_expr_get_item(expr, 2);
  pmath_t section_id  = pmath_expr_get_item(expr, 3);
  pmath_t box_id      = pmath_expr_get_item(expr, 4);
  pmath_unref(expr);
  
  if( pmath_is_int32(document_id) &&
      pmath_is_int32(section_id) &&
      pmath_is_int32(box_id))
  {
    code = Application::internal_execute_for(
             Expr(code),
             PMATH_AS_INT32(document_id),
             PMATH_AS_INT32(section_id),
             PMATH_AS_INT32(box_id)).release();
  }
  else
    code = pmath_evaluate(code);
    
  pmath_unref(document_id);
  pmath_unref(section_id);
  pmath_unref(box_id);
  
  return code;
}

static pmath_t builtin_createdocument(pmath_expr_t expr) {
  return Application::notify_wait(CNT_CREATEDOCUMENT, Expr(expr)).release();
}

static pmath_t builtin_documentapply_or_documentwrite(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr.expr_length() != 2) {
    pmath_message_argxxx(expr.expr_length(), 2, 2);
    return expr.release();
  }
  
  Application::notify_wait(CNT_MENUCOMMAND, expr);
  
  return PMATH_NULL;
}

static pmath_t builtin_documentdelete(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  Application::notify_wait(CNT_MENUCOMMAND, expr);
  
  return PMATH_NULL;
}

static pmath_t builtin_documentget(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  return Application::notify_wait(CNT_DOCUMENTGET, expr).release();
  
  return PMATH_NULL;
}

static pmath_t builtin_documentread(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr.expr_length() > 1) {
    pmath_message_argxxx(expr.expr_length(), 0, 1);
    return expr.release();
  }
  
  return Application::notify_wait(CNT_DOCUMENTREAD, expr).release();
  
  return PMATH_NULL;
}

static pmath_t builtin_documents(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return Application::notify_wait(CNT_GETDOCUMENTS, Expr()).release();
}

static pmath_t builtin_currentvalue(pmath_expr_t expr) {
  return Application::notify_wait(CNT_CURRENTVALUE, Expr(expr)).release();
}

static pmath_t builtin_internal_dynamicupdated(pmath_expr_t expr) {
  Application::notify(CNT_DYNAMICUPDATE, Expr(expr));
  return PMATH_NULL;
}

static pmath_t builtin_feo_options(pmath_expr_t _expr) {
  Expr expr(_expr);
  
  if(expr[0] == PMATH_SYMBOL_OPTIONS) {
    if(expr.expr_length() == 1) {
      Expr opts = Application::notify_wait(CNT_GETOPTIONS, expr[1]);
      
      return opts.release();
    }
    
    if(expr.expr_length() == 2
        && expr[1][0] == PMATH_SYMBOL_FRONTENDOBJECT) {
      Expr opts = Application::notify_wait(CNT_GETOPTIONS, expr[1]);
      
      expr.set(1, opts);
    }
  }
  else if(expr[0] == PMATH_SYMBOL_SETOPTIONS) {
    if(expr.expr_length() >= 1
        && expr[1][0] == PMATH_SYMBOL_FRONTENDOBJECT) {
      Expr opts = Application::notify_wait(CNT_SETOPTIONS, expr);
      
      return opts.release();
    }
  }
  
  return expr.release();
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
             pmath_ref(PMATH_SYMBOL_SECTION), 3,
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_BOXDATA), 1,
               boxes),
             style,
             pmath_expr_new_extended(
               pmath_ref(PMATH_SYMBOL_RULE), 2,
               pmath_ref(PMATH_SYMBOL_SECTIONGENERATED),
               pmath_ref(PMATH_SYMBOL_TRUE)));
               
    Application::notify_wait(CNT_PRINTSECTION, Expr(expr));
    
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
  
  return Application::notify_wait(CNT_GETEVALUATIONDOCUMENT, Expr()).release();
}

static pmath_t builtin_selecteddocument(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return pmath_expr_new_extended(
           pmath_ref(PMATH_SYMBOL_FRONTENDOBJECT), 1,
           PMATH_FROM_INT32(current_document_id));
}

//} ... pmath functions

//{ menu command availability checkers ...

static bool can_abort(Expr cmd) {
  return !Application::is_idle();
}

static bool can_convert_dynamic_to_literal(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || doc->selection_length() == 0)
    return false;
    
  Box *sel = doc->selection_box();
  if(!sel || !sel->get_style(Editable))
    return false;
    
  return true;
}

static bool can_copy_cut(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || !doc->can_copy())
    return false;
    
  if(String(cmd).equals("Cut")) {
    Box *sel = doc->selection_box();
    return sel && sel->get_style(Editable);
  }
  
  return true;
}

static bool can_open_close_group(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || doc->selection_length() == 0)
    return false;
    
  return true;
}

static bool can_document_write(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  Box *sel = doc->selection_box();
  return sel && sel->get_style(Editable);
}

static bool can_duplicate_previous_input_output(Expr cmd) {
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
    MathSection *math = dynamic_cast<MathSection *>(doc->item(i));
    
    if(math &&
        (( input && math->get_style(Evaluatable)) ||
         (!input && math->get_style(SectionGenerated))))
    {
      return true;
    }
  }
  
  return false;
}

static bool can_edit_boxes(Expr cmd) {
  Document *doc = get_current_document();
  
  return doc && (doc->selection_length() > 0 || doc->selection_box() != doc) && doc->get_style(Editable);
}

static bool can_expand_selection(Expr cmd) {
  Document *doc = get_current_document();
  
  return doc && doc->selection_box() && doc->selection_box() != doc;
}

static bool can_evaluate_in_place(Expr cmd) {
  Document *doc = get_current_document();
  
  return doc && 0 != dynamic_cast<MathSequence *>(doc->selection_box());
}

static bool can_evaluate_sections(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  Box *box = doc->selection_box();
  
  if(box == doc) {
    for(int i = doc->selection_start(); i < doc->selection_end(); ++i) {
      MathSection *math = dynamic_cast<MathSection *>(doc->item(i));
      
      if(math && math->get_style(Evaluatable))
        return true;
    }
  }
  else {
    while(box && !dynamic_cast<MathSection *>(box))
      box = box->parent();
      
    MathSection *math = dynamic_cast<MathSection *>(box);
    if(math && math->get_style(Evaluatable))
      return true;
  }
  
  return false;
}

static bool can_find_evaluating_section(Expr cmd) {
  Box *box = Application::find_current_job();
  
  if(!box)
    return false;
    
  Section *sect = box->find_parent<Section>(true);
  if(!sect)
    return false;
    
  Document *doc = sect->find_parent<Document>(false);
  if(!doc)
    return false;
    
  return true;
}

static bool can_find_matching_fence(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(doc->selection_box());
  
  if(seq) {
    int pos = doc->selection_end() - 1;
    int match = seq->matching_fence(pos);
    
    if(match < 0 && doc->selection_start() == doc->selection_end()) {
      ++pos;
      match = seq->matching_fence(pos);
    }
    
    return match >= 0;
  }
  
  return false;
}

static bool can_remove_from_evaluation_queue(Expr cmd) {
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
  
  if(!box || start >= end)
    return false;
    
  for(int i = end - 1; i >= start; --i) {
    if(Application::remove_job(doc->section(i), true))
      return true;
  }
  
  return false;
}

static bool can_section_merge(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  return doc->merge_sections(false);
}

static bool can_section_split(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  return doc->split_section(false);
}

static bool can_similar_section_below(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || !doc->get_style(Editable))
    return false;
    
  Box *box = doc->selection_box();
  while(box && box->parent() != doc) {
    box = box->parent();
  }
  
  return 0 != dynamic_cast<AbstractSequenceSection *>(box);
}

static bool can_subsession_evaluate_sections(Expr cmd) {
  return !can_abort(Expr()) && can_evaluate_sections(Expr());
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

static bool cut_cmd(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc || !doc->can_copy())
    return false;
    
  doc->cut_to_clipboard();
  return true;
}

static bool document_apply_cmd(Expr cmd) {
  Document *doc = dynamic_cast<Document *>(Box::find(cmd[1]));
  
  if(!doc)
    return false;
    
  AbstractSequence *seq;
  
  if(dynamic_cast<TextSequence *>(doc->selection_box()))
    seq = new TextSequence;
  else
    seq = new MathSequence;
    
  seq->load_from_object(cmd[2], BoxOptionDefault);
  doc->insert_box(seq, true);
  
  return true;
}

static bool document_delete_cmd(Expr cmd) {
  Document *doc;
  
  if(cmd.expr_length() == 0)
    doc = get_current_document();
  else
    doc = dynamic_cast<Document *>(Box::find(cmd[1]));
    
  if(!doc)
    return false;
    
  doc->remove_selection();
  
  return true;
}

static bool document_write_cmd(Expr cmd) {
  Document *doc = dynamic_cast<Document *>(Box::find(cmd[1]));
  
  if(!doc)
    return false;
    
  AbstractSequence *seq;
  
  if(dynamic_cast<TextSequence *>(doc->selection_box()))
    seq = new TextSequence;
  else
    seq = new MathSequence;
    
  seq->load_from_object(cmd[2], BoxOptionDefault);
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
    MathSection *math = dynamic_cast<MathSection *>(doc->item(i));
    
    if( math &&
        (( input && math->get_style(Evaluatable)) ||
         (!input && math->get_style(SectionGenerated))))
    {
      MathSequence *seq = new MathSequence;
      seq->load_from_object(Expr(math->content()->to_pmath(BoxFlagDefault)), 0);
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
    doc->select(0, 0, 0);
    
    for(int i = a; i < b; ++i) {
      EditSection *edit = dynamic_cast<EditSection *>(doc->section(i));
      pmath_continue_after_abort();
      
      if(edit) {
        Expr parsed(edit->to_pmath(BoxFlagDefault));
        
        if(parsed == 0) {
          doc->native()->beep();//MessageBeep(MB_ICONEXCLAMATION);
        }
        else {
          Section *sect = Section::create_from_object(parsed);
          sect->swap_id(edit);
          
          delete doc->swap(i, sect);
        }
      }
      else {
        Section *sect = doc->section(i);
        edit = new EditSection;
        if(!edit->style)
          edit->style = new Style;
        edit->style->set(SectionGroupPrecedence, sect->get_style(SectionGroupPrecedence));
        edit->swap_id(sect);
        edit->original = doc->swap(i, edit);
        
        Expr obj(sect->to_pmath(BoxFlagDefault));
        
        doc->select(edit->content(), 0, 0);
        doc->insert_string(obj.to_string(
                             PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR));
      }
    }
    
    doc->select_range(box, a, a, box, b, b);
  }
  
  return true;
}

static bool evaluate_in_place_cmd(Expr cmd) {
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
    
  MathSequence *seq = dynamic_cast<MathSequence *>(doc->selection_box());
  
  if(seq) {
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
    for(int i = doc->selection_start(); i < doc->selection_end(); ++i) {
      MathSection *math = dynamic_cast<MathSection *>(doc->item(i));
      
      if(math && math->get_style(Evaluatable))
        Application::add_job(new InputJob(math));
      else
        return false;
    }
  }
  else {
    while(box && !dynamic_cast<MathSection *>(box))
      box = box->parent();
      
    MathSection *math = dynamic_cast<MathSection *>(box);
    if(math && math->get_style(Evaluatable)) {
      Application::add_job(new InputJob(math));
    }
    else {
      if(dynamic_cast<AbstractSequence *>(doc->selection_box()))
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
    
  // non-blocking interrupt
  Application::execute_for(Call(Symbol(PMATH_SYMBOL_DIALOG)), 0, Infinity);
  
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
  
  Section *sect = box->find_parent<Section>(true);
  if(!sect) {
    if(current_doc)
      current_doc->native()->beep();
    return false;
  }
  
  Document *doc = sect->find_parent<Document>(false);
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
    
  MathSequence *seq = dynamic_cast<MathSequence *>(doc->selection_box());
  
  if(seq) {
    int pos = doc->selection_end() - 1;
    int match = seq->matching_fence(pos);
    
    if(match < 0 && doc->selection_start() == doc->selection_end()) {
      ++pos;
      match = seq->matching_fence(pos);
    }
    
    if(match >= 0) {
      doc->select(seq, match, match + 1);
    }
  }
  
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
  Application::notify(
    CNT_CREATEDOCUMENT,
    Call(Symbol(PMATH_SYMBOL_CREATEDOCUMENT), List()));
    
  return true;
}

static bool open_cmd(Expr cmd) {
  Expr filter = List(
                  Rule(String("pMath Documents (*.pmathdoc)"), String("*.pmathdoc")),
                  Rule(String("All Files (*.*)"),              String("*.*")));
                  
  Expr filenames = Application::run_filedialog(
                     Call(
                       GetSymbol(FileOpenDialog),
                       filter));
                       
  if(filenames.is_string())
    filenames = List(filenames);
    
  if(filenames[0] != PMATH_SYMBOL_LIST)
    return false;
    
  for(size_t i = 1; i <= filenames.expr_length(); ++i) {
    Document *doc = Application::create_document();
    if(!doc)
      continue;
      
    String filename(filenames[i]);
    if(filename.part(filename.length() - 9).equals(".pmathdoc")) {
      Expr held_boxes = Application::interrupt(
                          Parse("Get(`1`, Head->HoldComplete)", filename),
                          Application::button_timeout);
                          
                          
      if( held_boxes.expr_length() == 1 &&
          held_boxes[0] == PMATH_SYMBOL_HOLDCOMPLETE &&
          doc->try_load_from_object(held_boxes[1], BoxOptionDefault))
      {
      
        if(doc->selectable())
          set_current_document(doc);
        else
          doc->select(0, 0, 0);
          
        doc->invalidate_options();
        
        continue;
      }
    }
    
    set_current_document(doc);
    
    ReadableTextFile file(Evaluate(Call(Symbol(PMATH_SYMBOL_OPENREAD), filename)));
    String s;
    
    while(!pmath_aborting() && file.status() == PMATH_FILE_OK) {
      if(s.is_valid())
        s += "\n";
      s += file.readline();
    }
    
    int pos = 0;
    Expr section_expr = Call(Symbol(PMATH_SYMBOL_SECTION), s, String("Text"));
    doc->insert_pmath(&pos, section_expr);
    
    int c = filename.length();
    const uint16_t *buf = filename.buffer();
    while(c >= 0 && buf[c] != '\\' && buf[c] != '/')
      --c;
      
    doc->style->set(WindowTitle, filename.part(c + 1));
    doc->style->set(InternalHasModifiedWindowOption, true);
    doc->invalidate_options();
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
    doc->move_horizontal(Forward, false);
    return true;
  }
  
  doc->native()->beep();
  return false;
}

static bool subsession_evaluate_sections_cmd(Expr cmd) {
  // non-blocking interrupt
  Application::execute_for(
    Call(Symbol(PMATH_SYMBOL_DIALOG),
         Call(Symbol(PMATH_SYMBOL_FRONTENDTOKENEXECUTE),
              String("EvaluateSectionsAndReturn"))),
    0, Infinity);
    
  return false;
}

//} ... menu commands

static pmath_symbol_t fe_symbols[FrontEndSymbolsCount];

bool richmath::init_bindings() {
#define BIND_DOWN(SYMBOL, FUNC)  pmath_register_code(SYMBOL, FUNC, PMATH_CODE_USAGE_DOWNCALL)
#define BIND_UP(  SYMBOL, FUNC)  pmath_register_code(SYMBOL, FUNC, PMATH_CODE_USAGE_UPCALL)

  Application::register_menucommand(String("New"),                        new_cmd);
  Application::register_menucommand(String("Open"),                       open_cmd);
  Application::register_menucommand(String("Close"),                      close_cmd);
  
  Application::register_menucommand(String("Copy"),                       copy_cmd,                            can_copy_cut);
  Application::register_menucommand(String("Cut"),                        cut_cmd,                             can_copy_cut);
  Application::register_menucommand(String("OpenCloseGroup"),             open_close_group_cmd,                can_open_close_group);
  Application::register_menucommand(String("Paste"),                      paste_cmd,                           can_document_write);
  Application::register_menucommand(String("EditBoxes"),                  edit_boxes_cmd,                      can_edit_boxes);
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
  
#define VERIFY(x)  if(0 == (x)) goto FAIL_SYMBOLS;
#define NEW_SYMBOL(name)     pmath_symbol_get(PMATH_C_STRING(name), TRUE)
  
  memset(fe_symbols, 0, sizeof(fe_symbols));
  VERIFY(fe_symbols[NumberBoxSymbol]          = NEW_SYMBOL("FE`NumberBox"))
  VERIFY(fe_symbols[SymbolInfoSymbol]         = NEW_SYMBOL("FE`SymbolInfo"))
  VERIFY(fe_symbols[AddConfigShaperSymbol]    = NEW_SYMBOL("FE`AddConfigShaper"))
  VERIFY(fe_symbols[DelimiterSymbol]          = NEW_SYMBOL("FE`Delimiter"))
  VERIFY(fe_symbols[ItemSymbol]               = NEW_SYMBOL("FE`Item"))
  VERIFY(fe_symbols[KeyEventSymbol]           = NEW_SYMBOL("FE`KeyEvent"))
  VERIFY(fe_symbols[KeyAltSymbol]             = NEW_SYMBOL("FE`KeyAlt"))
  VERIFY(fe_symbols[KeyControlSymbol]         = NEW_SYMBOL("FE`KeyControl"))
  VERIFY(fe_symbols[KeyShiftSymbol]           = NEW_SYMBOL("FE`KeyShift"))
  VERIFY(fe_symbols[MenuSymbol]               = NEW_SYMBOL("FE`Menu"))
  VERIFY(fe_symbols[InternalExecuteForSymbol] = NEW_SYMBOL("FE`InternalExecuteFor"))
  VERIFY(fe_symbols[SymbolDefinitionsSymbol]  = NEW_SYMBOL("FE`SymbolDefinitions"))
  VERIFY(fe_symbols[FileOpenDialog]           = NEW_SYMBOL("FE`FileOpenDialog"))
  VERIFY(fe_symbols[FileSaveDialog]           = NEW_SYMBOL("FE`FileSaveDialog"))
  VERIFY(fe_symbols[ColorDialog]              = NEW_SYMBOL("FE`ColorDialog"))
  VERIFY(fe_symbols[FontDialog]               = NEW_SYMBOL("FE`FontDialog"))
  
  VERIFY(BIND_DOWN(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED,  builtin_internal_dynamicupdated))
  
  VERIFY(BIND_DOWN(PMATH_SYMBOL_CREATEDOCUMENT,           builtin_createdocument))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_CURRENTVALUE,             builtin_currentvalue))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTAPPLY,            builtin_documentapply_or_documentwrite))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTDELETE,           builtin_documentdelete))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTGET,              builtin_documentget))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTREAD,             builtin_documentread))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTWRITE,            builtin_documentapply_or_documentwrite))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTS,                builtin_documents))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_EVALUATIONDOCUMENT,       builtin_evaluationdocument))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_FRONTENDTOKENEXECUTE,     builtin_frontendtokenexecute))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_SECTIONPRINT,             builtin_sectionprint))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_SELECTEDDOCUMENT,         builtin_selecteddocument))
  
  VERIFY(BIND_UP(PMATH_SYMBOL_FRONTENDOBJECT,             builtin_feo_options))
  
  VERIFY(BIND_DOWN(fe_symbols[AddConfigShaperSymbol],     builtin_addconfigshaper))
  VERIFY(BIND_DOWN(fe_symbols[InternalExecuteForSymbol],  builtin_internalexecutefor))
  VERIFY(BIND_DOWN(fe_symbols[ColorDialog],               builtin_colordialog))
  VERIFY(BIND_DOWN(fe_symbols[FileOpenDialog],            builtin_filedialog))
  VERIFY(BIND_DOWN(fe_symbols[FileSaveDialog],            builtin_filedialog))
  VERIFY(BIND_DOWN(fe_symbols[FontDialog],                builtin_fontdialog))
  
  pmath_symbol_set_attributes(
    fe_symbols[InternalExecuteForSymbol],
    pmath_symbol_get_attributes(
      fe_symbols[InternalExecuteForSymbol]) | PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST);
      
  return true;
  
FAIL_SYMBOLS:
  for(size_t i = 0; i < FrontEndSymbolsCount; ++i)
    pmath_unref(fe_symbols[i]);
    
  memset(fe_symbols, 0, sizeof(fe_symbols));
  return false;
}

void richmath::done_bindings() {
  for(size_t i = 0; i < FrontEndSymbolsCount; ++i)
    pmath_unref(fe_symbols[i]);
    
  memset(fe_symbols, 0, sizeof(fe_symbols));
}

Expr richmath::GetSymbol(FrontEndSymbolIndex i) {
  if((size_t)i >= (size_t)FrontEndSymbolsCount)
    return Expr();
    
  return Symbol(fe_symbols[(size_t)i]);
}

void richmath::set_current_document(Document *document) {
  Document *old = get_current_document();
  
  if(old)
    old->focus_killed();
    
  if(document)
    document->focus_set();
    
  current_document_id = document ? document->id() : 0;
}

Document *richmath::get_current_document() {
  return dynamic_cast<Document *>(Box::find(current_document_id));
}



