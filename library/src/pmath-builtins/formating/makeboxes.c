#include <pmath-core/custom-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/packed-arrays-private.h>
#include <pmath-core/symbols-private.h>

#include <pmath-language/patterns-private.h>
#include <pmath-language/scanner.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads-private.h>
#include <pmath-util/debug.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/symbol-values-private.h>
#include <pmath-util/user-format-private.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control/definitions-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


extern pmath_symbol_t pmath_Developer_PackedArrayForm;

extern pmath_symbol_t pmath_System_DollarPageWidth;
extern pmath_symbol_t pmath_System_DollarFailed;

extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_And;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_AssignWith;
extern pmath_symbol_t pmath_System_AutoDelete;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_AutoNumberFormating;
extern pmath_symbol_t pmath_System_BaseForm;
extern pmath_symbol_t pmath_System_Bold;
extern pmath_symbol_t pmath_System_BoxRotation;
extern pmath_symbol_t pmath_System_ButtonBox;
extern pmath_symbol_t pmath_System_CircleTimes;
extern pmath_symbol_t pmath_System_CirclePlus;
extern pmath_symbol_t pmath_System_Colon;
extern pmath_symbol_t pmath_System_ColonForm;
extern pmath_symbol_t pmath_System_Column;
extern pmath_symbol_t pmath_System_ColumnSpacing;
extern pmath_symbol_t pmath_System_Complex;
extern pmath_symbol_t pmath_System_ComplexInfinity;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_Congruent;
extern pmath_symbol_t pmath_System_Cross;
extern pmath_symbol_t pmath_System_CupCap;
extern pmath_symbol_t pmath_System_Decrement;
extern pmath_symbol_t pmath_System_Derivative;
extern pmath_symbol_t pmath_System_DirectedInfinity;
extern pmath_symbol_t pmath_System_Dot;
extern pmath_symbol_t pmath_System_DotEqual;
extern pmath_symbol_t pmath_System_DoubleDownArrow;
extern pmath_symbol_t pmath_System_DoubleLeftArrow;
extern pmath_symbol_t pmath_System_DoubleLeftRightArrow;
extern pmath_symbol_t pmath_System_DoubleLowerLeftArrow;
extern pmath_symbol_t pmath_System_DoubleLowerRightArrow;
extern pmath_symbol_t pmath_System_DoubleRightArrow;
extern pmath_symbol_t pmath_System_DoubleUpArrow;
extern pmath_symbol_t pmath_System_DoubleUpDownArrow;
extern pmath_symbol_t pmath_System_DoubleUpperLeftArrow;
extern pmath_symbol_t pmath_System_DoubleUpperRightArrow;
extern pmath_symbol_t pmath_System_DownArrow;
extern pmath_symbol_t pmath_System_DivideBy;
extern pmath_symbol_t pmath_System_E;
extern pmath_symbol_t pmath_System_Element;
extern pmath_symbol_t pmath_System_Equal;
extern pmath_symbol_t pmath_System_EvaluationSequence;
extern pmath_symbol_t pmath_System_Factorial;
extern pmath_symbol_t pmath_System_Factorial2;
extern pmath_symbol_t pmath_System_False;
extern pmath_symbol_t pmath_System_FillBox;
extern pmath_symbol_t pmath_System_FontColor;
extern pmath_symbol_t pmath_System_FontSize;
extern pmath_symbol_t pmath_System_FontSlant;
extern pmath_symbol_t pmath_System_FontWeight;
extern pmath_symbol_t pmath_System_Format;
extern pmath_symbol_t pmath_System_FractionBox;
extern pmath_symbol_t pmath_System_FrameBox;
extern pmath_symbol_t pmath_System_Framed;
extern pmath_symbol_t pmath_System_FullForm;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_Graphics;
extern pmath_symbol_t pmath_System_GrayLevel;
extern pmath_symbol_t pmath_System_Greater;
extern pmath_symbol_t pmath_System_GreaterEqual;
extern pmath_symbol_t pmath_System_GreaterEqualLess;
extern pmath_symbol_t pmath_System_GreaterFullEqual;
extern pmath_symbol_t pmath_System_GreaterGreater;
extern pmath_symbol_t pmath_System_GreaterLess;
extern pmath_symbol_t pmath_System_GreaterTilde;
extern pmath_symbol_t pmath_System_Grid;
extern pmath_symbol_t pmath_System_GridBox;
extern pmath_symbol_t pmath_System_GridBoxColumnSpacing;
extern pmath_symbol_t pmath_System_GridBoxRowSpacing;
extern pmath_symbol_t pmath_System_HoldForm;
extern pmath_symbol_t pmath_System_HumpDownHump;
extern pmath_symbol_t pmath_System_HumpEqual;
extern pmath_symbol_t pmath_System_Identical;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_Inequation;
extern pmath_symbol_t pmath_System_Infinity;
extern pmath_symbol_t pmath_System_InputForm;
extern pmath_symbol_t pmath_System_Interpretation;
extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_Invisible;
extern pmath_symbol_t pmath_System_Italic;
extern pmath_symbol_t pmath_System_Large;
extern pmath_symbol_t pmath_System_LeftArrow;
extern pmath_symbol_t pmath_System_LeftRightArrow;
extern pmath_symbol_t pmath_System_LeftTriangle;
extern pmath_symbol_t pmath_System_LeftTriangleEqual;
extern pmath_symbol_t pmath_System_Less;
extern pmath_symbol_t pmath_System_LessEqual;
extern pmath_symbol_t pmath_System_LessEqualGreater;
extern pmath_symbol_t pmath_System_LessFullEqual;
extern pmath_symbol_t pmath_System_LessGreater;
extern pmath_symbol_t pmath_System_LessLess;
extern pmath_symbol_t pmath_System_LessTilde;
extern pmath_symbol_t pmath_System_LinearSolveFunction;
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_LongForm;
extern pmath_symbol_t pmath_System_LowerLeftArrow;
extern pmath_symbol_t pmath_System_LowerRightArrow;
extern pmath_symbol_t pmath_System_MakeBoxes;
extern pmath_symbol_t pmath_System_MatrixForm;
extern pmath_symbol_t pmath_System_Medium;
extern pmath_symbol_t pmath_System_MessageName;
extern pmath_symbol_t pmath_System_MinusPlus;
extern pmath_symbol_t pmath_System_Not;
extern pmath_symbol_t pmath_System_NotCongruent;
extern pmath_symbol_t pmath_System_NotCupCap;
extern pmath_symbol_t pmath_System_NotElement;
extern pmath_symbol_t pmath_System_NotGreater;
extern pmath_symbol_t pmath_System_NotGreaterLess;
extern pmath_symbol_t pmath_System_NotGreaterEqual;
extern pmath_symbol_t pmath_System_NotGreaterTilde;
extern pmath_symbol_t pmath_System_NotLeftTriangle;
extern pmath_symbol_t pmath_System_NotLeftTriangleEqual;
extern pmath_symbol_t pmath_System_NotLess;
extern pmath_symbol_t pmath_System_NotLessEqual;
extern pmath_symbol_t pmath_System_NotLessGreater;
extern pmath_symbol_t pmath_System_NotLessTilde;
extern pmath_symbol_t pmath_System_NotPrecedes;
extern pmath_symbol_t pmath_System_NotPrecedesEqual;
extern pmath_symbol_t pmath_System_NotReverseElement;
extern pmath_symbol_t pmath_System_NotRightTriangle;
extern pmath_symbol_t pmath_System_NotRightTriangleEqual;
extern pmath_symbol_t pmath_System_NotSubset;
extern pmath_symbol_t pmath_System_NotSubsetEqual;
extern pmath_symbol_t pmath_System_NotSucceeds;
extern pmath_symbol_t pmath_System_NotSucceedsEqual;
extern pmath_symbol_t pmath_System_NotSuperset;
extern pmath_symbol_t pmath_System_NotSupersetEqual;
extern pmath_symbol_t pmath_System_NotTildeEqual;
extern pmath_symbol_t pmath_System_NotTildeFullEqual;
extern pmath_symbol_t pmath_System_NotTildeTilde;
extern pmath_symbol_t pmath_System_Optional;
extern pmath_symbol_t pmath_System_Or;
extern pmath_symbol_t pmath_System_OutputForm;
extern pmath_symbol_t pmath_System_Overscript;
extern pmath_symbol_t pmath_System_OverscriptBox;
extern pmath_symbol_t pmath_System_PaneBox;
extern pmath_symbol_t pmath_System_PanelBox;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_Pi;
extern pmath_symbol_t pmath_System_Piecewise;
extern pmath_symbol_t pmath_System_Placeholder;
extern pmath_symbol_t pmath_System_Plain;
extern pmath_symbol_t pmath_System_Plus;
extern pmath_symbol_t pmath_System_PlusMinus;
extern pmath_symbol_t pmath_System_PostDecrement;
extern pmath_symbol_t pmath_System_PostIncrement;
extern pmath_symbol_t pmath_System_Power;
extern pmath_symbol_t pmath_System_Precedes;
extern pmath_symbol_t pmath_System_PrecedesEqual;
extern pmath_symbol_t pmath_System_PrecedesTilde;
extern pmath_symbol_t pmath_System_PureArgument;
extern pmath_symbol_t pmath_System_RadicalBox;
extern pmath_symbol_t pmath_System_Range;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_ReverseElement;
extern pmath_symbol_t pmath_System_RGBColor;
extern pmath_symbol_t pmath_System_RightTriangle;
extern pmath_symbol_t pmath_System_RightTriangleEqual;
extern pmath_symbol_t pmath_System_Rotate;
extern pmath_symbol_t pmath_System_RotationBox;
extern pmath_symbol_t pmath_System_Row;
extern pmath_symbol_t pmath_System_RowSpacing;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_SetterBox;
extern pmath_symbol_t pmath_System_Shallow;
extern pmath_symbol_t pmath_System_Short;
extern pmath_symbol_t pmath_System_ShowContents;
extern pmath_symbol_t pmath_System_ShowStringCharacters;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_Skeleton;
extern pmath_symbol_t pmath_System_Small;
extern pmath_symbol_t pmath_System_SqrtBox;
extern pmath_symbol_t pmath_System_StandardForm;
extern pmath_symbol_t pmath_System_StringBox;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_StringForm;
extern pmath_symbol_t pmath_System_StripOnInput;
extern pmath_symbol_t pmath_System_Style;
extern pmath_symbol_t pmath_System_StyleBox;
extern pmath_symbol_t pmath_System_Subscript;
extern pmath_symbol_t pmath_System_SubscriptBox;
extern pmath_symbol_t pmath_System_Subset;
extern pmath_symbol_t pmath_System_SubsetEqual;
extern pmath_symbol_t pmath_System_Subsuperscript;
extern pmath_symbol_t pmath_System_SubsuperscriptBox;
extern pmath_symbol_t pmath_System_Succeeds;
extern pmath_symbol_t pmath_System_SucceedsEqual;
extern pmath_symbol_t pmath_System_SucceedsTilde;
extern pmath_symbol_t pmath_System_Superscript;
extern pmath_symbol_t pmath_System_SuperscriptBox;
extern pmath_symbol_t pmath_System_Superset;
extern pmath_symbol_t pmath_System_SupersetEqual;
extern pmath_symbol_t pmath_System_Switch;
extern pmath_symbol_t pmath_System_TagAssign;
extern pmath_symbol_t pmath_System_TagAssignDelayed;
extern pmath_symbol_t pmath_System_TagBox;
extern pmath_symbol_t pmath_System_TemplateBox;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_TildeEqual;
extern pmath_symbol_t pmath_System_TildeFullEqual;
extern pmath_symbol_t pmath_System_TildeTilde;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_TimesBy;
extern pmath_symbol_t pmath_System_Tiny;
extern pmath_symbol_t pmath_System_TooltipBox;
extern pmath_symbol_t pmath_System_TransformationBox;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Underoverscript;
extern pmath_symbol_t pmath_System_UnderoverscriptBox;
extern pmath_symbol_t pmath_System_Underscript;
extern pmath_symbol_t pmath_System_UnderscriptBox;
extern pmath_symbol_t pmath_System_Unequal;
extern pmath_symbol_t pmath_System_Unidentical;
extern pmath_symbol_t pmath_System_UpArrow;
extern pmath_symbol_t pmath_System_UpDownArrow;
extern pmath_symbol_t pmath_System_UpperLeftArrow;
extern pmath_symbol_t pmath_System_UpperRightArrow;


struct emit_stylebox_options_info_t {
  pmath_bool_t have_explicit_strip_on_input;
};

const int PMATH_PREC_INC_OUT = PMATH_PREC_RANGE + 1;


static pmath_token_t pmath_token_analyse_output(const uint16_t *str, int len, int *prec);
static pmath_token_t box_token_analyse(pmath_t box, int *prec); // box will be freed
static int box_token_prefix_prec(pmath_t box, int defprec); // box will be freed
static int expr_precedence(pmath_t box, int *pos); // box wont be freed
static pmath_t ensure_min_precedence(pmath_t box, int minprec, int pos);

static pmath_t relation(pmath_symbol_t head, int boxform); // head wont be freed
static pmath_t simple_nary(pmath_symbol_t head, int *prec, int boxform); // head wont be freed
static pmath_t simple_prefix(pmath_symbol_t head, int *prec, int boxform); // head wont be freed
static pmath_t simple_postfix(pmath_symbol_t head, int *prec, int boxform); // head wont be freed
static pmath_t simple_binary(pmath_symbol_t head, int *leftprec, int *rightprec, int boxform); // head wont be freed
static int _pmath_symbol_to_precedence(pmath_t head); // head wont be freed

static pmath_t object_to_boxes(pmath_thread_t thread, pmath_t obj);
static pmath_t object_to_boxes_or_empty(pmath_thread_t thread, pmath_t obj);

static pmath_t nary_to_boxes(pmath_thread_t thread, pmath_expr_t expr, pmath_t op_box, int firstprec, int restprec, pmath_bool_t skip_null); // expr, op_box will be freed 
static pmath_t extract_minus(pmath_t *box);
static pmath_bool_t negate_exponent(pmath_t *obj);
static pmath_bool_t is_char(pmath_t obj, uint16_t ch);
static pmath_bool_t is_char_at(pmath_expr_t expr, size_t i, uint16_t ch);
static pmath_bool_t is_minus(pmath_t box);
static pmath_bool_t is_inversion(pmath_t box);
static pmath_t remove_paren(pmath_t box);
static uint16_t first_char(pmath_t box);
static uint16_t last_char (pmath_t box);
static void emit_num_den(pmath_expr_t product);

// For InputForm:
static pmath_t call_to_boxes(              pmath_thread_t thread, pmath_expr_t expr);
static pmath_t complex_to_boxes(           pmath_thread_t thread, pmath_expr_t expr);
static pmath_t directedinfinity_to_boxes(  pmath_thread_t thread, pmath_expr_t expr);
static pmath_t evaluationsequence_to_boxes(pmath_thread_t thread, pmath_expr_t expr);
static pmath_t inequation_to_boxes(        pmath_thread_t thread, pmath_expr_t expr);
static pmath_t list_to_boxes(              pmath_thread_t thread, pmath_expr_t expr);
static pmath_t optional_to_boxes(          pmath_thread_t thread, pmath_expr_t expr);
static pmath_t pattern_to_boxes(           pmath_thread_t thread, pmath_expr_t expr);
static pmath_t plus_to_boxes(              pmath_thread_t thread, pmath_expr_t expr);
static pmath_t enclose_subsuper_base(pmath_t box);
static pmath_t power_to_boxes(             pmath_thread_t thread, pmath_expr_t expr);
static pmath_t pureargument_to_boxes(      pmath_thread_t thread, pmath_expr_t expr);
static pmath_t range_to_boxes(             pmath_thread_t thread, pmath_expr_t expr);
static pmath_t repeated_to_boxes(          pmath_thread_t thread, pmath_expr_t expr);
static pmath_t singlematch_to_boxes(       pmath_thread_t thread, pmath_expr_t expr);
static pmath_t subscript_to_boxes(         pmath_thread_t thread, pmath_expr_t expr);
static pmath_t subsuperscript_to_boxes(    pmath_thread_t thread, pmath_expr_t expr);
static pmath_t superscript_to_boxes(       pmath_thread_t thread, pmath_expr_t expr);
static pmath_t tagassign_to_boxes(         pmath_thread_t thread, pmath_expr_t expr);
static pmath_t times_to_boxes(             pmath_thread_t thread, pmath_expr_t expr);

// For OutputForm:
static pmath_t baseform_to_boxes(                 pmath_thread_t thread, pmath_expr_t expr);
static pmath_t column_to_boxes(                   pmath_thread_t thread, pmath_expr_t expr);
static pmath_t derivative_to_boxes(               pmath_thread_t thread, pmath_expr_t expr);
static void emit_gridbox_options(pmath_expr_t expr, size_t start); // expr wont be freed
static pmath_t grid_to_boxes(                     pmath_thread_t thread, pmath_expr_t expr);
static pmath_t fullform(                          pmath_thread_t thread, pmath_t obj);
static pmath_t fullform_to_boxes(                 pmath_thread_t thread, pmath_expr_t expr);
static pmath_t holdform_to_boxes(                 pmath_thread_t thread, pmath_expr_t expr);
static pmath_t inputform_to_boxes(                pmath_thread_t thread, pmath_expr_t expr);
static pmath_t strip_interpretation_boxes(pmath_t expr);
static pmath_t interpretation_to_boxes(           pmath_thread_t thread, pmath_expr_t expr);
static pmath_t linearsolvefunction_to_boxes(      pmath_thread_t thread, pmath_expr_t expr);
static pmath_t longform_to_boxes(                 pmath_thread_t thread, pmath_expr_t expr);
static pmath_t matrixform(                        pmath_thread_t thread, pmath_t obj);
static pmath_t matrixform_to_boxes(               pmath_thread_t thread, pmath_expr_t expr);
static pmath_t outputform_to_boxes(               pmath_thread_t thread, pmath_expr_t expr);
static pmath_t packedarrayform_to_boxes(          pmath_thread_t thread, pmath_expr_t expr);
static pmath_t row_to_boxes(                      pmath_thread_t thread, pmath_expr_t expr);
static pmath_t shallow_to_boxes(                  pmath_thread_t thread, pmath_expr_t expr);
static pmath_t skeleton_to_boxes(                 pmath_thread_t thread, pmath_expr_t expr);
static pmath_bool_t emit_stylebox_options(pmath_expr_t expr, size_t start, struct emit_stylebox_options_info_t *info); // expr wont be freed
static pmath_t style_to_boxes(                    pmath_thread_t thread, pmath_expr_t expr);
static pmath_t underscript_or_overscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr);
static pmath_t underoverscript_to_boxes(          pmath_thread_t thread, pmath_expr_t expr);

// For StandardForm:
static pmath_t expr_to_boxes(             pmath_thread_t thread, pmath_expr_t expr);
static pmath_t framed_to_boxes(           pmath_thread_t thread, pmath_expr_t expr);
static pmath_t invisible_to_boxes(        pmath_thread_t thread, pmath_expr_t expr);
static pmath_t packed_array_to_boxes(     pmath_thread_t thread, pmath_packed_array_t packed_array);
static pmath_t piecewise_to_boxes(        pmath_thread_t thread, pmath_expr_t expr);
static pmath_bool_t user_make_boxes(pmath_t *obj);
static pmath_t string_to_stringbox(       pmath_thread_t thread, pmath_t obj);
static pmath_t object_to_boxes_or_empty(  pmath_thread_t thread, pmath_t obj);
static pmath_t object_to_boxes(           pmath_thread_t thread, pmath_t obj);

static pmath_t strip_pattern_condition(pmath_t expr);

//{ operator precedence of boxes ...

static pmath_token_t pmath_token_analyse_output(const uint16_t *str, int len, int *prec) {
  pmath_token_t tok = pmath_token_analyse(str, len, prec);
  
  if(prec && *prec == PMATH_PREC_INC)
    *prec = PMATH_PREC_INC_OUT;
    
  return tok;
}

static pmath_token_t box_token_analyse(pmath_t box, int *prec) { // box will be freed
  if(pmath_is_string(box)) {
    pmath_token_t tok = pmath_token_analyse_output(
                          pmath_string_buffer(&box),
                          pmath_string_length(box),
                          prec);
                          
    pmath_unref(box);
    return tok;
  }
  
  if( pmath_is_expr_of_len(box, pmath_System_List, 1)        ||
      pmath_is_expr_of(box, pmath_System_OverscriptBox)      ||
      pmath_is_expr_of(box, pmath_System_UnderscriptBox)     ||
      pmath_is_expr_of(box, pmath_System_UnderoverscriptBox) ||
      pmath_is_expr_of(box, pmath_System_StyleBox)           ||
      pmath_is_expr_of(box, pmath_System_TagBox)             ||
      pmath_is_expr_of(box, pmath_System_InterpretationBox))
  {
    pmath_token_t tok = box_token_analyse(pmath_expr_get_item(box, 1), prec);
    pmath_unref(box);
    return tok;
  }
  
  if( pmath_is_expr_of(box, pmath_System_SubscriptBox)      ||
      pmath_is_expr_of(box, pmath_System_SubsuperscriptBox) ||
      pmath_is_expr_of(box, pmath_System_SuperscriptBox))
  {
    pmath_unref(box);
    *prec = PMATH_PREC_PRIM;//PMATH_PREC_POW;
    return PMATH_TOK_BINARY_RIGHT;
  }
  
  pmath_unref(box);
  *prec = PMATH_PREC_ANY;
  return PMATH_TOK_NAME2;
}

static int box_token_prefix_prec(pmath_t box, int defprec) { // box will be freed
  if(pmath_is_string(box)) {
    int prec = pmath_token_prefix_precedence(
                 pmath_string_buffer(&box),
                 pmath_string_length(box),
                 defprec);
                 
    if(prec == PMATH_PREC_INC)
      prec = PMATH_PREC_INC_OUT;
      
    pmath_unref(box);
    return prec;
  }
  
  if( pmath_is_expr_of_len(box, pmath_System_List, 1)               ||
      pmath_is_expr_of_len(box, pmath_System_OverscriptBox, 2)      ||
      pmath_is_expr_of_len(box, pmath_System_UnderscriptBox, 2)     ||
      pmath_is_expr_of_len(box, pmath_System_UnderoverscriptBox, 3) ||
      pmath_is_expr_of(box, pmath_System_StyleBox)                  ||
      pmath_is_expr_of(box, pmath_System_TagBox)                    ||
      pmath_is_expr_of(box, pmath_System_InterpretationBox))
  {
    int prec = box_token_prefix_prec(pmath_expr_get_item(box, 1), defprec);
    pmath_unref(box);
    return prec;
  }
  
  return defprec + 1;
}


// pos = -1 => Prefix
// pos =  0 => Infix/Other
// pos = +1 => Postfix
static int expr_precedence(pmath_t box, int *pos) { // box wont be freed
  pmath_token_t tok;
  int prec;
  
  *pos = 0; // infix or other by default
  
  if(pmath_is_expr_of(box, pmath_System_List)) {
    size_t len = pmath_expr_length(box);
    
    if(len <= 1) {
      pmath_t item = pmath_expr_get_item(box, 1);
      prec = expr_precedence(item, pos);
      pmath_unref(item);
      return prec;
    }
    
    tok = box_token_analyse(pmath_expr_get_item(box, 1), &prec);
    switch(tok) {
      case PMATH_TOK_NONE:
      case PMATH_TOK_SPACE:
      case PMATH_TOK_DIGIT:
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
      case PMATH_TOK_SLOT:
      case PMATH_TOK_NEWLINE:
        break;
        
      case PMATH_TOK_NARY_OR_PREFIX:
      case PMATH_TOK_POSTFIX_OR_PREFIX:
      case PMATH_TOK_PLUSPLUS:
        prec = box_token_prefix_prec(pmath_expr_get_item(box, 1), prec);
        if(len == 2) {
          *pos = -1;
          goto FINISH;
        }
        return prec;
        
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_NARY:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_PREFIX:
      case PMATH_TOK_POSTFIX:
      case PMATH_TOK_CALL:
      case PMATH_TOK_CALLPIPE:
      case PMATH_TOK_ASSIGNTAG:
      case PMATH_TOK_COLON:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_INTEGRAL:
        if(len == 2) {
          *pos = -1;
          goto FINISH;
        }
        return prec;
        
      case PMATH_TOK_LEFTCALL:
      case PMATH_TOK_LEFT:
      case PMATH_TOK_RIGHT:
      case PMATH_TOK_COMMENTEND:
        return PMATH_PREC_PRIM;
        
      case PMATH_TOK_PRETEXT:
      case PMATH_TOK_QUESTION:
        *pos = -1;
        if(len == 4)
          prec = PMATH_PREC_CIRCMUL;
          
        goto FINISH;
    }
    
    tok = box_token_analyse(pmath_expr_get_item(box, 2), &prec);
    switch(tok) {
      case PMATH_TOK_NONE:
      case PMATH_TOK_SPACE:
      case PMATH_TOK_DIGIT:
      case PMATH_TOK_STRING:
      case PMATH_TOK_NAME:
      case PMATH_TOK_NAME2:
      case PMATH_TOK_LEFT:
      case PMATH_TOK_PREFIX:
      case PMATH_TOK_PRETEXT:
      case PMATH_TOK_TILDES:
      case PMATH_TOK_SLOT:
      case PMATH_TOK_INTEGRAL:
        prec = PMATH_PREC_MUL;
        break;
        
      case PMATH_TOK_CALL:
      case PMATH_TOK_LEFTCALL:
        prec = PMATH_PREC_CALL;
        break;
        
      case PMATH_TOK_RIGHT:
      case PMATH_TOK_COMMENTEND:
        prec = PMATH_PREC_PRIM;
        break;
        
      case PMATH_TOK_BINARY_LEFT:
      case PMATH_TOK_QUESTION:
      case PMATH_TOK_BINARY_RIGHT:
      case PMATH_TOK_ASSIGNTAG:
      case PMATH_TOK_NARY:
      case PMATH_TOK_NARY_AUTOARG:
      case PMATH_TOK_NARY_OR_PREFIX:
      case PMATH_TOK_NEWLINE:
      case PMATH_TOK_COLON:
      case PMATH_TOK_CALLPIPE:
        //prec = prec;  // TODO: what did i mean here?
        break;
        
      case PMATH_TOK_POSTFIX_OR_PREFIX:
      case PMATH_TOK_POSTFIX:
        if(len == 2)
          *pos = +1;
        prec = prec;
        break;
        
      case PMATH_TOK_PLUSPLUS:
        if(len == 2) {
          *pos = +1;
          prec = PMATH_PREC_INC_OUT;
        }
        else {
          prec = PMATH_PREC_STR;
        }
        break;
    }
    
  FINISH:
    if(*pos >= 0) { // infix or postfix
      int prec2, pos2;
      pmath_t item;
      
      item = pmath_expr_get_item(box, 1);
      prec2 = expr_precedence(item, &pos2);
      pmath_unref(item);
      
      if(pos2 > 0 && prec2 < prec) { // starts with postfix operator, eg. box = a+b&*c = ((a+b)&)*c
        prec = prec2;
        *pos = 0;
      }
    }
    
    if(*pos <= 0) { // prefix or infix
      int prec2, pos2;
      pmath_t item;
      
      item = pmath_expr_get_item(box, pmath_expr_length(box));
      prec2 = expr_precedence(item, &pos2);
      pmath_unref(item);
      
      if(pos2 < 0 && prec2 < prec) { // ends with prefix operator, eg. box = a*!b+c = a*(!(b+c))
        prec = prec2;
        *pos = 0;
      }
    }
    
    return prec;
  }
  
  if( pmath_is_expr_of(box, pmath_System_StyleBox) ||
      pmath_is_expr_of(box, pmath_System_TagBox)   ||
      pmath_is_expr_of(box, pmath_System_InterpretationBox))
  {
    pmath_t item = pmath_expr_get_item(box, 1);
    tok = expr_precedence(item, pos);
    pmath_unref(item);
    return tok;
  }
  
  return PMATH_PREC_PRIM;
}

// pos = -1: box at start => allow any Postfix operator (except ++,--)
// pos = +1: box at end   => allow any Prefix operator (except ++,--)
static pmath_t ensure_min_precedence(pmath_t box, int minprec, int pos) {
  int expr_pos;
  int p = expr_precedence(box, &expr_pos);
  
  if(p < minprec) {
    if(p != PMATH_PREC_INC_OUT) {
      if(pos < 0 && expr_pos > 0)
        return box;
        
      if(pos > 0 && expr_pos < 0)
        return box;
    }
    
    return pmath_build_value("(sos)", "(", box, ")");
  }
  
  return box;
}

//} ... operator precedence of boxes

//{ boxforms for simple functions ...

static pmath_t relation(pmath_symbol_t head, int boxform) { // head wont be freed
#define  RET_CH(C)  do{ int         retc = (C); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S)  do{ const char *rets = (S); return pmath_build_value("s", rets); }while(0)

  if(pmath_same(head, pmath_System_Equal))        RET_CH('=');
  if(pmath_same(head, pmath_System_Less))         RET_CH('<');
  if(pmath_same(head, pmath_System_Greater))      RET_CH('>');
  if(pmath_same(head, pmath_System_Identical))    RET_ST("===");
  if(pmath_same(head, pmath_System_Unidentical))  RET_ST("=!=");
  
  if(boxform < BOXFORM_OUTPUT) {
    if(pmath_same(head, pmath_System_Element))               RET_CH(0x2208);
    if(pmath_same(head, pmath_System_NotElement))            RET_CH(0x2209);
    if(pmath_same(head, pmath_System_ReverseElement))        RET_CH(0x220B);
    if(pmath_same(head, pmath_System_NotReverseElement))     RET_CH(0x220C);
    
    if(pmath_same(head, pmath_System_Colon))                 RET_CH(0x2236);
    
    if(pmath_same(head, pmath_System_TildeEqual))            RET_CH(0x2243);
    if(pmath_same(head, pmath_System_NotTildeEqual))         RET_CH(0x2244);
    if(pmath_same(head, pmath_System_TildeFullEqual))        RET_CH(0x2245);
    
    if(pmath_same(head, pmath_System_NotTildeFullEqual))     RET_CH(0x2247);
    if(pmath_same(head, pmath_System_TildeTilde))            RET_CH(0x2248);
    if(pmath_same(head, pmath_System_NotTildeTilde))         RET_CH(0x2249);
    
    if(pmath_same(head, pmath_System_CupCap))                RET_CH(0x224D);
    if(pmath_same(head, pmath_System_HumpDownHump))          RET_CH(0x224E);
    if(pmath_same(head, pmath_System_HumpEqual))             RET_CH(0x224F);
    if(pmath_same(head, pmath_System_DotEqual))              RET_CH(0x2250);
    
    if(pmath_same(head, pmath_System_Unequal))               RET_CH(0x2260);
    if(pmath_same(head, pmath_System_Congruent))             RET_CH(0x2261);
    if(pmath_same(head, pmath_System_NotCongruent))          RET_CH(0x2262);
    
    if(pmath_same(head, pmath_System_LessEqual))             RET_CH(0x2264);
    if(pmath_same(head, pmath_System_GreaterEqual))          RET_CH(0x2265);
    if(pmath_same(head, pmath_System_LessFullEqual))         RET_CH(0x2266);
    if(pmath_same(head, pmath_System_GreaterFullEqual))      RET_CH(0x2267);
    
    if(pmath_same(head, pmath_System_LessLess))              RET_CH(0x226A);
    if(pmath_same(head, pmath_System_GreaterGreater))        RET_CH(0x226B);
    
    if(pmath_same(head, pmath_System_NotCupCap))             RET_CH(0x226D);
    if(pmath_same(head, pmath_System_NotLess))               RET_CH(0x226E);
    if(pmath_same(head, pmath_System_NotGreater))            RET_CH(0x226F);
    if(pmath_same(head, pmath_System_NotLessEqual))          RET_CH(0x2270);
    if(pmath_same(head, pmath_System_NotGreaterEqual))       RET_CH(0x2271);
    if(pmath_same(head, pmath_System_LessTilde))             RET_CH(0x2272);
    if(pmath_same(head, pmath_System_GreaterTilde))          RET_CH(0x2273);
    if(pmath_same(head, pmath_System_NotLessTilde))          RET_CH(0x2274);
    if(pmath_same(head, pmath_System_NotGreaterTilde))       RET_CH(0x2275);
    if(pmath_same(head, pmath_System_LessGreater))           RET_CH(0x2276);
    if(pmath_same(head, pmath_System_GreaterLess))           RET_CH(0x2277);
    if(pmath_same(head, pmath_System_NotLessGreater))        RET_CH(0x2278);
    if(pmath_same(head, pmath_System_NotGreaterLess))        RET_CH(0x2279);
    if(pmath_same(head, pmath_System_Precedes))              RET_CH(0x227A);
    if(pmath_same(head, pmath_System_Succeeds))              RET_CH(0x227B);
    if(pmath_same(head, pmath_System_PrecedesEqual))         RET_CH(0x227C);
    if(pmath_same(head, pmath_System_SucceedsEqual))         RET_CH(0x227D);
    if(pmath_same(head, pmath_System_PrecedesTilde))         RET_CH(0x227E);
    if(pmath_same(head, pmath_System_SucceedsTilde))         RET_CH(0x227F);
    if(pmath_same(head, pmath_System_NotPrecedes))           RET_CH(0x2280);
    if(pmath_same(head, pmath_System_NotSucceeds))           RET_CH(0x2281);
    if(pmath_same(head, pmath_System_Subset))                RET_CH(0x2282);
    if(pmath_same(head, pmath_System_Superset))              RET_CH(0x2283);
    if(pmath_same(head, pmath_System_NotSubset))             RET_CH(0x2284);
    if(pmath_same(head, pmath_System_NotSuperset))           RET_CH(0x2285);
    if(pmath_same(head, pmath_System_SubsetEqual))           RET_CH(0x2286);
    if(pmath_same(head, pmath_System_SupersetEqual))         RET_CH(0x2287);
    if(pmath_same(head, pmath_System_NotSubsetEqual))        RET_CH(0x2288);
    if(pmath_same(head, pmath_System_NotSupersetEqual))      RET_CH(0x2289);
    
    if(pmath_same(head, pmath_System_LeftTriangle))          RET_CH(0x22B3);
    if(pmath_same(head, pmath_System_RightTriangle))         RET_CH(0x22B4);
    if(pmath_same(head, pmath_System_LeftTriangleEqual))     RET_CH(0x22B5);
    if(pmath_same(head, pmath_System_RightTriangleEqual))    RET_CH(0x22B6);
    
    if(pmath_same(head, pmath_System_LessEqualGreater))      RET_CH(0x22DA);
    if(pmath_same(head, pmath_System_GreaterEqualLess))      RET_CH(0x22DB);
    
    if(pmath_same(head, pmath_System_NotPrecedesEqual))      RET_CH(0x22E0);
    if(pmath_same(head, pmath_System_NotSucceedsEqual))      RET_CH(0x22E1);
    
    if(pmath_same(head, pmath_System_NotLeftTriangle))       RET_CH(0x22EA);
    if(pmath_same(head, pmath_System_NotRightTriangle))      RET_CH(0x22EA);
    if(pmath_same(head, pmath_System_NotLeftTriangleEqual))  RET_CH(0x22EA);
    if(pmath_same(head, pmath_System_NotRightTriangleEqual)) RET_CH(0x22EA);
    
  }
  else {
    if(pmath_same(head, pmath_System_Unequal))       RET_ST("!=");
    if(pmath_same(head, pmath_System_LessEqual))     RET_ST("<=");
    if(pmath_same(head, pmath_System_GreaterEqual))  RET_ST(">=");
    
    if(boxform < BOXFORM_INPUT) {
      if(pmath_same(head, pmath_System_Colon))   RET_CH(':');
    }
  }
  
  return PMATH_NULL;
  
#undef RET_CH
#undef RET_ST
}

    
#define  RET_CH(C,P)  do{ int         retc = (C); *prec = (P); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S,P)  do{ const char *rets = (S); *prec = (P); return pmath_build_value("s", rets); }while(0)
static pmath_t simple_nary(pmath_symbol_t head, int *prec, int boxform) { // head wont be freed
//  if(pmath_same(head, pmath_System_Sequence))            RET_CH(',',  PMATH_PREC_SEQ);
//  if(pmath_same(head, pmath_System_EvaluationSequence))  RET_CH(';',  PMATH_PREC_EVAL);
  if(pmath_same(head, pmath_System_StringExpression))    RET_ST("++", PMATH_PREC_STR);
  if(pmath_same(head, pmath_System_Alternatives))        RET_CH('|',  PMATH_PREC_ALT);
  
  if(boxform < BOXFORM_OUTPUT) {
    if(pmath_same(head, pmath_System_And))                   RET_CH(0x2227, PMATH_PREC_AND);
    if(pmath_same(head, pmath_System_Or))                    RET_CH(0x2228, PMATH_PREC_AND);
    
    if(pmath_same(head, pmath_System_CirclePlus))            RET_CH(0x2A2F, PMATH_PREC_CIRCADD);
    if(pmath_same(head, pmath_System_CircleTimes))           RET_CH(0x2A2F, PMATH_PREC_CIRCMUL);
    if(pmath_same(head, pmath_System_PlusMinus))             RET_CH(0x00B1, PMATH_PREC_PLUMI);
    if(pmath_same(head, pmath_System_MinusPlus))             RET_CH(0x2213, PMATH_PREC_PLUMI);
    
    if(pmath_same(head, pmath_System_Dot))                   RET_CH(0x22C5, PMATH_PREC_MIDDOT);  //0x00B7
    if(pmath_same(head, pmath_System_Cross))                 RET_CH(0x2A2F, PMATH_PREC_CROSS);
    
    if(pmath_same(head, pmath_System_LeftArrow))             RET_CH(0x2190, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_UpArrow))               RET_CH(0x2191, PMATH_PREC_ARROW);
    //if(pmath_same(head, PMATH_SYMBOL_RIGHTARROW))            RET_CH(0x2192, PMATH_PREC_ARROW); // Rule
    if(pmath_same(head, pmath_System_DownArrow))             RET_CH(0x2193, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_LeftRightArrow))        RET_CH(0x2194, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_UpDownArrow))           RET_CH(0x2195, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_UpperLeftArrow))        RET_CH(0x2196, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_UpperRightArrow))       RET_CH(0x2197, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_LowerRightArrow))       RET_CH(0x2198, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_LowerLeftArrow))        RET_CH(0x2199, PMATH_PREC_ARROW);
    
    if(pmath_same(head, pmath_System_DoubleLeftArrow))       RET_CH(0x21D0, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleUpArrow))         RET_CH(0x21D1, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleRightArrow))      RET_CH(0x21D2, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleDownArrow))       RET_CH(0x21D3, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleLeftRightArrow))  RET_CH(0x21D4, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleUpDownArrow))     RET_CH(0x21D5, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleUpperLeftArrow))  RET_CH(0x21D6, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleUpperRightArrow)) RET_CH(0x21D7, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleLowerRightArrow)) RET_CH(0x21D8, PMATH_PREC_ARROW);
    if(pmath_same(head, pmath_System_DoubleLowerLeftArrow))  RET_CH(0x21D9, PMATH_PREC_ARROW);
    
  }
  else {
    if(pmath_same(head, pmath_System_And))  RET_ST("&&", PMATH_PREC_AND);
    if(pmath_same(head, pmath_System_Or))   RET_ST("||", PMATH_PREC_AND);
  }
  
  *prec = PMATH_PREC_REL;
  return relation(head, boxform);
}

static pmath_t simple_prefix(pmath_symbol_t head, int *prec, int boxform) { // head wont be freed

  if(pmath_same(head, pmath_System_Increment))     RET_ST("++", PMATH_PREC_INC_OUT);
  if(pmath_same(head, pmath_System_Decrement))     RET_ST("--", PMATH_PREC_INC_OUT);
  
  if(boxform < BOXFORM_OUTPUT) {
    if(pmath_same(head, pmath_System_Not))  RET_CH(0x00AC, PMATH_PREC_REL);
    
    if(pmath_same(head, pmath_System_PlusMinus))    RET_CH(0x00B1, PMATH_PREC_PLUMI);
    if(pmath_same(head, pmath_System_MinusPlus))    RET_CH(0x2213, PMATH_PREC_PLUMI);
  }
  else {
    if(pmath_same(head, pmath_System_Not))  RET_CH('!', PMATH_PREC_REL);
  }
  
  return PMATH_NULL;
}

static pmath_t simple_postfix(pmath_symbol_t head, int *prec, int boxform) { // head wont be freed

  if(pmath_same(head, pmath_System_Factorial))      RET_CH('!',  PMATH_PREC_FAC);
  if(pmath_same(head, pmath_System_Factorial2))     RET_ST("!!", PMATH_PREC_FAC);
  if(pmath_same(head, pmath_System_PostIncrement))  RET_ST("++", PMATH_PREC_INC_OUT);
  if(pmath_same(head, pmath_System_PostDecrement))  RET_ST("--", PMATH_PREC_INC_OUT);
  
  return PMATH_NULL;
}
#undef RET_CH
#undef RET_ST

static pmath_t simple_binary(pmath_symbol_t head, int *leftprec, int *rightprec, int boxform) { // head wont be freed
#define  RET_CH(C, L, R)  do{ int         retc = (C); *leftprec = (L); *rightprec = (R); return pmath_build_value("c", retc); }while(0)
#define  RET_ST(S, L, R)  do{ const char *rets = (S); *leftprec = (L); *rightprec = (R); return pmath_build_value("s", rets); }while(0)
#define  RET_CH_L(C, L)  RET_CH(C, L, (L)+1)
#define  RET_CH_R(C, R)  RET_CH(C, (R)+1, R)
#define  RET_ST_L(S, L)  RET_ST(S, L, (L)+1)
#define  RET_ST_R(S, R)  RET_ST(S, (R)+1, R)

  if(pmath_same(head, pmath_System_Increment))   RET_ST_R("+=",  PMATH_PREC_MODY);
  if(pmath_same(head, pmath_System_Decrement))   RET_ST_R("-=",  PMATH_PREC_MODY);
  if(pmath_same(head, pmath_System_TimesBy))     RET_ST_R("*=",  PMATH_PREC_MODY);
  if(pmath_same(head, pmath_System_DivideBy))    RET_ST_R("/=",  PMATH_PREC_MODY);
  if(pmath_same(head, pmath_System_AssignWith))  RET_ST_R("//=", PMATH_PREC_MODY);
  if(pmath_same(head, pmath_System_Condition))   RET_ST_L("/?",  PMATH_PREC_COND);
  if(pmath_same(head, pmath_System_TestPattern)) RET_CH_L('?',   PMATH_PREC_TEST);
  if(pmath_same(head, pmath_System_MessageName)) RET_ST_L("::",  PMATH_PREC_CALL);
  
  if(boxform < BOXFORM_OUTPUT) {
    if(pmath_same(head, pmath_System_Assign))         RET_CH_R(PMATH_CHAR_ASSIGN,        PMATH_PREC_ASS);
    if(pmath_same(head, pmath_System_AssignDelayed))  RET_CH_R(PMATH_CHAR_ASSIGNDELAYED, PMATH_PREC_ASS);
    
    if(pmath_same(head, pmath_System_Rule))           RET_CH_R(PMATH_CHAR_RULE,        PMATH_PREC_RULE);
    if(pmath_same(head, pmath_System_RuleDelayed))    RET_CH_R(PMATH_CHAR_RULEDELAYED, PMATH_PREC_RULE);
  }
  else {
    if(pmath_same(head, pmath_System_Assign))         RET_ST_R(":=",  PMATH_PREC_ASS);
    if(pmath_same(head, pmath_System_AssignDelayed))  RET_ST_R("::=", PMATH_PREC_ASS);
    
    if(pmath_same(head, pmath_System_Rule))           RET_ST_R("->", PMATH_PREC_RULE);
    if(pmath_same(head, pmath_System_RuleDelayed))    RET_ST_R(":>", PMATH_PREC_RULE);
  }
  
  if(boxform < BOXFORM_INPUT) {
    if(pmath_same(head, pmath_System_ColonForm)) RET_ST_L(": ", PMATH_PREC_REL);
  }
  
  {
    pmath_t op = simple_nary(head, leftprec, boxform);
    *rightprec = *leftprec += 1;
    return op;
  }
  
#undef RET_ST_L
#undef RET_ST_R
#undef RET_CH_L
#undef RET_CH_R
#undef RET_CH
#undef RET_ST
}

static int _pmath_symbol_to_precedence(pmath_t head) { // head wont be freed
  pmath_t op;
  int prec, prec2;
  
  op = simple_binary(head, &prec, &prec2, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op)) {
    if(prec == prec2) // n-ary
      return prec - 1;
      
    return (prec < prec2) ? prec : prec2;
  }
  
  op = simple_prefix(head, &prec, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op))
    return prec;
    
  op = simple_postfix(head, &prec, BOXFORM_STANDARD);
  pmath_unref(op);
  if(!pmath_is_null(op))
    return prec;
    
  if(pmath_same(head, pmath_System_Sequence)) return PMATH_PREC_SEQ;
  if(pmath_same(head, pmath_System_Range))    return PMATH_PREC_RANGE;
  if(pmath_same(head, pmath_System_Plus))     return PMATH_PREC_ADD;
  if(pmath_same(head, pmath_System_Times))    return PMATH_PREC_MUL;
  if(pmath_same(head, pmath_System_Power))    return PMATH_PREC_POW;
  
  return PMATH_PREC_PRIM;
}

//} ... boxforms for simple functions

//{ boxforms for more complex functions ...

//{ boxforms valid for InputForm ...

static pmath_t nary_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr,        // will be freed
  pmath_t        op_box,      // will be freed
  int            firstprec,
  int            restprec,
  pmath_bool_t   skip_null
) {
  pmath_t item;
  size_t len = pmath_expr_length(expr);
  size_t i;
  
  pmath_gather_begin(PMATH_NULL);
  
  item = pmath_expr_get_item(expr, 1);
  if(!pmath_is_null(item) || !skip_null || len == 1) {
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item),
        firstprec,
        -1),
      PMATH_NULL);
  }
  
  if(len > 1) {
    for(i = 2; i < len; ++i) {
      pmath_emit(pmath_ref(op_box), PMATH_NULL);
      
      item = pmath_expr_get_item(expr, i);
      if(!pmath_is_null(item) || !skip_null) {
        pmath_emit(
          ensure_min_precedence(
            object_to_boxes(thread, item),
            restprec,
            0),
          PMATH_NULL);
      }
    }
    
    pmath_emit(pmath_ref(op_box), PMATH_NULL);
    
    item = pmath_expr_get_item(expr, len);
    if(!pmath_is_null(item) || !skip_null) {
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item),
          restprec,
          +1),
        PMATH_NULL);
    }
  }
  
  pmath_unref(expr);
  pmath_unref(op_box);
  return pmath_gather_end();
}

// *box = {"-", x}     -->  *box = x      and return value is "-"
// otherwise *box unchanged and return value is PMATH_NULL
static pmath_t extract_minus(pmath_t *box) {
  if(pmath_is_expr_of_len(*box, pmath_System_List, 2)) {
    pmath_t minus = pmath_expr_get_item(*box, 1);
    
    if( pmath_is_string(minus) &&
        pmath_string_equals_latin1(minus, "-"))
    {
      pmath_t x = pmath_expr_get_item(*box, 2);
      pmath_unref(*box);
      *box = x;
      return minus;
    }
    
    pmath_unref(minus);
  }
  
  return PMATH_NULL;
}

// x^-n  ==>  x^n
static pmath_bool_t negate_exponent(pmath_t *obj) {
  if(pmath_is_expr_of_len(*obj, pmath_System_Power, 2)) {
    pmath_t exp = pmath_expr_get_item(*obj, 2);
    
    if(pmath_equals(exp, PMATH_FROM_INT32(-1))) {
      pmath_unref(exp);
      exp = *obj;
      *obj = pmath_expr_get_item(exp, 1);
      pmath_unref(exp);
      return TRUE;
    }
    
    if(pmath_is_number(exp) && pmath_number_sign(exp) < 0) {
      exp = pmath_number_neg(exp);
      *obj = pmath_expr_set_item(*obj, 2, exp);
      return TRUE;
    }
    
    pmath_unref(exp);
  }
  return FALSE;
}

static pmath_bool_t is_char(pmath_t obj, uint16_t ch) {
  return pmath_is_string(obj)          &&
         pmath_string_length(obj) == 1 &&
         pmath_string_buffer(&obj)[0] == ch;
}

static pmath_bool_t is_char_at(pmath_expr_t expr, size_t i, uint16_t ch) {
  pmath_t obj = pmath_expr_get_item(expr, i);
  
  if(is_char(obj, ch)) {
    pmath_unref(obj);
    return TRUE;
  }
  
  pmath_unref(obj);
  return FALSE;
}

static pmath_bool_t is_minus(pmath_t box) {
  return pmath_is_expr_of_len(box, pmath_System_List, 2) && is_char_at(box, 1, '-');
}

static pmath_bool_t is_inversion(pmath_t box) {
  return pmath_is_expr_of_len(box, pmath_System_List, 3) &&
         is_char_at(box, 2, '/')                         &&
         is_char_at(box, 1, '1');
}

static pmath_t remove_paren(pmath_t box) {
  if( pmath_is_expr_of_len(box, pmath_System_List, 3) &&
      is_char_at(box, 1, '(') &&
      is_char_at(box, 3, ')'))
  {
    pmath_t tmp = box;
    box = pmath_expr_get_item(tmp, 1);
    pmath_unref(tmp);
  }
  
  return box;
}

static uint16_t first_char(pmath_t box) {
  if(pmath_is_string(box)) {
    if(pmath_string_length(box) > 0)
      return *pmath_string_buffer(&box);
      
    return 0;
  }
  
  if(pmath_is_expr(box) && pmath_expr_length(box) > 0) {
    pmath_t item = pmath_expr_get_item(box, 0);
    pmath_unref(item);
    
    if(pmath_same(item, pmath_System_List)) {
      uint16_t result;
      
      item = pmath_expr_get_item(box, 1);
      result = first_char(item);
      pmath_unref(item);
      
      return result;
    }
  }
  
  return 0;
}

static uint16_t last_char(pmath_t box) {
  if(pmath_is_string(box)) {
    int len = pmath_string_length(box);
    if(len > 0)
      return pmath_string_buffer(&box)[len - 1];
      
    return 0;
  }
  
  if(pmath_is_expr(box) && pmath_expr_length(box) > 0) {
    pmath_t item = pmath_expr_get_item(box, 0);
    pmath_unref(item);
    
    if(pmath_same(item, pmath_System_List)) {
      uint16_t result;
      
      item = pmath_expr_get_item(box, pmath_expr_length(box));
      result = last_char(item);
      pmath_unref(item);
      
      return result;
    }
  }
  
  return 0;
}

#define NUM_TAG  PMATH_NULL
#define DEN_TAG  PMATH_UNDEFINED

static void emit_num_den(pmath_expr_t product) { // product will be freed
  size_t i;
  
  for(i = 1; i <= pmath_expr_length(product); ++i) {
    pmath_t factor = pmath_expr_get_item(product, i);
    
    if(pmath_is_expr_of(factor, pmath_System_Times)) {
      emit_num_den(factor);
      continue;
    }
    
    if(negate_exponent(&factor)) {
      if(pmath_is_expr_of(factor, pmath_System_Times)) {
        size_t j;
        for(j = 1; j <= pmath_expr_length(factor); ++j) {
          pmath_emit(pmath_expr_get_item(factor, j), DEN_TAG);
        }
        
        pmath_unref(factor);
        continue;
      }
      
      pmath_emit(factor, DEN_TAG);
      continue;
    }
    
    if(pmath_is_quotient(factor)) {
      pmath_t num = pmath_rational_numerator(factor);
      pmath_t den = pmath_rational_denominator(factor);
      
      if(pmath_equals(num, PMATH_FROM_INT32(1)))
        pmath_unref(num);
      else
        pmath_emit(num, NUM_TAG);
        
      pmath_emit(den, DEN_TAG);
      
      pmath_unref(factor);
      continue;
    }
    
    pmath_emit(factor, NUM_TAG);
  }
  
  pmath_unref(product);
}

static pmath_t call_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  pmath_t item;
  
  pmath_gather_begin(PMATH_NULL);
  
  item = pmath_expr_get_item(expr, 0);
  pmath_emit(
    ensure_min_precedence(
      object_to_boxes(thread, item),
      PMATH_PREC_CALL,
      -1),
    PMATH_NULL);
    
  pmath_emit(PMATH_C_STRING("("), PMATH_NULL);
  
  if(pmath_expr_length(expr) > 0) {
    pmath_emit(
      nary_to_boxes(
        thread,
        expr,
        PMATH_C_STRING(","),
        PMATH_PREC_SEQ + 1,
        PMATH_PREC_SEQ + 1,
        TRUE),
      PMATH_NULL);
  }
  else
    pmath_unref(expr);
    
  pmath_emit(PMATH_C_STRING(")"), PMATH_NULL);
  
  return pmath_gather_end();
}

static pmath_t complex_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t re = pmath_expr_get_item(expr, 1);
    pmath_t im = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(re, PMATH_FROM_INT32(0))) {
      if(pmath_equals(im, PMATH_FROM_INT32(1))) {
        pmath_unref(expr);
        pmath_unref(re);
        pmath_unref(im);
        
        if(thread->boxform < BOXFORM_OUTPUT)
          return pmath_build_value("c", 0x2148);
          
        return PMATH_C_STRING("I");
      }
      
      if(pmath_is_number(im)) {
        pmath_unref(re);
        expr = pmath_expr_set_item(expr, 2, PMATH_FROM_INT32(1));
        
        return object_to_boxes(thread,
                               pmath_expr_new_extended(
                                 pmath_ref(pmath_System_Times), 2,
                                 im,
                                 expr));
      }
    }
    else if(pmath_is_number(re) && pmath_is_number(im)) {
      pmath_unref(im);
      expr = pmath_expr_set_item(expr, 1, PMATH_FROM_INT32(0));
      
      return object_to_boxes(thread,
                             pmath_expr_new_extended(
                               pmath_ref(pmath_System_Plus), 2,
                               re,
                               expr));
    }
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t directedinfinity_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t dir = pmath_expr_get_item(expr, 1);
    
    if(pmath_equals(dir, PMATH_FROM_INT32(1))) {
      pmath_unref(expr);
      pmath_unref(dir);
      
      return object_to_boxes(thread, pmath_ref(pmath_System_Infinity));
    }
    
    if(pmath_equals(dir, PMATH_FROM_INT32(-1))) {
      pmath_unref(expr);
      pmath_unref(dir);
      
      return object_to_boxes(thread,
                             pmath_expr_new_extended(
                               pmath_ref(pmath_System_Times), 2,
                               PMATH_FROM_INT32(-1),
                               pmath_ref(pmath_System_Infinity)));
    }
    
    if(pmath_is_expr_of_len(dir, pmath_System_Complex, 2)) {
      pmath_t x = pmath_expr_get_item(dir, 1);
      
      if(pmath_equals(x, PMATH_FROM_INT32(0))) {
        pmath_unref(x);
        x = pmath_expr_get_item(dir, 2);
        
        if(pmath_equals(x, PMATH_FROM_INT32(1))) {
          pmath_unref(expr);
          pmath_unref(dir);
          pmath_unref(x);
          
          return object_to_boxes(thread,
                                 pmath_expr_new_extended(
                                   pmath_ref(pmath_System_Times), 2,
                                   pmath_expr_new_extended(
                                     pmath_ref(pmath_System_Complex), 2,
                                     PMATH_FROM_INT32(0),
                                     PMATH_FROM_INT32(1)),
                                   pmath_ref(pmath_System_Infinity)));
        }
        
        if(pmath_equals(x, PMATH_FROM_INT32(-1))) {
          pmath_unref(expr);
          pmath_unref(dir);
          pmath_unref(x);
          
          return object_to_boxes(thread,
                                 pmath_expr_new_extended(
                                   pmath_ref(pmath_System_Times), 2,
                                   pmath_expr_new_extended(
                                     pmath_ref(pmath_System_Complex), 2,
                                     PMATH_FROM_INT32(0),
                                     PMATH_FROM_INT32(-1)),
                                   pmath_ref(pmath_System_Infinity)));
        }
      }
      
      pmath_unref(x);
    }
    
    pmath_unref(dir);
  }
  else if(pmath_expr_length(expr) == 0) {
    pmath_unref(expr);
    return object_to_boxes(thread, pmath_ref(pmath_System_ComplexInfinity));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t evaluationsequence_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) > 1) {
    return nary_to_boxes(
             thread,
             expr,
             PMATH_C_STRING(";"),
             PMATH_PREC_EVAL + 1,
             PMATH_PREC_EVAL + 1,
             TRUE);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t inequation_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  size_t len = pmath_expr_length(expr);
  
  if((len & 1) == 1 && len >= 3) {
    pmath_t item;
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 1);
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item),
        PMATH_PREC_REL + 1,
        -1),
      PMATH_NULL);
      
    for(i = 1; i <= len / 2; ++i) {
      pmath_t op;
      
      item = pmath_expr_get_item(expr, 2 * i);
      op = relation(item, thread->boxform);
      pmath_unref(item);
      
      if(pmath_is_null(op)) {
        pmath_unref(pmath_gather_end());
        return call_to_boxes(thread, expr);
      }
      
      pmath_emit(op, PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 2 * i + 1);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item),
          PMATH_PREC_REL + 1,
          (2 * i + 1 == len) ? +1 : 0),
        PMATH_NULL);
    }
    
    pmath_unref(expr);
    return pmath_gather_end();
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t list_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 0) {
    pmath_unref(expr);
    return pmath_build_value("ss", "{", "}");
  }
  
  return pmath_build_value("sos",
                           "{",
                           nary_to_boxes(
                             thread,
                             expr,
                             PMATH_C_STRING(","),
                             PMATH_PREC_SEQ + 1,
                             PMATH_PREC_SEQ + 1,
                             TRUE),
                           "}");
}

static pmath_t optional_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t pattern = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of_len(pattern, pmath_System_Pattern, 2)) {
      pmath_t name = pmath_expr_get_item(pattern, 1);
      pmath_t sub  = pmath_expr_get_item(pattern, 2);
      
      if(pmath_is_symbol(name) && pmath_equals(sub, _pmath_object_singlematch)) {
        pmath_unref(expr);
        pmath_unref(pattern);
        pmath_unref(sub);
        
        name = object_to_boxes(thread, name);
        name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);
        
        return pmath_build_value("so", "?", name);
      }
      
      pmath_unref(name);
      pmath_unref(sub);
    }
    
    pmath_unref(pattern);
  }
  else if(pmath_expr_length(expr) == 2) {
    pmath_t pattern = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of_len(pattern, pmath_System_Pattern, 2)) {
      pmath_t name = pmath_expr_get_item(pattern, 1);
      pmath_t sub  = pmath_expr_get_item(pattern, 2);
      pmath_t value = pmath_expr_get_item(expr, 2);
      
      if(pmath_is_symbol(name) && pmath_equals(sub, _pmath_object_singlematch)) {
        pmath_unref(expr);
        pmath_unref(pattern);
        pmath_unref(sub);
        
        name = object_to_boxes(thread, name);
        name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);
        
        value = object_to_boxes(thread, value);
        value = ensure_min_precedence(value, PMATH_PREC_CIRCMUL + 1, +1);
        
        return pmath_build_value("soso", "?", name, ":", value);
      }
      
      pmath_unref(name);
      pmath_unref(sub);
      pmath_unref(value);
    }
    
    pmath_unref(pattern);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t pattern_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t name = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_symbol(name)) {
      pmath_t pat =  pmath_expr_get_item(expr, 2);
      
      name = object_to_boxes(thread, name);
      name = ensure_min_precedence(name, PMATH_PREC_PRIM, 0);
      
      pmath_unref(expr);
      
      if(pmath_equals(pat, _pmath_object_singlematch)) {
        pmath_unref(pat);
        
        return pmath_build_value("so", "~", name);
      }
      
      if(pmath_is_expr_of_len(pat, pmath_System_SingleMatch, 1)) {
        pmath_t type = pmath_expr_get_item(pat, 1);
        
        pmath_unref(pat);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
        
        return pmath_build_value("soso", "~", name, ":", type);
      }
      
      if(pmath_equals(pat, _pmath_object_multimatch)) {
        pmath_unref(pat);
        
        return pmath_build_value("so", "~~", name);
      }
      
      if(pmath_equals(pat, _pmath_object_zeromultimatch)) {
        pmath_unref(pat);
        
        return pmath_build_value("so", "~~~", name);
      }
      
      if(pmath_is_expr_of_len(pat, pmath_System_Repeated, 2)) {
        pmath_t rep = pmath_expr_get_item(pat, 1);
        
        if(pmath_is_expr_of_len(rep, pmath_System_SingleMatch, 1)) {
          pmath_t range = pmath_expr_get_item(pat, 2);
          
          if(pmath_equals(range, _pmath_object_range_from_one)) {
            pmath_t type = pmath_expr_get_item(rep, 1);
            
            pmath_unref(rep);
            pmath_unref(range);
            pmath_unref(pat);
            type = object_to_boxes(thread, type);
            type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
            
            return pmath_build_value("soso", "~~", name, ":", type);
          }
          
          if(pmath_equals(range, _pmath_object_range_from_zero)) {
            pmath_t type = pmath_expr_get_item(rep, 1);
            
            pmath_unref(rep);
            pmath_unref(range);
            pmath_unref(pat);
            type = object_to_boxes(thread, type);
            type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
            
            return pmath_build_value("soso", "~~~", name, ":", type);
          }
          
          pmath_unref(range);
        }
        
        pmath_unref(rep);
      }
      
      pat = object_to_boxes(thread, pat);
      pat = ensure_min_precedence(pat, PMATH_PREC_MUL + 1, +1);
      
      return pmath_build_value("oso", name, ":", pat);
    }
    
    pmath_unref(name);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t plus_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  size_t len = pmath_expr_length(expr);
  
  if(len >= 2) {
    pmath_t item, minus;
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 1);
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item),
        PMATH_PREC_ADD + 1,
        -1),
      PMATH_NULL);
      
    for(i = 2; i <= len; ++i) {
      item = pmath_expr_get_item(expr, i);
      item = object_to_boxes(thread, item);
      
      minus = extract_minus(&item);
      if(pmath_is_null(minus))
        pmath_emit(PMATH_C_STRING("+"), PMATH_NULL);
      else
        pmath_emit(minus, PMATH_NULL);
        
      pmath_emit(
        ensure_min_precedence(
          item,
          PMATH_PREC_ADD + 1,
          (i == len) ? +1 : 0),
        PMATH_NULL);
    }
    
    pmath_unref(expr);
    return pmath_gather_end();
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t enclose_subsuper_base(pmath_t box) {
  if(pmath_is_expr_of(box, pmath_System_FractionBox))
    return pmath_build_value("(sos)", "(", box, ")");
    
  return ensure_min_precedence(box, PMATH_PREC_POW + 1, -1);
}

static pmath_t power_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t base, exp;
    pmath_bool_t fraction = FALSE;
    
    base = pmath_expr_get_item(expr, 1);
    exp = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    
    if(pmath_is_number(exp) && pmath_number_sign(exp) < 0) {
      exp = pmath_number_neg(exp);
      fraction = TRUE;
    }
    
    if(pmath_equals(exp, _pmath_one_half)) {
      pmath_unref(exp);
      
      if(thread->boxform < BOXFORM_OUTPUT) {
        expr = pmath_expr_new_extended(
                 pmath_ref(pmath_System_SqrtBox), 1,
                 object_to_boxes(thread, base));
      }
      else {
        expr = pmath_expr_new_extended(
                 pmath_ref(pmath_System_List), 4,
                 PMATH_C_STRING("Sqrt"),
                 PMATH_C_STRING("("),
                 object_to_boxes(thread, base),
                 PMATH_C_STRING(")"));
      }
      
      if(fraction) {
        if( thread->boxform != BOXFORM_STANDARDEXPONENT &&
            thread->boxform != BOXFORM_OUTPUTEXPONENT   &&
            thread->boxform <= BOXFORM_OUTPUT)
        {
          return pmath_expr_new_extended(
                   pmath_ref(pmath_System_FractionBox), 2,
                   PMATH_C_STRING("1"),
                   expr);
        }
        else {
          if(first_char(expr) == '?')
            expr = pmath_build_value("(so)", " ", expr);
          return pmath_build_value("(sso)", "1", "/", expr);
        }
      }
      
      return expr;
    }
    
    if(!pmath_equals(exp, PMATH_FROM_INT32(1))) {
      int old_boxform = thread->boxform;
      
      if(thread->boxform == BOXFORM_STANDARD)
        thread->boxform = BOXFORM_STANDARDEXPONENT;
      else if(thread->boxform == BOXFORM_OUTPUT)
        thread->boxform = BOXFORM_OUTPUTEXPONENT;
        
      exp = object_to_boxes(thread, exp);
      thread->boxform = old_boxform;
      
      if(thread->boxform < BOXFORM_INPUT) {
        if( pmath_is_expr_of(base, pmath_System_Subscript) &&
            pmath_expr_length(base) >= 2)
        {
          pmath_expr_t indices;
          
          expr = base;
          base = pmath_expr_get_item(expr, 1);
          indices = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
          pmath_unref(expr);
          
          exp = pmath_expr_new_extended(
                  pmath_ref(pmath_System_SubsuperscriptBox), 2,
                  nary_to_boxes(
                    thread,
                    indices,
                    PMATH_C_STRING(","),
                    PMATH_PREC_SEQ + 1,
                    PMATH_PREC_SEQ + 1,
                    TRUE),
                  exp);
        }
        else if(thread->boxform <= BOXFORM_OUTPUT)
          exp = pmath_expr_new_extended(
                  pmath_ref(pmath_System_SuperscriptBox), 1,
                  exp);
      }
      else
        exp = ensure_min_precedence(exp, PMATH_PREC_POW, +1);
    }
    else { // exp = 1
      pmath_unref(exp);
      exp = PMATH_NULL;
    }
    
    base = object_to_boxes(thread, base);
    base = enclose_subsuper_base(base);
    
    if(!pmath_is_null(exp)) {
      if(thread->boxform <= BOXFORM_OUTPUT)
        expr = pmath_build_value("(oo)", base, exp);
      else
        expr = pmath_build_value("(oso)", base, "^", exp);
    }
    else
      expr = base;
      
    if(fraction) {
      if( thread->boxform != BOXFORM_STANDARDEXPONENT &&
          thread->boxform != BOXFORM_OUTPUTEXPONENT   &&
          thread->boxform <= BOXFORM_OUTPUT)
      {
        return pmath_expr_new_extended(
                 pmath_ref(pmath_System_FractionBox), 2,
                 PMATH_C_STRING("1"),
                 expr);
      }
      else {
        if(first_char(expr) == '?')
          expr = pmath_build_value("(so)", " ", expr);
        return pmath_build_value("(sso)", "1", "/", expr);
      }
    }
    
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t pureargument_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_integer(item) && pmath_number_sign(item) > 0) {
      pmath_unref(expr);
      return pmath_build_value("(so)", "#", object_to_boxes(thread, item));
    }
    
    if(pmath_is_expr_of_len(item, pmath_System_Range, 2)) {
      pmath_t a = pmath_expr_get_item(item, 1);
      pmath_t b = pmath_expr_get_item(item, 2);
      pmath_unref(b);
      
      if( pmath_same(b, pmath_System_Automatic) &&
          pmath_is_integer(a)                   &&
          pmath_number_sign(a) > 0)
      {
        pmath_unref(item);
        pmath_unref(expr);
        
        return pmath_build_value("(so)", "##", object_to_boxes(thread, a));
      }
      
      pmath_unref(a);
    }
    
    pmath_unref(item);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t range_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) > 1) {
    return nary_to_boxes(
             thread,
             expr,
             PMATH_C_STRING(".."),
             PMATH_PREC_RANGE,
             PMATH_PREC_RANGE + 1,
             TRUE);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t repeated_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  assert(thread != NULL);
  
  if(pmath_expr_length(expr) == 2) {
    pmath_t item = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(item, _pmath_object_range_from_one)) {
      pmath_unref(item);
      
      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      
      if(pmath_equals(item, _pmath_object_singlematch)) {
        pmath_unref(item);
        return PMATH_C_STRING("~~");
      }
      
      if(pmath_is_expr_of_len(item, pmath_System_SingleMatch, 1)) {
        pmath_t type = pmath_expr_get_item(item, 1);
        
        pmath_unref(item);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
        
        return pmath_build_value("(sso)", "~~", ":", type);
      }
      
      item = object_to_boxes(thread, item);
      item = ensure_min_precedence(item, PMATH_PREC_REPEAT + 1, -1);
      
      return pmath_build_value("(os)", item, "**");
    }
    
    if(pmath_equals(item, _pmath_object_range_from_zero)) {
      pmath_unref(item);
      
      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      
      if(pmath_equals(item, _pmath_object_singlematch)) {
        pmath_unref(item);
        return PMATH_C_STRING("~~~");
      }
      
      if(pmath_is_expr_of_len(item, pmath_System_SingleMatch, 1)) {
        pmath_t type = pmath_expr_get_item(item, 1);
        
        pmath_unref(item);
        type = object_to_boxes(thread, type);
        type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
        
        return pmath_build_value("(sso)", "~~~", ":", type);
      }
      
      item = object_to_boxes(thread, item);
      item = ensure_min_precedence(item, PMATH_PREC_REPEAT + 1, -1);
      
      return pmath_build_value("(os)", item, "***");
    }
    
    pmath_unref(item);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t singlematch_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 0) {
    pmath_unref(expr);
    return PMATH_C_STRING("~");
  }
  
  if(pmath_expr_length(expr) == 1) {
    pmath_t type = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    type = object_to_boxes(thread, type);
    type = ensure_min_precedence(type, PMATH_PREC_PRIM, +1);
    
    return pmath_build_value("sso", "~", ":", type);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t subscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) >= 2) {
    pmath_t base;
    pmath_expr_t indices;
    
    base =  pmath_expr_get_item(expr, 1);
    indices = pmath_expr_get_item_range(expr, 2, SIZE_MAX);
    pmath_unref(expr);
    
    base = object_to_boxes(thread, base);
    base = enclose_subsuper_base(base);
    
    indices = pmath_expr_new_extended(
                pmath_ref(pmath_System_SubscriptBox), 1,
                nary_to_boxes(
                  thread,
                  indices,
                  PMATH_C_STRING(","),
                  PMATH_PREC_SEQ + 1,
                  PMATH_PREC_SEQ + 1,
                  TRUE));
                  
    return pmath_build_value("(oo)", base, indices);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t subsuperscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 3) {
    pmath_t base;
    pmath_t sub;
    pmath_t super;
    
    base  = pmath_expr_get_item(expr, 1);
    sub   = pmath_expr_get_item(expr, 2);
    super = pmath_expr_get_item(expr, 3);
    pmath_unref(expr);
    
    base  = object_to_boxes(thread, base);
    sub   = object_to_boxes(thread, sub);
    super = object_to_boxes(thread, super);
    base = enclose_subsuper_base(base);
    
    return pmath_build_value("(oo)",
                             base,
                             pmath_expr_new_extended(
                               pmath_ref(pmath_System_SubsuperscriptBox), 2,
                               sub,
                               super));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t superscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t base;
    pmath_t super;
    
    base  = pmath_expr_get_item(expr, 1);
    super = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    base  = object_to_boxes(thread, base);
    super = object_to_boxes(thread, super);
    base = enclose_subsuper_base(base);
    
    return pmath_build_value("(oo)",
                             base,
                             pmath_expr_new_extended(
                               pmath_ref(pmath_System_SuperscriptBox), 1,
                               super));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t tagassign_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  pmath_t head, op;
  size_t len;
  
  assert(thread != NULL);
  
  len = pmath_expr_length(expr);
  head = pmath_expr_get_item(expr, 0);
  pmath_unref(head);
  
  if(len == 3) {
    op = PMATH_NULL;
    if(thread->boxform < BOXFORM_OUTPUT) {
      if(pmath_same(head, pmath_System_TagAssign))
        op = pmath_build_value("c", PMATH_CHAR_ASSIGN);
      else if(pmath_same(head, pmath_System_TagAssignDelayed))
        op = pmath_build_value("c", PMATH_CHAR_ASSIGNDELAYED);
    }
    else {
      if(pmath_same(head, pmath_System_TagAssign))
        op = PMATH_C_STRING(":=");
      else if(pmath_same(head, pmath_System_TagAssignDelayed))
        op = PMATH_C_STRING("::=");
    }
    
    if(!pmath_is_null(op)) {
      pmath_t item;
      
      pmath_gather_begin(PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 1);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item),
          PMATH_PREC_ASS + 1,
          -1),
        PMATH_NULL);
        
      pmath_emit(PMATH_C_STRING("/:"), PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 2);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item),
          PMATH_PREC_ASS + 1,
          0),
        PMATH_NULL);
        
      pmath_emit(op, PMATH_NULL);
      
      item = pmath_expr_get_item(expr, 3);
      pmath_emit(
        ensure_min_precedence(
          object_to_boxes(thread, item),
          PMATH_PREC_ASS,
          +1),
        PMATH_NULL);
        
      pmath_unref(expr);
      return pmath_gather_end();
    }
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t times_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  size_t len = pmath_expr_length(expr);
  
  if(len >= 2) {
    pmath_t minus = PMATH_NULL;
    pmath_t num, den;
    
    if( thread->boxform != BOXFORM_STANDARDEXPONENT &&
        thread->boxform != BOXFORM_OUTPUTEXPONENT   &&
        thread->boxform <= BOXFORM_OUTPUT)
    {
      pmath_gather_begin(NUM_TAG);
      pmath_gather_begin(DEN_TAG);
      
      emit_num_den(expr); expr = PMATH_NULL;
      
      den = pmath_gather_end();
      num = pmath_gather_end();
    }
    else {
      num = expr;
      expr = PMATH_NULL;
      den = PMATH_NULL;
    }
    
    if(pmath_expr_length(den) == 0) {
      pmath_token_t last_tok;
      pmath_t factor;
      pmath_t prevfactor;
      pmath_bool_t div = FALSE;
      uint16_t ch;
      size_t i;
      size_t numlen = pmath_expr_length(num);
      
      pmath_unref(den);
      pmath_gather_begin(PMATH_NULL);
      
      prevfactor = pmath_expr_get_item(num, 1);
      prevfactor = object_to_boxes(thread, prevfactor);
      
      if(is_minus(prevfactor)) {
        den = prevfactor;
        minus      = pmath_expr_get_item(den, 1);
        prevfactor = pmath_expr_get_item(den, 2);
        pmath_unref(den);
        
        prevfactor = ensure_min_precedence(prevfactor, PMATH_PREC_MUL + 1, -1);
        ch = last_char(prevfactor);
        last_tok = pmath_token_analyse_output(&ch, 1, NULL);
      }
      else {
        prevfactor = ensure_min_precedence(prevfactor, PMATH_PREC_MUL + 1, -1);
        ch = last_char(prevfactor);
        last_tok = pmath_token_analyse_output(&ch, 1, NULL);
      }
      
      for(i = 2; i <= numlen; ++i) {
        factor = pmath_expr_get_item(num, i);
        factor = object_to_boxes(thread, factor);
        factor = ensure_min_precedence(
                   factor,
                   PMATH_PREC_MUL + 1,
                   (i == numlen) ? +1 : 0);
                   
        if(is_inversion(factor)) {
          if(!div) {
            div = TRUE;
            pmath_gather_begin(PMATH_NULL);
          }
          pmath_emit(prevfactor, PMATH_NULL);
          
          den = factor;
          factor = pmath_expr_get_item(den, 3);
          pmath_unref(den);
          
          pmath_emit(PMATH_C_STRING("/"), PMATH_NULL);
        }
        else {
          if(i == 2 && is_char(prevfactor, '1'))
            pmath_unref(prevfactor);
          else
            pmath_emit(prevfactor, PMATH_NULL);
            
          if(div)
            pmath_emit(pmath_gather_end(), PMATH_NULL);
            
          if(last_tok != PMATH_TOK_DIGIT) {
            pmath_emit(PMATH_C_STRING(" "), PMATH_NULL);
          }
          else {
            ch = first_char(factor);
            last_tok = pmath_token_analyse_output(&ch, 1, NULL);
            
            if(last_tok == PMATH_TOK_LEFTCALL)
              pmath_emit(PMATH_C_STRING(" "), PMATH_NULL);
          }
        }
        
        ch = last_char(factor);
        last_tok = pmath_token_analyse_output(&ch, 1, NULL);
        
        prevfactor = factor;
      }
      
      pmath_emit(prevfactor, PMATH_NULL);
      if(div)
        pmath_emit(pmath_gather_end(), PMATH_NULL);
        
      pmath_unref(num);
      expr = pmath_gather_end();
      if(pmath_expr_length(expr) == 1) {
        num = expr;
        expr = pmath_expr_get_item(num, 1);
        pmath_unref(num);
      }
    }
    else {
      pmath_bool_t use_fraction_box;
      
      use_fraction_box = thread->boxform != BOXFORM_STANDARDEXPONENT &&
                         thread->boxform != BOXFORM_OUTPUTEXPONENT   &&
                         thread->boxform <= BOXFORM_OUTPUT;
                         
      if(pmath_expr_length(den) == 1) {
        pmath_t tmp = den;
        den = pmath_expr_get_item(tmp, 1);
        pmath_unref(tmp);
      }
      else
        den = pmath_expr_set_item(den, 0, pmath_ref(pmath_System_Times));
        
      if(pmath_expr_length(num) == 0) {
        pmath_unref(num);
        num = PMATH_FROM_INT32(1);
      }
      else if(pmath_expr_length(num) == 1) {
        pmath_t tmp = num;
        num = pmath_expr_get_item(tmp, 1);
        pmath_unref(tmp);
      }
      else
        num = pmath_expr_set_item(num, 0, pmath_ref(pmath_System_Times));
        
      num = object_to_boxes(thread, num);
      den = object_to_boxes(thread, den);
      
      if(is_minus(num)) {
        if(!is_minus(den)) {
          pmath_t tmp = num;
          minus = pmath_expr_get_item(tmp, 1);
          num = remove_paren(pmath_expr_get_item(tmp, 2));
          pmath_unref(tmp);
        }
      }
      else if(is_minus(den)) {
        pmath_t tmp = den;
        minus = pmath_expr_get_item(tmp, 1);
        den = remove_paren(pmath_expr_get_item(tmp, 2));
        pmath_unref(tmp);
      }
      
      if(use_fraction_box) {
        expr = pmath_expr_new_extended(
                 pmath_ref(pmath_System_FractionBox), 2,
                 num, den);
      }
      else {
        num = ensure_min_precedence(num, PMATH_PREC_DIV, -1);
        den = ensure_min_precedence(den, PMATH_PREC_DIV, +1);
        expr = pmath_build_value("(oco)", num, '/', den);
      }
    }
    
    if(!pmath_is_null(minus))
      return pmath_build_value("(oo)", minus, expr);
      
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for InputForm

//{ boxforms valid for OutputForm ...

static pmath_t baseform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t base = pmath_expr_get_item(expr, 2);
    pmath_t item;
    uint8_t oldbase;
    
    if( !pmath_is_int32(base)    ||
        PMATH_AS_INT32(base) < 2 ||
        PMATH_AS_INT32(base) > 36)
    {
      pmath_unref(base);
      return call_to_boxes(thread, expr);;
    }
    
    oldbase = thread->numberbase;
    thread->numberbase = (uint8_t)PMATH_AS_INT32(base);
    
    item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    item = object_to_boxes(thread, item);
    
    thread->numberbase = oldbase;
    return item;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t column_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr wont be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if( pmath_is_expr_of(list, pmath_System_List) &&
        pmath_expr_length(list) > 0)
    {
      size_t i;
      
      pmath_unref(expr);
      
      for(i = pmath_expr_length(list); i > 0; --i) {
        list = pmath_expr_set_item(list, i,
                                   pmath_expr_new_extended(
                                     pmath_ref(pmath_System_List), 1,
                                     object_to_boxes(
                                       thread,
                                       pmath_expr_get_item(list, i))));
      }
      
      return pmath_expr_new_extended(
               pmath_ref(pmath_System_TagBox), 2,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_GridBox), 1,
                 list),
               PMATH_C_STRING("Column"));
    }
    
    pmath_unref(list);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t derivative_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t head = pmath_expr_get_item(expr, 0);
    pmath_t func = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(head, pmath_System_Derivative)) {
      size_t headlen = pmath_expr_length(head);
      pmath_bool_t need_tag = TRUE;
      
      func = object_to_boxes(thread, func);
      func = enclose_subsuper_base(func);
      
      if(headlen == 1) {
        const uint16_t single_prime_char = 0x2032;
        const uint16_t double_prime_char = 0x2033;
        const uint16_t triple_prime_char = 0x2034;
        
        pmath_t order = pmath_expr_get_item(head, 1);
        pmath_unref(head);
          
        if(pmath_same(order, PMATH_FROM_INT32(1))) {
          head = pmath_string_insert_ucs2(PMATH_NULL, 0, &single_prime_char, 1);
          need_tag = FALSE;
        }
        else if(pmath_same(order, PMATH_FROM_INT32(2))) {
          head = pmath_string_insert_ucs2(PMATH_NULL, 0, &double_prime_char, 1);
          need_tag = FALSE;
        }
        else if(pmath_same(order, PMATH_FROM_INT32(3))) {
          head = pmath_string_insert_ucs2(PMATH_NULL, 0, &triple_prime_char, 1);
          need_tag = FALSE;
        }
        else {
          head = object_to_boxes(thread, order);
          need_tag = TRUE;
        }
      }
      else {
        head = nary_to_boxes(
                 thread,
                 head,
                 PMATH_C_STRING(","),
                 PMATH_PREC_SEQ + 1,
                 PMATH_PREC_SEQ + 1,
                 TRUE);
        need_tag = TRUE;
      }
      
      if(need_tag) {
        head = pmath_expr_new_extended(
                 pmath_ref(pmath_System_List), 3,
                 PMATH_C_STRING("("),
                 head,
                 PMATH_C_STRING(")"));
        head = pmath_expr_new_extended(
                 pmath_ref(pmath_System_TagBox), 2,
                 head,
                 pmath_ref(pmath_System_Derivative));
      }
      
      pmath_unref(expr);
      return pmath_build_value("(oo)",
                         func,
                         pmath_expr_new_extended(
                           pmath_ref(pmath_System_SuperscriptBox), 1,
                           head));
    }
    
    pmath_unref(head);
    pmath_unref(func);
  }
  
  return call_to_boxes(thread, expr);
}

static void emit_gridbox_options(pmath_expr_t expr, size_t start) { // expr wont be freed
  size_t i;
  
  for(i = start; i <= pmath_expr_length(expr); ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_is_rule(item)) {
      pmath_t lhs = pmath_expr_get_item(item, 1);
      pmath_unref(lhs);
      
      if(pmath_same(lhs, pmath_System_ColumnSpacing)) {
        item = pmath_expr_set_item(item, 1, pmath_ref(pmath_System_GridBoxColumnSpacing));
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      if(pmath_same(lhs, pmath_System_RowSpacing)) {
        item = pmath_expr_set_item(item, 1, pmath_ref(pmath_System_GridBoxRowSpacing));
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      pmath_emit(item, PMATH_NULL);
      continue;
    }
    
    if(pmath_is_expr_of(item, pmath_System_List)) {
      emit_gridbox_options(item, 1);
      pmath_unref(item);
      continue;
    }
    
    pmath_unref(item);
  }
}

static pmath_t grid_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) >= 1) {
    pmath_expr_t obj = pmath_options_extract(expr, 1);
    
    if(!pmath_is_null(obj)) {
      size_t rows, cols;
      
      pmath_unref(obj);
      obj = pmath_expr_get_item(expr, 1);
      
      if( _pmath_is_matrix(obj, &rows, &cols, FALSE) &&
          rows > 0 &&
          cols > 0)
      {
        size_t i, j;
        
        expr = pmath_expr_set_item(expr, 1, PMATH_NULL);
        
        for(i = 1; i <= rows; ++i) {
          pmath_t row = pmath_expr_get_item(obj, i);
          obj = pmath_expr_set_item(obj, i, PMATH_NULL);
          
          for(j = 1; j <= cols; ++j) {
            pmath_t item = pmath_expr_get_item(row, j);
            
            item = object_to_boxes_or_empty(thread, item);
            
            row = pmath_expr_set_item(row, j, item);
          }
          
          obj = pmath_expr_set_item(obj, i, row);
        }
        
        pmath_gather_begin(PMATH_NULL);
        
        pmath_emit(obj, PMATH_NULL);
        emit_gridbox_options(expr, 2);
        
        pmath_unref(expr);
        expr = pmath_gather_end();
        expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_GridBox));
        
        return pmath_expr_new_extended(
                 pmath_ref(pmath_System_TagBox), 2,
                 expr,
                 PMATH_C_STRING("Grid"));
      }
      
      pmath_unref(obj);
    }
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t fullform(pmath_thread_t thread, pmath_t obj) { // obj will be freed
  if(pmath_is_expr(obj)) {
    pmath_expr_t result;
    size_t len;
    len = pmath_expr_length(obj);
    
    if(len > 0) {
      pmath_t comma;
      size_t i;
      
      comma = PMATH_C_STRING(",");
      
      result = pmath_expr_new(pmath_ref(pmath_System_List), 2 * len - 1);
      
      result = pmath_expr_set_item(
                 result, 1,
                 fullform(
                   thread,
                   pmath_expr_get_item(obj, 1)));
                   
      for(i = 2; i <= len; ++i) {
        result = pmath_expr_set_item(
                   result, 2 * i - 2,
                   pmath_ref(comma));
                   
        result = pmath_expr_set_item(
                   result, 2 * i - 1,
                   fullform(
                     thread,
                     pmath_expr_get_item(obj, i)));
      }
      
      pmath_unref(comma);
      
      result = pmath_build_value("osos",
                                 fullform(
                                   thread,
                                   pmath_expr_get_item(obj, 0)),
                                 "(",
                                 result,
                                 ")");
    }
    else {
      result = pmath_build_value("oss",
                                 fullform(
                                   thread,
                                   pmath_expr_get_item(obj, 0)),
                                 "(",
                                 ")");
    }
    
    pmath_unref(obj);
    return result;
  }
  
  return object_to_boxes(thread, obj);
}

static pmath_t fullform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_INPUT;
    
    result = fullform(
               thread,
               pmath_expr_get_item(expr, 1));
               
    thread->boxform = old_boxform;
    
    pmath_unref(expr);
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_StyleBox), 3,
             result,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_AutoDelete),
               pmath_ref(pmath_System_True)),
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_ShowStringCharacters),
               pmath_ref(pmath_System_True)));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t holdform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t obj = pmath_expr_get_item(expr, 1);
    
    pmath_unref(expr);
    
    return object_to_boxes(thread, obj);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t inputform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_INPUT;
    
    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));
    
    thread->boxform = old_boxform;
    
    pmath_unref(expr);
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_StyleBox), 4,
             result,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_AutoDelete),
               pmath_ref(pmath_System_True)),
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_AutoNumberFormating),
               pmath_ref(pmath_System_False)),
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_ShowStringCharacters),
               pmath_ref(pmath_System_True)));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t strip_interpretation_boxes(pmath_t expr) {
  if(pmath_is_expr(expr)) {
    size_t first_box;
    size_t last_box;
    size_t len = pmath_expr_length(expr);
    pmath_t head = pmath_expr_get_item(expr, 0);
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_InterpretationBox) && len == 2) {
      pmath_t box = pmath_expr_extract_item(expr, 1);
      pmath_unref(expr);
      return strip_interpretation_boxes(box);
      //return pmath_expr_set_item(expr, 2, PMATH_NULL);
    }
    
    // TODO: PaneSelectorBox, TemplateBox
    
    if(      pmath_same(head, pmath_System_List)              || 
             pmath_same(head, pmath_System_StringBox)         || 
             pmath_same(head, PMATH_NULL))
    {
      first_box = 1;
      last_box = len;
    }
    else if( pmath_same(head, pmath_System_ButtonBox)         ||
             pmath_same(head, pmath_System_FillBox)           ||
             pmath_same(head, pmath_System_FrameBox)          ||
             pmath_same(head, pmath_System_GridBox)           ||
             pmath_same(head, pmath_System_PaneBox)           ||
             pmath_same(head, pmath_System_PanelBox)          ||
             pmath_same(head, pmath_System_RotationBox)       ||
             pmath_same(head, pmath_System_SqrtBox)           ||
             pmath_same(head, pmath_System_SubscriptBox)      ||
             pmath_same(head, pmath_System_SuperscriptBox)    ||
             pmath_same(head, pmath_System_TagBox)            ||
             pmath_same(head, pmath_System_TransformationBox))
    {
      first_box = 1;
      last_box = 1;
    }
    else if( pmath_same(head, pmath_System_OverscriptBox)     ||
             pmath_same(head, pmath_System_RadicalBox)        ||
             pmath_same(head, pmath_System_SubsuperscriptBox) ||
             pmath_same(head, pmath_System_TooltipBox)        ||
             pmath_same(head, pmath_System_UnderscriptBox))
    {
      first_box = 1;
      last_box = 2;
    }
    else if( pmath_same(head, pmath_System_UnderoverscriptBox)) {
      first_box = 1;
      last_box = 3;
    }
    else if( pmath_same(head, pmath_System_SetterBox)) {
      first_box = 3;
      last_box = 3;
    }
    else {
      first_box = 1;
      last_box = 0;
    }
    
    for(; first_box <= last_box; ++first_box) {
      pmath_t box = pmath_expr_extract_item(expr, first_box);
      box = strip_interpretation_boxes(box);
      expr = pmath_expr_set_item(expr, first_box, box);
    }
  }
  return expr;
}

static pmath_t interpretation_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t form  = pmath_expr_get_item(expr, 1);
    pmath_t value = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    form = object_to_boxes(thread, form);
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_InterpretationBox), 2,
             strip_interpretation_boxes(form),
             value);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t linearsolvefunction_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t skeleton = pmath_expr_new_extended(
                         pmath_ref(pmath_System_Skeleton), 1,
                         PMATH_FROM_INT32(1));
                         
    expr = pmath_expr_set_item(expr, 2, skeleton);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t longform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t obj = pmath_expr_get_item(expr, 1);
    pmath_bool_t old_longform = thread->longform;
    thread->longform = TRUE;
    
    pmath_unref(expr);
    expr = object_to_boxes(thread, obj);
    
    thread->longform = old_longform;
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t matrixform(pmath_thread_t thread, pmath_t obj) { // obj will be freed
  size_t rows, cols;
  
  if(_pmath_is_matrix(obj, &rows, &cols, FALSE) && rows > 0 && cols > 0) {
    size_t i, j;
    
    for(i = 1; i <= rows; ++i) {
      pmath_t row = pmath_expr_extract_item(obj, i);
      
      for(j = 1; j <= cols; ++j) {
        pmath_t item = pmath_expr_extract_item(row, j);
        
        item = matrixform(thread, item);
        
        row = pmath_expr_set_item(row, j, item);
      }
      
      obj = pmath_expr_set_item(obj, i, row);
    }
    
    obj = pmath_expr_new_extended(
            pmath_ref(pmath_System_GridBox), 1,
            obj);
            
    return pmath_build_value("sos", "(", obj, ")");
  }
  
  if(pmath_is_expr_of(obj, pmath_System_List)) {
    size_t i;
    
    rows = pmath_expr_length(obj);
    
    for(i = 1; i <= rows; ++i) {
      pmath_t item = pmath_expr_extract_item(obj, i);
      
      item = object_to_boxes(thread, item);
      item = pmath_build_value("(o)", item);
      
      obj = pmath_expr_set_item(obj, i, item);
    }
    
    obj = pmath_expr_new_extended(
            pmath_ref(pmath_System_GridBox), 1,
            obj);
            
    obj = pmath_expr_new_extended(
            pmath_ref(pmath_System_TagBox), 2,
            obj,
            pmath_ref(pmath_System_Column));
            
    return pmath_build_value("sos", "(", obj, ")");
  }
  
  return object_to_boxes(thread, obj);
}

static pmath_t matrixform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t obj;
    
    obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    return matrixform(thread, obj);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t outputform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_OUTPUT;
    
    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));
    
    thread->boxform = old_boxform;
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_InterpretationBox), 2,
             strip_interpretation_boxes(result),
             expr);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t packedarrayform_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_t obj = pmath_expr_get_item(expr, 1);
    pmath_bool_t old_paf = thread->use_packedarrayform_boxes;
    thread->use_packedarrayform_boxes = TRUE;
    
    pmath_unref(expr);
    expr = object_to_boxes(thread, obj);
    
    thread->use_packedarrayform_boxes = old_paf;
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t row_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  size_t len = pmath_expr_length(expr);
  
  assert(thread != NULL);
  
  if(len == 1) {
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(list, pmath_System_List)) {
      size_t i;
      
      pmath_unref(expr); 
      expr = PMATH_NULL;
      
      pmath_gather_begin(PMATH_NULL);
      
      for(i = 1; i <= pmath_expr_length(list); ++i) {
        pmath_t item = pmath_expr_get_item(list, i);
        item = object_to_boxes(thread, item);
        pmath_emit(item, PMATH_NULL);
      }
      
      pmath_unref(list);
      list = pmath_gather_end();
      list = pmath_expr_new_extended(
               pmath_ref(pmath_System_TemplateBox), 2,
               list,
               PMATH_C_STRING("RowDefault"));
      return list;
    }
    
    pmath_unref(list);
  }
  else if(len == 2) {
    pmath_t list = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_expr_of(list, pmath_System_List)) {
      pmath_t delim = pmath_expr_get_item(expr, 2);
      size_t i;
      
      pmath_unref(expr); 
      expr = PMATH_NULL;
      
      pmath_gather_begin(PMATH_NULL);
      
      if(pmath_is_string(delim))
        pmath_emit(pmath_ref(delim), PMATH_NULL);
      else
        pmath_emit(object_to_boxes(thread, delim), PMATH_NULL);
      
      pmath_emit(delim, PMATH_NULL);
      for(i = 1; i <= pmath_expr_length(list); ++i) {
        pmath_t item = pmath_expr_get_item(list, i);
        item = object_to_boxes(thread, item);
        pmath_emit(item, PMATH_NULL);
      }
      
      pmath_unref(list);
      list = pmath_gather_end();
      list = pmath_expr_new_extended(
               pmath_ref(pmath_System_TemplateBox), 2,
               list,
               PMATH_C_STRING("RowWithSeparators"));
      return list;
    }
    pmath_unref(list);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t shallow_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  pmath_t item;
  size_t maxdepth = 4;
  size_t maxlength = 10;
  
  if(pmath_expr_length(expr) == 2) {
    item = pmath_expr_get_item(expr, 2);
    
    if(pmath_is_int32(item) && PMATH_AS_INT32(item) >= 0) {
      maxdepth = (size_t)PMATH_AS_INT32(item);
    }
    else if(pmath_equals(item, _pmath_object_pos_infinity)) {
      maxdepth = SIZE_MAX;
    }
    else if(pmath_is_expr_of_len(item, pmath_System_List, 2)) {
      // {maxdepth, maxlength}
      pmath_t obj = pmath_expr_get_item(item, 1);
      
      if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
        maxdepth = (size_t)PMATH_AS_INT32(obj);
      }
      else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
        maxdepth = SIZE_MAX;
      }
      else {
        pmath_unref(obj);
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
      
      pmath_unref(obj);
      obj = pmath_expr_get_item(item, 2);
      
      if(pmath_is_int32(obj) && PMATH_AS_INT32(obj) >= 0) {
        maxlength = (size_t)PMATH_AS_INT32(obj);
      }
      else if(pmath_equals(obj, _pmath_object_pos_infinity)) {
        maxlength = SIZE_MAX;
      }
      else {
        pmath_unref(obj);
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
      
      pmath_unref(obj);
    }
    else {
      pmath_unref(item);
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(item);
  }
  else if(pmath_expr_length(expr) != 1)
    return call_to_boxes(thread, expr);
    
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  item = _pmath_prepare_shallow(item, maxdepth, maxlength);
  
  return object_to_boxes(thread, item);
}

static pmath_t short_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  pmath_t item;
  double pagewidth = 72;
  double lines = 1;
  
  if(pmath_expr_length(expr) == 2) {
    item = pmath_expr_get_item(expr, 2);
    
    if(pmath_equals(item, _pmath_object_pos_infinity)) {
      pmath_unref(item);
      item = pmath_expr_get_item(expr, 1);
      pmath_unref(expr);
      return object_to_boxes(thread, item);
    }
    
    if(pmath_is_number(item)) {
      lines = pmath_number_get_d(item);
      
      if(lines < 0) {
        pmath_unref(item);
        return call_to_boxes(thread, expr);
      }
    }
    else {
      pmath_unref(item);
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(item);
  }
  else if(pmath_expr_length(expr) != 1)
    return call_to_boxes(thread, expr);
    
  item = pmath_evaluate(pmath_ref(pmath_System_DollarPageWidth));
  if(pmath_equals(item, _pmath_object_pos_infinity)) {
    pmath_unref(item);
    item = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    return object_to_boxes(thread, item);
  }
  
  if(pmath_is_number(item)) {
    pagewidth = pmath_number_get_d(item);
  }
  
  pmath_unref(item);
  
  pagewidth *= lines;
  if(pagewidth < 1 || pagewidth > 1000000)
    return call_to_boxes(thread, expr);
    
  item = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  item = object_to_boxes(thread, item);
  return _pmath_shorten_boxes(item, (long)pagewidth);
}

static pmath_t skeleton_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 1) {
    pmath_expr_t obj = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    if(thread->boxform < BOXFORM_OUTPUT) {
      return pmath_build_value("(ccocc)", 0x00AB, 0x00A0, object_to_boxes(thread, obj), 0x00A0, 0x00BB);
    }
    
    return pmath_build_value("(sos)", "<<", object_to_boxes(thread, obj), ">>");
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_bool_t emit_stylebox_options(pmath_expr_t expr, size_t start, struct emit_stylebox_options_info_t *info) { // expr wont be freed
  size_t i;
  
  for(i = start; i <= pmath_expr_length(expr); ++i) {
    pmath_t item = pmath_expr_get_item(expr, i);
    
    if(pmath_same(item, pmath_System_Bold)) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontWeight),
          item),
        PMATH_NULL);
      continue;
    }
    
    if(pmath_same(item, pmath_System_Italic)) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontSlant),
          item),
        PMATH_NULL);
      continue;
    }
    
    if( pmath_same(item, pmath_System_Large) || 
        pmath_same(item, pmath_System_Medium) ||
        pmath_same(item, pmath_System_Small) ||
        pmath_same(item, pmath_System_Tiny)
    ) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontSize),
          item),
        PMATH_NULL);
      continue;
    }
    
    if(pmath_same(item, pmath_System_Plain)) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontSlant),
          pmath_ref(item)),
        PMATH_NULL);
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontWeight),
          item),
        PMATH_NULL);
      continue;
    }
    
    if(pmath_is_number(item)) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontSize),
          item),
        PMATH_NULL);
      continue;
    }
    
    if(pmath_is_expr_of(item, pmath_System_List)) {
      if(!emit_stylebox_options(expr, 1, info)) {
        pmath_unref(item);
        return FALSE;
      }
      
      pmath_unref(item);
      continue;
    }
    
    if( pmath_is_expr_of_len(item, pmath_System_GrayLevel, 1) ||
        pmath_is_expr_of_len(item, pmath_System_RGBColor, 3))
    {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_FontColor),
          item),
        PMATH_NULL);
      continue;
    }
    
    if(pmath_is_rule(item)) {
      pmath_t lhs = pmath_expr_get_item(item, 1);
      pmath_unref(lhs);
      
      if(!info->have_explicit_strip_on_input) {
        info->have_explicit_strip_on_input = pmath_same(lhs, pmath_System_StripOnInput);
      }
      
      pmath_emit(item, PMATH_NULL);
      continue;
    }
    
    if(pmath_is_string(item) && i == start && i > 1) {
      pmath_emit(item, PMATH_NULL);
      continue;
    }
    
    pmath_unref(item);
    return FALSE;
  }
  
  return TRUE;
}

static pmath_t style_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  size_t len = pmath_expr_length(expr);
  
  if(len >= 1) {
    struct emit_stylebox_options_info_t info;
    memset(&info, 0, sizeof(info));
    
    pmath_gather_begin(PMATH_NULL);
    pmath_emit(object_to_boxes(thread, pmath_expr_get_item(expr, 1)), PMATH_NULL);
    
    if(!emit_stylebox_options(expr, 2, &info)) {
      pmath_unref(pmath_gather_end());
      
      return call_to_boxes(thread, expr);
    }
    
    pmath_unref(expr);
    
    if(!info.have_explicit_strip_on_input) {
      pmath_emit(
        pmath_expr_new_extended(
          pmath_ref(pmath_System_Rule), 2,
          pmath_ref(pmath_System_StripOnInput),
          pmath_ref(pmath_System_False)),
        PMATH_NULL);
    }
    
    expr = pmath_gather_end();
    return pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_StyleBox));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t underscript_or_overscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 2) {
    pmath_t head = pmath_expr_get_item(expr, 0);
    pmath_t base = pmath_expr_get_item(expr, 1);
    pmath_t uo   = pmath_expr_get_item(expr, 2);
    
    pmath_unref(head);
    pmath_unref(expr);
    
    base = object_to_boxes(thread, base);
    uo   = object_to_boxes(thread, uo);
    
    if(pmath_same(head, pmath_System_Underscript))
      head = pmath_ref(pmath_System_UnderscriptBox);
    else
      head = pmath_ref(pmath_System_OverscriptBox);
      
    return pmath_expr_new_extended(
             head, 2,
             base,
             uo);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t underoverscript_to_boxes(pmath_thread_t thread, pmath_expr_t expr) { // expr will be freed
  if(pmath_expr_length(expr) == 3) {
    pmath_t base  = pmath_expr_get_item(expr, 1);
    pmath_t under = pmath_expr_get_item(expr, 2);
    pmath_t over  = pmath_expr_get_item(expr, 3);
    
    pmath_unref(expr);
    
    base  = object_to_boxes(thread, base);
    under = object_to_boxes(thread, under);
    over  = object_to_boxes(thread, over);
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_UnderoverscriptBox), 3,
             base,
             under,
             over);
  }
  
  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for OutputForm

//{ boxforms valid for StandardForm ...

static pmath_t framed_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  if(pmath_expr_length(expr) >= 1) {
    pmath_t item = pmath_expr_extract_item(expr, 1);
    
    item = object_to_boxes(thread, item);
    
    expr = pmath_expr_set_item(expr, 1, item);
    expr = pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_FrameBox));
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t invisible_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  if(pmath_expr_length(expr) == 1) {
    pmath_t item = pmath_expr_extract_item(expr, 1);
    pmath_unref(expr);
    
    item = object_to_boxes(thread, item);
    
    expr = pmath_expr_new_extended(
             pmath_ref(pmath_System_StyleBox), 3,
             item,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2, 
               pmath_ref(pmath_System_ShowContents), 
               pmath_ref(pmath_System_False)),
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2, 
               pmath_ref(pmath_System_StripOnInput), 
               pmath_ref(pmath_System_False)));
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t piecewise_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen >= 1 && exprlen <= 2) {
    size_t rows, cols;
    pmath_expr_t mat = pmath_expr_get_item(expr, 1);
    
    if( _pmath_is_matrix(mat, &rows, &cols, FALSE) &&
        cols == 2)
    {
      size_t i, j;
      
      if(exprlen == 2) {
        pmath_t def = pmath_expr_get_item(expr, 2);
        pmath_unref(expr);
        
        def = object_to_boxes(thread, def);
        
        def = pmath_build_value("os", def, "True");
        
        mat = pmath_expr_append(mat, 1, def);
      }
      else
        pmath_unref(expr);
        
      for(i = 1; i <= rows; ++i) {
        pmath_t row = pmath_expr_get_item(mat, i);
        mat = pmath_expr_set_item(mat, i, PMATH_NULL);
        
        for(j = 1; j <= cols; ++j) {
          pmath_t item = pmath_expr_get_item(row, j);
          
          item = object_to_boxes(thread, item);
          
          row = pmath_expr_set_item(row, j, item);
        }
        
        mat = pmath_expr_set_item(mat, i, row);
      }
      
      return pmath_build_value("co",
                               PMATH_CHAR_PIECEWISE,
                               pmath_expr_new_extended(
                                 pmath_ref(pmath_System_GridBox), 1,
                                 mat));
    }
    
    pmath_unref(mat);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t rotate_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  if(pmath_expr_length(expr) == 2) {
    pmath_t angle, obj;
    
    obj = pmath_expr_get_item(expr, 1);
    obj = object_to_boxes(thread, obj);
    
    angle = pmath_expr_get_item(expr, 2);
    pmath_unref(expr);
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_RotationBox), 2,
             obj,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Rule), 2,
               pmath_ref(pmath_System_BoxRotation),
               angle));
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t standardform_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  if(pmath_expr_length(expr) == 1) {
    pmath_t result;
    uint8_t old_boxform = thread->boxform;
    thread->boxform = BOXFORM_STANDARD;
    
    result = object_to_boxes(thread, pmath_expr_get_item(expr, 1));
    
    thread->boxform = old_boxform;
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_InterpretationBox), 2,
             result,
             expr);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t switch_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  size_t exprlen = pmath_expr_length(expr);
  
  if((exprlen & 1) == 1) {
    pmath_t item;
    size_t i;
    
    pmath_gather_begin(PMATH_NULL);
    
    item = pmath_expr_get_item(expr, 0);
    pmath_emit(
      ensure_min_precedence(
        object_to_boxes(thread, item),
        PMATH_PREC_CALL,
        -1),
      PMATH_NULL);
      
    pmath_emit(PMATH_C_STRING("("), PMATH_NULL);
    
    {
      pmath_gather_begin(PMATH_NULL);
      
      for(i = 1; i <= exprlen; ++i) {
        if(i > 1) {
          pmath_emit(PMATH_C_STRING(","), PMATH_NULL);
          
          if((i & 1) == 0)
            pmath_emit(PMATH_C_STRING("\n"), PMATH_NULL);
        }
        
        item = pmath_expr_get_item(expr, i);
        pmath_emit(
          ensure_min_precedence(
            object_to_boxes(thread, item),
            PMATH_PREC_SEQ + 1,
            -1),
          PMATH_NULL);
      }
      
      item = pmath_gather_end();
      pmath_emit(item, PMATH_NULL);
    }
    
    pmath_emit(PMATH_C_STRING(")"), PMATH_NULL);
    
    pmath_unref(expr);
    expr = pmath_gather_end();
    return expr;
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t placeholder_to_boxes(
  pmath_thread_t thread,
  pmath_expr_t   expr    // will be freed
) {
  if(pmath_expr_length(expr) == 0) {
    pmath_unref(expr);
    
    return pmath_build_value("c", PMATH_CHAR_PLACEHOLDER);
  }
  
  if(pmath_expr_length(expr) == 1) {
    pmath_t description = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    
    description = object_to_boxes(thread, description);
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_TagBox), 2,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_FrameBox), 1,
               description),
             PMATH_C_STRING("Placeholder"));
  }
  
  return call_to_boxes(thread, expr);
}

//} ... boxforms valid for StandardForm

static pmath_t expr_to_boxes(pmath_thread_t thread, pmath_expr_t expr) {
  pmath_t head = pmath_expr_get_item(expr, 0);
  size_t  len  = pmath_expr_length(expr);
  
  if(pmath_is_symbol(head)) {
    if(len == 1) {
      pmath_t op;
      int prec;
      
      op = simple_prefix(head, &prec, thread->boxform);
      if(!pmath_is_null(op)) {
        pmath_unref(head);
        head = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        head = ensure_min_precedence(object_to_boxes(thread, head), prec, +1);
        return pmath_build_value("(oo)", op, head);
      }
      
      op = simple_postfix(head, &prec, thread->boxform);
      if(!pmath_is_null(op)) {
        pmath_unref(head);
        head = pmath_expr_get_item(expr, 1);
        pmath_unref(expr);
        head = ensure_min_precedence(object_to_boxes(thread, head), prec, -1);
        return pmath_build_value("(oo)", head, op);
      }
    }
    else if(len == 2) {
      pmath_t op;
      int prec1, prec2;
      
      op = simple_binary(head, &prec1, &prec2, thread->boxform);
      if(!pmath_is_null(op)) {
        pmath_unref(head);
        return nary_to_boxes(
                 thread,
                 expr,
                 op,
                 prec1,
                 prec2,
                 FALSE);
      }
    }
    else if(len >= 3) {
      pmath_t op;
      int prec;
      
      op = simple_nary(head, &prec, thread->boxform);
      if(!pmath_is_null(op)) {
        pmath_unref(head);
        return nary_to_boxes(
                 thread,
                 expr,
                 op,
                 prec + 1,
                 prec + 1,
                 FALSE);
      }
    }
    
    pmath_unref(head);
    
    if(pmath_same(head, pmath_System_Complex))
      return complex_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_DirectedInfinity))
      return directedinfinity_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_EvaluationSequence))
      return evaluationsequence_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Inequation))
      return inequation_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_List))
      return list_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Optional))
      return optional_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Pattern))
      return pattern_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Plus))
      return plus_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Power))
      return power_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_PureArgument))
      return pureargument_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Range))
      return range_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_Repeated))
      return repeated_to_boxes(thread, expr);
      
    if(pmath_same(head, pmath_System_SingleMatch))
      return singlematch_to_boxes(thread, expr);
      
    if( pmath_same(head, pmath_System_TagAssign) ||
        pmath_same(head, pmath_System_TagAssignDelayed))
    {
      return tagassign_to_boxes(thread, expr);
    }
    
    if(pmath_same(head, pmath_System_Times))
      return times_to_boxes(thread, expr);
      
    /*------------------------------------------------------------------------*/
    if(thread->boxform < BOXFORM_INPUT) {
      if(pmath_same(head, pmath_System_BaseForm))
        return baseform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Column))
        return column_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_FullForm))
        return fullform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Graphics)) {
        expr = pmath_expr_new_extended(
                 pmath_ref(pmath_System_Interpretation), 2,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_Skeleton), 1,
                   pmath_ref(pmath_System_Graphics)),
                 expr);
                 
        return expr_to_boxes(thread, expr);
      }
      
      if(pmath_same(head, pmath_System_Grid))
        return grid_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_HoldForm))
        return holdform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_InputForm))
        return inputform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Interpretation))
        return interpretation_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_LinearSolveFunction))
        return linearsolvefunction_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_LongForm))
        return longform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_MatrixForm))
        return matrixform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_OutputForm))
        return outputform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_Developer_PackedArrayForm))
        return packedarrayform_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Row))
        return row_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Shallow))
        return shallow_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Short))
        return short_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Skeleton))
        return skeleton_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_StringForm)) {
        pmath_t res = _pmath_stringform_to_boxes(thread, expr);
        
        if(!pmath_is_null(res)) {
          pmath_unref(expr);
          return res;
        }
        
        return call_to_boxes(thread, expr);
      }
      
      if(pmath_same(head, pmath_System_Subscript))
        return subscript_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Subsuperscript))
        return subsuperscript_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Superscript))
        return superscript_to_boxes(thread, expr);
        
      if(pmath_same(head, pmath_System_Style))
        return style_to_boxes(thread, expr);
        
      if( pmath_same(head, pmath_System_Underscript) ||
          pmath_same(head, pmath_System_Overscript))
      {
        return underscript_or_overscript_to_boxes(thread, expr);
      }
      
      if(pmath_same(head, pmath_System_Underoverscript))
        return underoverscript_to_boxes(thread, expr);
        
      /*----------------------------------------------------------------------*/
      if(thread->boxform < BOXFORM_OUTPUT) {
        if(pmath_same(head, pmath_System_Framed))
          return framed_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_Invisible))
          return invisible_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_Piecewise))
          return piecewise_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_Rotate))
          return rotate_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_StandardForm))
          return standardform_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_Switch))
          return switch_to_boxes(thread, expr);
          
        if(pmath_same(head, pmath_System_Placeholder))
          return placeholder_to_boxes(thread, expr);
      }
    }
  }
  else {
    if(thread->boxform < BOXFORM_INPUT) {
      if(pmath_is_expr_of(head, pmath_System_Derivative)) {
        pmath_unref(head);
        return derivative_to_boxes(thread, expr);
      }
    }
    
    pmath_unref(head);
  }
  
  return call_to_boxes(thread, expr);
}

static pmath_t packed_array_to_boxes(pmath_thread_t thread, pmath_packed_array_t packed_array) {
  assert(pmath_is_packed_array(packed_array));
  
  if(thread->use_packedarrayform_boxes) {
    pmath_t obj = _pmath_packed_array_form(packed_array);
    pmath_unref(packed_array);
    
    return expr_to_boxes(thread, obj);
  }
  
  return expr_to_boxes(thread, packed_array);
}

static pmath_bool_t user_make_boxes(pmath_t *obj) {
  if(pmath_is_symbol(*obj) || pmath_is_expr(*obj)) {
    pmath_symbol_t head = _pmath_topmost_symbol(*obj);
    
    if(!pmath_is_null(head)) {
      struct _pmath_symbol_rules_t *rules;
      
      rules = _pmath_symbol_get_rules(head, RULES_READ);
      
      if(rules) {
        pmath_t debug_metadata = pmath_get_debug_metadata(*obj);
        pmath_t result     = pmath_expr_new_extended(
                               pmath_ref(pmath_System_MakeBoxes), 1,
                               pmath_ref(*obj));
                               
        if(_pmath_rulecache_find(&rules->format_rules, &result)) {
          pmath_unref(head);
          pmath_unref(*obj);
          *obj = pmath_evaluate(result);
          *obj = pmath_try_set_debug_metadata(*obj, debug_metadata);
          return TRUE;
        }
        
        pmath_unref(result);
        pmath_unref(debug_metadata);
      }
      
      pmath_unref(head);
    }
  }
  
  return FALSE;
}

static pmath_t string_to_stringbox(pmath_thread_t thread, pmath_t obj) {
  size_t i, len;
  pmath_t part;
  
  obj = pmath_string_expand_boxes(obj);
  
  if(pmath_is_string(obj)) {
    obj = _pmath_escape_string(
            PMATH_C_STRING("\""),
            obj,
            PMATH_C_STRING("\""),
            thread->boxform >= BOXFORM_INPUT);
            
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_StringBox), 1,
             obj);
  }
  
  len = pmath_expr_length(obj);
  for(i = len; i > 0; --i) {
    part = pmath_expr_extract_item(obj, i);
    
    if(pmath_is_string(part)) {
      part = _pmath_escape_string(
               PMATH_NULL,
               part,
               PMATH_NULL,
               thread->boxform >= BOXFORM_INPUT);
    }
    
    obj = pmath_expr_set_item(obj, i, part);
  }
  
  part = pmath_expr_extract_item(obj, len);
  if(pmath_is_string(part)) {
    part = pmath_string_insert_latin1(part, INT_MAX, "\"", 1);
    obj = pmath_expr_set_item(obj, len, part);
  }
  else {
    obj = pmath_expr_set_item(obj, len, part);
    obj = pmath_expr_append(obj, 1, PMATH_C_STRING("\""));
  }
  
  
  part = pmath_expr_extract_item(obj, 1);
  if(pmath_is_string(part)) {
    part = pmath_string_insert_latin1(part, 0, "\"", 1);
    obj = pmath_expr_set_item(obj, 1, part);
  }
  else {
    pmath_t tmp;
    obj = pmath_expr_set_item(obj, 1, part);
    tmp = pmath_expr_set_item(obj, 0, PMATH_C_STRING("\""));
    obj = pmath_expr_get_item_range(tmp, 0, len + 1);
    pmath_unref(tmp);
  }
  
  obj = pmath_expr_set_item(obj, 0, pmath_ref(pmath_System_StringBox));
  
  return obj;
}

static pmath_t object_to_boxes_or_empty(pmath_thread_t thread, pmath_t obj) {
  if(pmath_same(obj, PMATH_NULL))
    return PMATH_C_STRING("");
  
  return object_to_boxes(thread, obj);
}

static pmath_t object_to_boxes(pmath_thread_t thread, pmath_t obj) {
  if(pmath_is_double(obj) || pmath_is_int32(obj)) {
    pmath_string_t s = PMATH_NULL;
    pmath_write(
      obj,
      PMATH_WRITE_OPTIONS_INPUTEXPR,
      _pmath_write_to_string,
      &s);
    pmath_unref(obj);
    
    if( pmath_string_length(s) > 0 &&
        *pmath_string_buffer(&s) == '-')
    {
      pmath_string_t minus = pmath_string_part(pmath_ref(s), 0, 1);
      return pmath_build_value("(oo)", minus, pmath_string_part(s, 1, -1));
    }
    
    return s;
  }
  
  if(pmath_is_ministr(obj)) {
    return string_to_stringbox(thread, obj);
//    pmath_string_t quote = PMATH_C_STRING("\"");
//
//    obj = _pmath_string_escape(
//            pmath_ref(quote),
//            obj,
//            pmath_ref(quote),
//            FALSE/*thread->boxform >= BOXFORM_INPUT*/);
//
//    pmath_unref(quote);
//    return obj;
  }
  
  if(pmath_is_pointer(obj)) {
    if(PMATH_AS_PTR(obj) == NULL)
      return PMATH_C_STRING("/\\/");
      
    if( thread->boxform < BOXFORM_OUTPUT &&
        user_make_boxes(&obj))
    {
      return obj;
    }
    
    if( thread->boxform < BOXFORM_OUTPUT) {
      pmath_t format;
      
      if(user_make_boxes(&obj))
        return obj;
        
      format = _pmath_get_user_format(obj);
      if(!pmath_same(format, PMATH_UNDEFINED)) {
        format = pmath_evaluate(
                   pmath_expr_new_extended(
                     pmath_ref(pmath_System_MakeBoxes), 1,
                     format));
        
        obj = pmath_expr_new_extended(
                pmath_ref(pmath_System_InterpretationBox), 2,
                strip_interpretation_boxes(format),
                obj);
        return pmath_evaluate(obj);
      }
    }
    
    switch(PMATH_AS_PTR(obj)->type_shift) {
      case PMATH_TYPE_SHIFT_SYMBOL: {
          pmath_string_t s = PMATH_NULL;
          
          if(thread->boxform < BOXFORM_OUTPUT) {
            if(pmath_same(obj, pmath_System_Pi)) {
              pmath_unref(obj);
              return pmath_build_value("c", 0x03C0);
            }
            
            if(pmath_same(obj, pmath_System_E)) {
              pmath_unref(obj);
              return pmath_build_value("c", 0x2147);
            }
            
            if(pmath_same(obj, pmath_System_Infinity)) {
              pmath_unref(obj);
              return pmath_build_value("c", 0x221E);
            }
          }
          
          pmath_write(
            obj,
            thread->longform ? PMATH_WRITE_OPTIONS_FULLNAME : 0,
            _pmath_write_to_string,
            &s);
          pmath_unref(obj);
          return s;
        }
        
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
      case PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: {
          pmath_t debug_metadata = pmath_get_debug_metadata(obj);
          obj = expr_to_boxes(thread, obj);
          obj = pmath_try_set_debug_metadata(obj, debug_metadata);
          return obj;
        }
        
      case PMATH_TYPE_SHIFT_PACKED_ARRAY:
        return packed_array_to_boxes(thread, obj);
        
      case PMATH_TYPE_SHIFT_MP_FLOAT:
      case PMATH_TYPE_SHIFT_MP_INT: {
          pmath_string_t s = PMATH_NULL;
          pmath_write(
            obj,
            PMATH_WRITE_OPTIONS_INPUTEXPR,
            _pmath_write_to_string,
            &s);
          pmath_unref(obj);
          
          if( pmath_string_length(s) > 0 &&
              *pmath_string_buffer(&s) == '-')
          {
            pmath_string_t minus = pmath_string_part(pmath_ref(s), 0, 1);
            return pmath_build_value("(oo)", minus, pmath_string_part(s, 1, -1));
          }
          
          return s;
        }
        
      case PMATH_TYPE_SHIFT_QUOTIENT: {
          pmath_string_t s, n, d;
          pmath_t result;
          s = n = d = PMATH_NULL;
          
          pmath_write(
            PMATH_QUOT_NUM(obj),
            PMATH_WRITE_OPTIONS_INPUTEXPR,
            _pmath_write_to_string,
            &n);
            
          pmath_write(
            PMATH_QUOT_DEN(obj),
            PMATH_WRITE_OPTIONS_INPUTEXPR,
            _pmath_write_to_string,
            &d);
            
          if(pmath_string_length(n) > 0 && *pmath_string_buffer(&n) == '-') {
            s = pmath_string_part(pmath_ref(n), 0, 1);
            n = pmath_string_part(n, 1, -1);
          }
          
          pmath_unref(obj);
          
          if( thread->boxform != BOXFORM_STANDARDEXPONENT &&
              thread->boxform != BOXFORM_OUTPUTEXPONENT   &&
              thread->boxform <= BOXFORM_OUTPUT)
          {
            result = pmath_expr_new_extended(
                       pmath_ref(pmath_System_FractionBox), 2,
                       n,
                       d);
          }
          else {
            result = pmath_expr_new_extended(
                       pmath_ref(pmath_System_List), 3,
                       n,
                       PMATH_C_STRING("/"),
                       d);
          }
          
          if(!pmath_is_null(s))
            return pmath_build_value("(oo)", s, result);
            
          return result;
        }
        
      case PMATH_TYPE_SHIFT_BIGSTRING: {
          return string_to_stringbox(thread, obj);
//        pmath_string_t quote = PMATH_C_STRING("\"");
//
//        obj = _pmath_string_escape(
//                pmath_ref(quote),
//                obj,
//                pmath_ref(quote),
//                FALSE/*thread->boxform >= BOXFORM_INPUT*/);
//
//        pmath_unref(quote);
//        return obj;
        }
    }
  }
  
  {
    pmath_string_t str = PMATH_NULL;
    pmath_t boxes;
    pmath_span_array_t *spans;
    
    pmath_write(obj, 0, _pmath_write_to_string, &str);
    
    spans = pmath_spans_from_string(&str, NULL, NULL, NULL, NULL, NULL);
    boxes = pmath_boxes_from_spans(spans, str, FALSE, NULL, NULL);
    pmath_span_array_free(spans);
    pmath_unref(str);
    
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_InterpretationBox), 2,
             strip_interpretation_boxes(boxes),
             obj);
  }
}

//} ... boxforms for more complex functions

PMATH_PRIVATE pmath_t builtin_makeboxes(pmath_expr_t expr) {
  /* MakeBoxes(object)
   */
  pmath_thread_t thread;
  pmath_t obj;
  
  thread = pmath_thread_get_current();
  if(!thread) {
    pmath_unref(expr);
    return PMATH_NULL;
  }
  
  if(pmath_expr_length(expr) != 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  obj = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  return object_to_boxes(thread, obj);
}

static pmath_t strip_pattern_condition(pmath_t expr) {
  for(;;) {
    if( pmath_is_expr_of_len(expr, pmath_System_Condition, 2) ||
        pmath_is_expr_of_len(expr, pmath_System_TestPattern, 2))
    {
      pmath_t arg = pmath_expr_get_item(expr, 1);
      
      pmath_unref(expr);
      expr = arg;
      continue;
    }
    
    break;
  }
  
  return expr;
}

PMATH_PRIVATE pmath_t builtin_assign_makeboxes_or_format(pmath_expr_t expr) {
  /* MakeBoxes(...)::= ...
     Format(...)::= ...
   */
  struct _pmath_symbol_rules_t *rules;
  pmath_t        tag;
  pmath_t        lhs;
  pmath_t        arg;
  pmath_t        rhs;
  pmath_symbol_t out_tag;
  int            kind_of_lhs;
  int            error;
  
  if(!_pmath_is_assignment(expr, &tag, &lhs, &rhs))
    return expr;
    
  if(!pmath_is_expr_of_len(lhs, pmath_System_MakeBoxes, 1) &&
      !pmath_is_expr_of_len(lhs, pmath_System_Format, 1))
  {
    pmath_unref(tag);
    pmath_unref(lhs);
    pmath_unref(rhs);
    return expr;
  }
  
  arg = pmath_expr_get_item(lhs, 1);
  arg = strip_pattern_condition(arg);
  out_tag = PMATH_NULL;
  error = _pmath_find_tag(arg, tag, &out_tag, &kind_of_lhs, FALSE);
  pmath_unref(arg);
  
  if(!error && kind_of_lhs == UP_RULES)
    error = SYM_SEARCH_TOODEEP;
    
  switch(error) {
    case SYM_SEARCH_NOTFOUND:
      if(pmath_same(tag, PMATH_UNDEFINED)) {
        pmath_message(PMATH_NULL, "notag", 1, lhs);
        pmath_unref(tag);
      }
      else
        pmath_message(PMATH_NULL, "tagnf", 2, tag, lhs);
        
      pmath_unref(out_tag);
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(pmath_System_DollarFailed);
      return rhs;
      
    case SYM_SEARCH_ALTERNATIVES:
      pmath_message(PMATH_NULL, "noalt", 1, lhs);
      pmath_unref(tag);
      pmath_unref(out_tag);
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(pmath_System_DollarFailed);
      return rhs;
      
    case SYM_SEARCH_TOODEEP:
      pmath_message(PMATH_NULL, "tagpos", 2, out_tag, lhs);
      pmath_unref(tag);
      pmath_unref(expr);
      if(pmath_same(rhs, PMATH_UNDEFINED))
        return pmath_ref(pmath_System_DollarFailed);
      return rhs;
  }
  
//  if(!pmath_same(tag, PMATH_UNDEFINED)
//  && !pmath_same(tag, sym)){
//    pmath_message(PMATH_NULL, "tag", 3, tag, lhs, sym);
//
//    pmath_unref(expr);
//    if(pmath_same(rhs, PMATH_UNDEFINED))
//      return pmath_ref(pmath_System_DollarFailed);
//    return rhs;
//  }

  pmath_unref(tag);
  pmath_unref(expr);
  
  rules = _pmath_symbol_get_rules(out_tag, RULES_WRITE);
  pmath_unref(out_tag);
  
  if(!rules) {
    pmath_unref(lhs);
    pmath_unref(rhs);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  _pmath_rulecache_change(&rules->format_rules, lhs, rhs);
  
  return PMATH_NULL;
}

PMATH_PRIVATE pmath_t builtin_parenthesizeboxes(pmath_expr_t expr) {
  /* ParenthesizeBoxes(obj, prec, pos)
   */
  pmath_t box, precobj, posobj;
  int prec, pos;
  
  if(pmath_expr_length(expr) != 3) {
    pmath_message_argxxx(pmath_expr_length(expr), 3, 3);
    return expr;
  }
  
  pos = 0;
  box     = pmath_expr_get_item(expr, 1);
  precobj = pmath_expr_get_item(expr, 2);
  posobj  = pmath_expr_get_item(expr, 3);
  
  if(pmath_is_string(posobj)) {
    if(pmath_string_equals_latin1(posobj, "Prefix"))  pos = +1;
    else if(pmath_string_equals_latin1(posobj, "Postfix")) pos = -1;
    else if(pmath_string_equals_latin1(posobj, "Infix"))   pos = 0;
  }
  
  prec = _pmath_symbol_to_precedence(precobj);
  pmath_unref(precobj);
  pmath_unref(posobj);
  pmath_unref(expr);
  
  box = ensure_min_precedence(box, prec + 1, pos);
  
  return box;
}
