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

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>


using namespace richmath;

Hashtable<FrontEndReference, Void> richmath::all_document_ids;

static FrontEndReference current_document_id = FrontEndReference::None;

//{ pmath functions ...

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_FrontEndObject;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_System_SectionGenerated;

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

static pmath_t builtin_colordialog(pmath_expr_t _expr) {
  return Application::notify_wait(ClientNotification::ColorDialog, Expr(_expr)).release();
}

static pmath_t builtin_filedialog(pmath_expr_t _expr) {
  return Application::notify_wait(ClientNotification::FileDialog, Expr(_expr)).release();
}

static pmath_t builtin_fontdialog(pmath_expr_t _expr) {
  return Application::notify_wait(ClientNotification::FontDialog, Expr(_expr)).release();
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

static pmath_t builtin_createdocument(pmath_expr_t expr) {
  return Application::notify_wait(ClientNotification::CreateDocument, Expr(expr)).release();
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

static pmath_t builtin_documents(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return Application::notify_wait(ClientNotification::GetDocuments, Expr()).release();
}

static pmath_t builtin_documentsave(pmath_expr_t _expr) {
  if(pmath_expr_length(_expr) > 2) {
    pmath_message_argxxx(pmath_expr_length(_expr), 0, 2);
    return _expr;
  }
  
  return Application::notify_wait(ClientNotification::Save, Expr(_expr)).release();
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

static pmath_t builtin_selecteddocument(pmath_expr_t expr) {
  if(pmath_expr_length(expr) > 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return current_document_id.to_pmath().release();
}

//} ... pmath functions

//{ menu command availability checkers ...

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
      
  if(sel && cmd.is_rule()) {
    int start = doc->selection_start();
    int end   = doc->selection_end();
    
    Expr val;
    
    if(start < end) {
      if(sel == doc) {
        status.checked = true;
        
        for(int i = start; i < end; ++i) {
          val = sel->item(i)->get_pmath_style(lhs_key);
          status.checked = val == rhs;
          if(!status.checked)
            break;
        }
        
        return status;
      }
    }
    
    val = sel->get_pmath_style(lhs_key);
    status.checked = val == rhs;
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

namespace {
  class TrackedSelection {
    public:
      TrackedSelection() = default;
      
      TrackedSelection(FrontEndReference sel_box, int sel_index) 
        : selection_box{sel_box},
          selection_index{sel_index},
          token_output_start{-1},
          token_output_depth{-1},
          output_pos{-1},
          token_source_start{-1},
          token_source_end{-1}
      {
      }
      
      void pre_write(pmath_t obj, const SelectionReference &source, const String &output, int call_depth) {
        if(source.id == selection_box) {
          if(source.start <= selection_index && selection_index <= source.end) {
            int written = output.length();
            if(token_output_start <= written) {
              token_output_start = written;
              token_output_depth = call_depth;
              token              = Expr{ pmath_ref(obj) };
              token_source_start = source.start;
              token_source_end   = source.end;
            }
          }
        }
        else if(pmath_is_string(obj)) {
          Box *box_at_source = find_box_at(source);
          
          if(NumberBox *num = dynamic_cast<NumberBox*>(box_at_source)) {
            if(num->is_number_part(FrontEndObject::find_cast<Box>(selection_box))) {
              int written = output.length();
              if(token_output_start <= written) {
                token_output_start = written;
                token_output_depth = call_depth;
                token              = Expr{ pmath_ref(obj) };
                token_source_start = source.start;
                token_source_end   = source.end;
              }
            }
          }
        }
      }
      
      void post_write(pmath_t obj, const SelectionReference &source, const String &output, int call_depth) {
        if(call_depth != token_output_depth) 
          return;
          
        token_output_depth = -1;
        int token_output_end = output.length();
        if(token_output_end < token_output_start)
          return;
        
        const uint16_t *buf = output.buffer();
        
        int out_tok_len = token_output_end - token_output_start;
        int in_tok_len  = token_source_end - token_source_start;
        
        const uint16_t *in16 = nullptr;
        const char     *in8 = nullptr;
        int in_length = 0;
        int in_start = token_source_start;
        int in_index = selection_index;
        
        FrontEndObject *selfeo = FrontEndObject::find(selection_box);
        if(source.id == selection_box) {
          if(auto mseq = dynamic_cast<MathSequence*>(selfeo)) {
            in16 = mseq->text().buffer();
            in_length = mseq->length();
          }
          else if(auto tseq = dynamic_cast<TextSequence*>(selfeo)) {
            in8 = tseq->text_buffer().buffer();
            in_length = tseq->length();
          }
          else {
            output_pos = token_output_start;
            return;
          }
        }
        else if(pmath_is_string(obj)){
          Box *selbox = dynamic_cast<Box*>(selfeo);
          
          Box *box_at_source = find_box_at(source);
          
          if(NumberBox *num = dynamic_cast<NumberBox*>(box_at_source)) {
            PositionInRange pos = num->selection_to_string_index(String{pmath_ref(obj)}, selbox, selection_index);
            if(pos.pos >= 0) {
              pos.pos = std::max(pos.start, std::min(pos.pos, pos.end));
            }
            if(pos.pos <= out_tok_len) {
              //output_pos = token_output_start + pos.pos;
              //return;
              in16 = pmath_string_buffer((pmath_string_t*)&obj);
              in_length = pmath_string_length((pmath_string_t)obj);
              in_start = 0;
              in_index = pos.pos;
            }
            else if(num->is_number_part(selbox)) {
              output_pos = token_output_start;
              return;
            }
          }
        }
        
        if( out_tok_len >= in_tok_len + 2 && 
            in_index <= in_length &&
            buf[token_output_start] == '"' &&
            buf[token_output_end - 1] == '"') 
        {
          int opos = token_output_start + 1;
          int ipos = in_start;
          while(ipos < in_index && opos < token_output_end - 1) {
            if(in8) {
              if(in8[ipos]) {
                const char *next = g_utf8_find_next_char(in8 + ipos, in8 + in_index);
                if(next)
                  ipos = (int)(next - in8);
                else
                  ipos = in_index;
              }
              else
                ++ipos; // embedded <NUL>
            }
            else if(in16) {
              if( is_utf16_high(in16[ipos]) && 
                  ipos + 1 < in_index &&
                  is_utf16_low(in16[ipos + 1])) 
              {
                ipos+= 2;
              }
              else
                ++ipos;
            }
            else
              break;
            
            if(buf[opos] == '\\') {
              ++opos;
              if(opos < token_output_end - 1 && buf[opos] == '[') {
                while(opos < token_output_end - 1 && buf[opos] != ']')
                  ++opos;
                
                ++opos;
              }
              else
                ++opos;
            }
            else if(is_utf16_high(buf[opos]) && 
                opos + 1 < token_output_end && 
                is_utf16_low(buf[opos + 1]))
            {
              opos+= 2;
            }
            else
              ++opos;
          }
          
          output_pos = opos;
        }
      }
      
    private:
      static Box *find_box_at(const SelectionReference &source) {
        FrontEndObject *feo = FrontEndObject::find(source.id);
        if(!feo)
          return nullptr;
        
        if(MathSequence *mseq = dynamic_cast<MathSequence*>(feo)) {
          if(source.end != source.start + 1)
            return nullptr;
          if(source.start < 0 || source.start >= mseq->length())
            return nullptr;
          
          if(mseq->char_at(source.start) != PMATH_CHAR_BOX)
            return nullptr;
          
          return raw_find_box_at(mseq, source.start);
        }
        else if(TextSequence *tseq = dynamic_cast<TextSequence*>(feo)) {
          if(!tseq->text_buffer().is_box_at(source.start, source.end)) 
            return nullptr;
          
          return raw_find_box_at(tseq, source.start);
        }
        else if(Box *box = dynamic_cast<Box*>(feo)) {
          if(source.start == 0 && source.end == box->length())
            return box;
        }
        
        return nullptr;
      }
      
      static Box *raw_find_box_at(Box *parent, int index) {
        for(int i = parent->count(); i > 0; --i) {
          Box *box = parent->item(i - 1);
          int box_index = box->index();
          if(box_index == index)
            return box;
          if(box_index < index)
            return nullptr;
        }
        return nullptr;
      }
    
    public:
      FrontEndReference selection_box;
      int               selection_index;
      
      Expr  token;
      int   token_output_start;
      int   token_output_depth;
      int   output_pos;
      int   token_source_start;
      int   token_source_end;
  };
  
  class PrintTracking {
    public:
      PrintTracking() 
        : output{""},
          call_depth{0}
      {
      }
      
      String write(Expr obj, pmath_write_options_t options) {
        pmath_write_ex_t info = {0};
        info.size = sizeof(info);
        info.options = PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR;
        info.user = this;
        info.write = write_callback;
        info.pre_write = pre_write_callback;
        info.post_write = post_write_callback;
        
        output = String("");
        pmath_write_ex(&info, obj.get());
        return output;
      }
      
    private:
      static void write_callback(void *_self, const uint16_t *data, int len) {
        PrintTracking *self = (PrintTracking*)_self;
        self->write(data, len);
      };
      
      static void pre_write_callback(void *_self, pmath_t obj, pmath_write_options_t opts) {
        PrintTracking *self = (PrintTracking*)_self;
        self->pre_write(obj, opts);
      };
      
      static void post_write_callback(void *_self, pmath_t obj, pmath_write_options_t opts) {
        PrintTracking *self = (PrintTracking*)_self;
        self->post_write(obj, opts);
      };
    
    private:
      void write(const uint16_t *data, int len) {
        output.insert(INT_MAX, data, len);
      }
      
      void pre_write(pmath_t obj, pmath_write_options_t opts) {
        ++call_depth;
        
        SelectionReference source = SelectionReference::from_debug_info_of(obj);
        if(source) {
          for(auto &sel : selections)
            sel.pre_write(obj, source, output, call_depth);
        }
      }
      
      void post_write(pmath_t obj, pmath_write_options_t opts) {
        SelectionReference source = SelectionReference::from_debug_info_of(obj);
        if(source) {
          for(auto &sel : selections)
            sel.post_write(obj, source, output, call_depth);
        }
        
        --call_depth;
      }
    
    public:
      String                  output;
      Array<TrackedSelection> selections;
    
    private:
      int                     call_depth;
  };
  
  class TrackedBoxSelection {
    public:
      TrackedBoxSelection() = default;
      
      TrackedBoxSelection(FrontEndReference id, int index) 
        : source_id{id},
          source_index{index}
      {
      }
      
      void after_creation(Box *box, const SelectionReference &source, Expr expr) {
        if(source.id != source_id)
          return;
        
        if(destination)
          return;
        
        if(source_index < source.start || source.end < source_index)
          return;
        
        if(auto seq = dynamic_cast<MathSequence*>(box)) {
          int len = seq->length();
          if(len == 0) {
            destination.set(seq, 0, 0);
            return;
          }
          
          SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
          if(se->end() + 1 != len) {
            if(expr.is_expr() && expr[0] == PMATH_NULL) {
              for(size_t i = 1; ;++i) {
                Expr item = expr[i];
                SelectionReference item_source = SelectionReference::from_debug_info_of(item);
                if(source_index <= item_source.end && item_source.id == source_id) {
                  visit_span(se, item_source, item);
                  delete se;
                  return;
                }
                
                int pos = se->end() + 1;
                if(pos < len) {
                  delete se;
                  se = new SpanExpr(pos, seq->span_array()[pos], seq);
                }
                else
                  break;
              }
            }
          }
          visit_span(se, source, expr);
          delete se;
        }
      }
    
    private:
      void visit_span(SpanExpr *se, const SelectionReference &source, Expr expr) {
        if(source_index <= source.start) {
          destination.set(se->sequence(), se->start(), se->start());
          return;
        }
        else if(source_index >= source.end) {
          destination.set(se->sequence(), se->end() + 1, se->end() + 1);
          return;
        }
        
        size_t expr_len = expr.expr_length();
        int count = se->count();
        
        if(expr_len > 0 && (size_t)count == expr_len && (expr[0] == PMATH_NULL || expr[0] == PMATH_SYMBOL_LIST)) {
          for(int i = 0; i < count; ++i) {
            Expr item = expr[(size_t)i + 1];
            SelectionReference item_source = SelectionReference::from_debug_info_of(item);
            
            if(source_index <= item_source.end && item_source.id == source_id) {
              visit_span(se->item(i), item_source, item);
              return;
            }
          }
          
          destination.set(se->sequence(), se->end() + 1, se->end() + 1);
          return;
        }
        
        if(count == 0 && expr.is_string()) {
          if(se->as_text() == expr) {
            if(auto source_seq = FrontEndObject::find_cast<MathSequence>(source.id)) {
              if(0 <= source.start && source.start <= source.end && source.end <= source_seq->length()) {
                const uint16_t *buf = source_seq->text().buffer();
                
                if(source.end - source.start >= 2 && buf[source.start] == '"' && buf[source.end-1] == '"') {
                  int in_pos = source.start + 1;
                  int o_pos = se->start();
                  int o_pos_max = se->end();
                  while(o_pos <= o_pos_max && in_pos < source_index) {
                    int in_next = in_pos;
                    if(buf[in_next] == '\\' && in_next + 1 < source.end) {
                      ++in_next;
                      if(buf[in_next] == '[') {
                        while(in_next < source.end && buf[in_next] != ']')
                          ++in_next;
                      }
                      else
                        ++in_next;
                    }
                    else
                      ++in_next;
                    
                    if(source_index < in_next) {
                      destination.set(se->sequence(), o_pos, o_pos + 1);
                      return;
                    }
                    in_pos = in_next;
                    ++o_pos;
                  }
                  
                  destination.set(se->sequence(), o_pos, o_pos);
                  return;
                }
              }
            }
          }
        }
        
        destination.set(se->sequence(), se->start(), se->end() + 1);
      }
    
    public:
      FrontEndReference source_id;
      int               source_index;
      
      SelectionReference destination;
  };
  
  class BoxTracking {
    public:
      BoxTracking() 
        : callback {
            [this](Box *box, Expr expr){ after_creation(box, std::move(expr)); }, 
            Box::on_finish_load_from_object}
      {
      }
      
      template<class F>
      void for_new_boxes_in(F body) {
        callback.next = Box::on_finish_load_from_object;
        Box::on_finish_load_from_object = &callback;
        
        body();
        
        Box::on_finish_load_from_object = callback.next;
      }
    
    private:
      void after_creation(Box *box, Expr expr) {
        auto source = SelectionReference::from_debug_info_of(expr);
        if(source)
          for(auto &sel : selections)
            sel.after_creation(box, source, expr);
      }
      
    public:
      Array<TrackedBoxSelection> selections;
    
    private:
      FunctionChain<Box*, Expr> callback;
  };
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
    
    SelectionReference final_selection_1;
    SelectionReference final_selection_2;
    
    final_selection_1.set(box, a, a);
    final_selection_2.set(box, b, b);
    
    for(int i = a; i < b; ++i) {
      pmath_continue_after_abort();
      
      if(auto edit = dynamic_cast<EditSection *>(doc->section(i))) {
        Expr parsed(edit->to_pmath(BoxOutputFlags::WithDebugInfo));
        
        if(parsed.is_null()) {
          doc->native()->beep();//MessageBeep(MB_ICONEXCLAMATION);
        }
        else {
          BoxTracking bt;
          
          bt.selections.add(TrackedBoxSelection{old_sel.id, old_sel.start});
          bt.selections.add(TrackedBoxSelection{old_sel.id, old_sel.end});
          
          Section *sect = nullptr;
          bt.for_new_boxes_in([&] {
            sect = Section::create_from_object(parsed);
          });
          
          sect->swap_id(edit);
          
          doc->swap(i, sect)->safe_destroy();
          
          if(bt.selections[0].destination && bt.selections[1].destination) {
            final_selection_1 = bt.selections[0].destination;
            final_selection_2 = bt.selections[1].destination;
          }
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
        
        Expr obj(sect->to_pmath(BoxOutputFlags::WithDebugInfo));
        
        Expr tmp = Call(Symbol(PMATH_SYMBOL_FULLFORM), obj);
        pmath_debug_print_object("\n fullform: ", tmp.get(), "\n");
        
        PrintTracking pt;
        pt.selections.add(TrackedSelection{old_sel.id, old_sel.start});
        pt.selections.add(TrackedSelection{old_sel.id, old_sel.end});
        pt.write(obj, PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR);
        
        doc->select(edit->content(), 0, 0);
        doc->insert_string(pt.output, false);
        
        int out_sel_start = pt.selections[0].output_pos;
        int out_sel_end   = pt.selections[1].output_pos;
        if(out_sel_start >= 0 && out_sel_end >= 0) {
          final_selection_1.set(edit->content(), out_sel_start, out_sel_start);
          final_selection_2.set(edit->content(), out_sel_end, out_sel_end);
        }
      }
    }
    
    doc->select_range(
      final_selection_1.get(), final_selection_1.start, final_selection_1.end, 
      final_selection_2.get(), final_selection_2.start, final_selection_2.end);
  }
  
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
    Document *doc = Application::create_document();
    if(!doc)
      continue;
      
    String filename(filenames[i]);
    doc->native()->filename(filename);
    
    if(filename.part(filename.length() - 9).equals(".pmathdoc")) {
      Expr held_boxes = Application::interrupt_wait(
                          Parse("Get(`1`, Head->HoldComplete)", filename),
                          Application::button_timeout);
                          
                          
      if( held_boxes.expr_length() == 1 &&
          held_boxes[0] == PMATH_SYMBOL_HOLDCOMPLETE &&
          doc->try_load_from_object(held_boxes[1], BoxInputFlags::Default))
      {
        if(!doc->selectable())
          doc->select(nullptr, 0, 0);
          
        doc->style->set(Visible,                         true);
        doc->style->set(InternalHasModifiedWindowOption, true);
        doc->invalidate_options();
        doc->native()->bring_to_front();
        continue;
      }
    }
    
    ReadableTextFile file(Evaluate(Call(Symbol(PMATH_SYMBOL_OPENREAD), filename)));
    String s;
    
    while(!pmath_aborting() && file.status() == PMATH_FILE_OK) {
      if(s.is_valid())
        s += "\n";
      s += file.readline();
    }
    
    int pos = 0;
    Expr section_expr = Call(Symbol(richmath_System_Section), s, String("Text"));
    doc->insert_pmath(&pos, section_expr);
    
    doc->style->set(Visible,                         true);
    doc->style->set(InternalHasModifiedWindowOption, true);
    doc->invalidate_options();
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
  Application::notify_wait(ClientNotification::Save, List(Symbol(PMATH_SYMBOL_AUTOMATIC), Symbol(PMATH_SYMBOL_AUTOMATIC)));
  
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
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL

bool richmath::init_bindings() {
  Application::register_menucommand(String("New"),                        new_cmd);
  Application::register_menucommand(String("Open"),                       open_cmd);
  Application::register_menucommand(String("Save"),                       save_cmd);
  Application::register_menucommand(String("SaveAs"),                     saveas_cmd);
  Application::register_menucommand(String("Close"),                      close_cmd);
  
  Application::register_menucommand(String("Copy"),                       copy_cmd,                            can_copy_cut);
  Application::register_menucommand(String("Cut"),                        cut_cmd,                             can_copy_cut);
  Application::register_menucommand(String("OpenCloseGroup"),             open_close_group_cmd,                can_open_close_group);
  Application::register_menucommand(String("Paste"),                      paste_cmd,                           can_document_write);
  Application::register_menucommand(String("GraphicsOriginalSize"),       graphics_original_size_cmd,          can_graphics_original_size);
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
  
#define VERIFY(X)  do{ pmath_t tmp = (X); if(pmath_is_null(tmp)) goto FAIL; }while(0);
#define NEW_SYMBOL(name)     pmath_symbol_get(PMATH_C_STRING(name), TRUE)
  
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           VERIFY( SYM = NEW_SYMBOL(NAME) )
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  pmath_symbol_set_attributes( (SYM), (ATTR) );
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL

#define BIND(SYMBOL, FUNC, USE)  if(!pmath_register_code((SYMBOL), (FUNC), (USE))) goto FAIL;
#define BIND_DOWN(SYMBOL, FUNC)   BIND((SYMBOL), (FUNC), PMATH_CODE_USAGE_DOWNCALL)
#define BIND_UP(SYMBOL, FUNC)     BIND((SYMBOL), (FUNC), PMATH_CODE_USAGE_UPCALL)
  
  BIND_DOWN(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED,  builtin_internal_dynamicupdated)
  
  BIND_DOWN(PMATH_SYMBOL_CREATEDOCUMENT,           builtin_createdocument)
  BIND_DOWN(PMATH_SYMBOL_CURRENTVALUE,             builtin_currentvalue)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTAPPLY,            builtin_documentapply_or_documentwrite)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTDELETE,           builtin_documentdelete)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTGET,              builtin_documentget)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTREAD,             builtin_documentread)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTWRITE,            builtin_documentapply_or_documentwrite)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTS,                builtin_documents)
  BIND_DOWN(PMATH_SYMBOL_DOCUMENTSAVE,             builtin_documentsave)
  BIND_DOWN(PMATH_SYMBOL_EVALUATIONDOCUMENT,       builtin_evaluationdocument)
  BIND_DOWN(PMATH_SYMBOL_FRONTENDTOKENEXECUTE,     builtin_frontendtokenexecute)
  BIND_DOWN(PMATH_SYMBOL_SECTIONPRINT,             builtin_sectionprint)
  BIND_DOWN(PMATH_SYMBOL_SELECTEDDOCUMENT,         builtin_selecteddocument)
  
  BIND_UP(richmath_System_FrontEndObject,          builtin_feo_options)
  
  BIND_DOWN(richmath_FE_AddConfigShaper,     builtin_addconfigshaper)
  BIND_DOWN(richmath_FE_InternalExecuteFor,  builtin_internalexecutefor)
  BIND_DOWN(richmath_FE_ColorDialog,         builtin_colordialog)
  BIND_DOWN(richmath_FE_FileOpenDialog,      builtin_filedialog)
  BIND_DOWN(richmath_FE_FileSaveDialog,      builtin_filedialog)
  BIND_DOWN(richmath_FE_FontDialog,          builtin_fontdialog)
  
  Application::register_menucommand(Symbol(richmath_FE_CopySpecial),   copy_special_cmd, can_copy_cut);
  Application::register_menucommand(Symbol(PMATH_SYMBOL_RULE),         set_style_cmd,    can_set_style);
  Application::register_menucommand(Symbol(richmath_FE_ScopedCommand), do_scoped_cmd,    can_do_scoped);
  
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
#define RICHMATH_DECLARE_SYMBOL(SYM, NAME)           pmath_unref( SYM ); SYM = PMATH_NULL;
#define RICHMATH_RESET_SYMBOL_ATTRIBUTES(SYM, ATTR)  
#  include "symbols.inc"
#undef RICHMATH_RESET_SYMBOL_ATTRIBUTES
#undef RICHMATH_DECLARE_SYMBOL
}

void richmath::set_current_document(Document *document) {
  if(auto old = get_current_document())
    old->focus_killed();
    
  if(document)
    document->focus_set();
    
  current_document_id = document ? document->id() : FrontEndReference::None;
}

Document *richmath::get_current_document() {
  return dynamic_cast<Document *>(Box::find(current_document_id));
}
