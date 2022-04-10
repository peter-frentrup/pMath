#include <pmath-core/expressions-private.h>
#include <pmath-core/numbers-private.h>
#include <pmath-core/strings.h>

#include <pmath-language/charnames.h>
#include <pmath-language/number-parsing-private.h>
#include <pmath-language/patterns-private.h>
#include <pmath-language/tokens.h>

#include <pmath-util/concurrency/threads.h>
#include <pmath-util/compression.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/debug.h>
#include <pmath-util/helpers.h>
#include <pmath-util/memory.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>
#include <pmath-util/strtod.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/arithmetic-private.h>
#include <pmath-builtins/build-expr-private.h>
#include <pmath-builtins/formating-private.h>
#include <pmath-builtins/lists-private.h>

#include <limits.h>
#include <string.h>


const uint16_t char_LeftCeiling  = 0x2308;
const uint16_t char_RightCeiling = 0x2309;
const uint16_t char_LeftFloor    = 0x230A;
const uint16_t char_RightFloor   = 0x230B;

extern pmath_symbol_t pmath_System_DollarFailed;
extern pmath_symbol_t pmath_System_Alternatives;
extern pmath_symbol_t pmath_System_And;
extern pmath_symbol_t pmath_System_Apply;
extern pmath_symbol_t pmath_System_Assign;
extern pmath_symbol_t pmath_System_AssignDelayed;
extern pmath_symbol_t pmath_System_AssignWith;
extern pmath_symbol_t pmath_System_Automatic;
extern pmath_symbol_t pmath_System_BoxRotation;
extern pmath_symbol_t pmath_System_BracketingBar;
extern pmath_symbol_t pmath_System_Ceiling;
extern pmath_symbol_t pmath_System_CircleTimes;
extern pmath_symbol_t pmath_System_CirclePlus;
extern pmath_symbol_t pmath_System_Colon;
extern pmath_symbol_t pmath_System_Column;
extern pmath_symbol_t pmath_System_ColumnSpacing;
extern pmath_symbol_t pmath_System_CompressedData;
extern pmath_symbol_t pmath_System_Condition;
extern pmath_symbol_t pmath_System_Congruent;
extern pmath_symbol_t pmath_System_Cross;
extern pmath_symbol_t pmath_System_CupCap;
extern pmath_symbol_t pmath_System_Decrement;
extern pmath_symbol_t pmath_System_Derivative;
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
extern pmath_symbol_t pmath_System_DoubleBracketingBar;
extern pmath_symbol_t pmath_System_DivideBy;
extern pmath_symbol_t pmath_System_Element;
extern pmath_symbol_t pmath_System_Equal;
extern pmath_symbol_t pmath_System_EvaluationSequence;
extern pmath_symbol_t pmath_System_Factorial;
extern pmath_symbol_t pmath_System_Factorial2;
extern pmath_symbol_t pmath_System_Floor;
extern pmath_symbol_t pmath_System_FractionBox;
extern pmath_symbol_t pmath_System_FrameBox;
extern pmath_symbol_t pmath_System_Framed;
extern pmath_symbol_t pmath_System_Function;
extern pmath_symbol_t pmath_System_Get;
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
extern pmath_symbol_t pmath_System_HoldAllComplete;
extern pmath_symbol_t pmath_System_HoldComplete;
extern pmath_symbol_t pmath_System_HumpDownHump;
extern pmath_symbol_t pmath_System_HumpEqual;
extern pmath_symbol_t pmath_System_Increment;
extern pmath_symbol_t pmath_System_Identical;
extern pmath_symbol_t pmath_System_Inequation;
extern pmath_symbol_t pmath_System_InterpretationBox;
extern pmath_symbol_t pmath_System_InterpretationFunction;
extern pmath_symbol_t pmath_System_Intersection;
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
extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_LowerLeftArrow;
extern pmath_symbol_t pmath_System_LowerRightArrow;
extern pmath_symbol_t pmath_System_MakeExpression;
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
extern pmath_symbol_t pmath_System_Overscript;
extern pmath_symbol_t pmath_System_OverscriptBox;
extern pmath_symbol_t pmath_System_ParserArguments;
extern pmath_symbol_t pmath_System_ParseSymbols;
extern pmath_symbol_t pmath_System_Part;
extern pmath_symbol_t pmath_System_Pattern;
extern pmath_symbol_t pmath_System_PatternSequence;
extern pmath_symbol_t pmath_System_Piecewise;
extern pmath_symbol_t pmath_System_Placeholder;
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
extern pmath_symbol_t pmath_System_RawBoxes;
extern pmath_symbol_t pmath_System_Repeated;
extern pmath_symbol_t pmath_System_ReverseElement;
extern pmath_symbol_t pmath_System_RightTriangle;
extern pmath_symbol_t pmath_System_RightTriangleEqual;
extern pmath_symbol_t pmath_System_Rotate;
extern pmath_symbol_t pmath_System_RotationBox;
extern pmath_symbol_t pmath_System_RowSpacing;
extern pmath_symbol_t pmath_System_Rule;
extern pmath_symbol_t pmath_System_RuleDelayed;
extern pmath_symbol_t pmath_System_Sequence;
extern pmath_symbol_t pmath_System_ShowDefinition;
extern pmath_symbol_t pmath_System_SingleMatch;
extern pmath_symbol_t pmath_System_SqrtBox;
extern pmath_symbol_t pmath_System_StringBox;
extern pmath_symbol_t pmath_System_StringExpression;
extern pmath_symbol_t pmath_System_Style;
extern pmath_symbol_t pmath_System_StyleBox;
extern pmath_symbol_t pmath_System_Subscript;
extern pmath_symbol_t pmath_System_SubscriptBox;
extern pmath_symbol_t pmath_System_Subset;
extern pmath_symbol_t pmath_System_SubsetEqual;
extern pmath_symbol_t pmath_System_SubsuperscriptBox;
extern pmath_symbol_t pmath_System_Succeeds;
extern pmath_symbol_t pmath_System_SucceedsEqual;
extern pmath_symbol_t pmath_System_SucceedsTilde;
extern pmath_symbol_t pmath_System_SuperscriptBox;
extern pmath_symbol_t pmath_System_Superset;
extern pmath_symbol_t pmath_System_SupersetEqual;
extern pmath_symbol_t pmath_System_Surd;
extern pmath_symbol_t pmath_System_SurdForm;
extern pmath_symbol_t pmath_System_Symbol;
extern pmath_symbol_t pmath_System_TagAssign;
extern pmath_symbol_t pmath_System_TagAssignDelayed;
extern pmath_symbol_t pmath_System_TagBox;
extern pmath_symbol_t pmath_System_TagUnassign;
extern pmath_symbol_t pmath_System_TemplateBox;
extern pmath_symbol_t pmath_System_TestPattern;
extern pmath_symbol_t pmath_System_TildeEqual;
extern pmath_symbol_t pmath_System_TildeFullEqual;
extern pmath_symbol_t pmath_System_TildeTilde;
extern pmath_symbol_t pmath_System_Times;
extern pmath_symbol_t pmath_System_TimesBy;
extern pmath_symbol_t pmath_System_True;
extern pmath_symbol_t pmath_System_Unassign;
extern pmath_symbol_t pmath_System_Underoverscript;
extern pmath_symbol_t pmath_System_UnderoverscriptBox;
extern pmath_symbol_t pmath_System_Underscript;
extern pmath_symbol_t pmath_System_UnderscriptBox;
extern pmath_symbol_t pmath_System_Unequal;
extern pmath_symbol_t pmath_System_Unidentical;
extern pmath_symbol_t pmath_System_Union;
extern pmath_symbol_t pmath_System_UpArrow;
extern pmath_symbol_t pmath_System_UpDownArrow;
extern pmath_symbol_t pmath_System_UpperLeftArrow;
extern pmath_symbol_t pmath_System_UpperRightArrow;

extern pmath_symbol_t pmath_System_Private_FindTemplateInterpretationFunction;
extern pmath_symbol_t pmath_System_Private_FlattenTemplateSequence;
extern pmath_symbol_t pmath_System_Private_MakeLimitsExpression;
extern pmath_symbol_t pmath_System_Private_MakeScriptsExpression;
extern pmath_symbol_t pmath_System_Private_MakeJuxtapositionExpression;

static uint16_t unichar_at(pmath_expr_t expr, size_t i); 
static pmath_bool_t is_empty_box(pmath_expr_t box);
static pmath_bool_t is_string_at(pmath_expr_t expr, size_t i, const char *str);
static pmath_bool_t string_equals(pmath_string_t str, const char *cstr);
static pmath_bool_t string_equals_char_repeated(pmath_string_t str, uint16_t ch);
static pmath_bool_t are_linebreaks_only_at(pmath_expr_t expr, size_t i);
static pmath_bool_t is_subsuperscript_at(pmath_expr_t expr, size_t i);
static pmath_bool_t is_underoverscript_at(pmath_expr_t expr, size_t i);

#define is_parse_error(x)  pmath_is_magic(x)
static pmath_bool_t parse(    pmath_t *box);
static pmath_bool_t parse_seq(pmath_t *box, pmath_symbol_t seq); // seq wont be freed
static pmath_t parse_at(    pmath_expr_t expr, size_t i);
static pmath_t parse_seq_at(pmath_expr_t expr, size_t i, pmath_symbol_t seq);
static pmath_bool_t try_parse_helper(pmath_symbol_t helper, pmath_expr_t *expr);

static pmath_t wrap_hold_with_debuginfo_from(pmath_t boxes_with_debuginfo, pmath_t result); // both arguments be freed
static void handle_row_error_at(pmath_expr_t expr, size_t i);

static pmath_symbol_t inset_operator(uint16_t ch); // do not free result!
static pmath_symbol_t relation_at(pmath_expr_t expr, size_t i); // do not free result!

static void emit_grid_options(pmath_expr_t options);
static pmath_t parse_gridbox(pmath_expr_t expr, pmath_bool_t remove_styling); // expr wont be freed, return PMATH_NULL on error

static pmath_t make_expression_with_options(pmath_expr_t expr);
static pmath_t get_parser_argument_from_string(pmath_string_t string); // string will be freed

static pmath_t make_expression_from_string(pmath_string_t string);
static pmath_t make_expression_from_name_token(pmath_string_t string);
static pmath_t make_expression_from_string_token(pmath_string_t string);
static pmath_string_t unescape_chars(pmath_string_t str);
static pmath_string_t box_as_string(pmath_t box);
static pmath_t make_expression_from_stringbox(pmath_expr_t box);
static pmath_t make_expression_from_compresseddata(pmath_expr_t box);
static pmath_t make_expression_from_fractionbox(pmath_expr_t box);
static pmath_t make_expression_from_framebox(pmath_expr_t box);
static pmath_t make_expression_from_gridbox(pmath_expr_t box);
static pmath_t make_expression_from_gridbox_column(pmath_t box, pmath_expr_t gridbox);
static pmath_t make_expression_from_interpretationbox(pmath_expr_t box);
static pmath_t make_expression_from_overscriptbox(pmath_expr_t box);
static pmath_t make_expression_from_radicalbox(pmath_expr_t box);
static pmath_t make_expression_from_rotationbox(pmath_expr_t box);
static pmath_t make_expression_from_sqrtbox(pmath_expr_t box);
static pmath_t make_expression_from_stylebox(pmath_expr_t box);
static pmath_t make_expression_from_tagbox(pmath_expr_t box);
static pmath_t make_expression_from_templatebox(pmath_expr_t box);
static pmath_t make_expression_from_underscriptbox(pmath_expr_t box);
static pmath_t make_expression_from_underoverscriptbox(pmath_expr_t box);

static pmath_t make_parenthesis(pmath_expr_t boxes); // (x)
static pmath_t make_comma_sequence(pmath_expr_t expr); // a,b,c ...
static pmath_t make_evaluation_sequence(pmath_expr_t boxes); // a; b; c\[RawNewline]d ...
static pmath_t make_implicit_evaluation_sequence(pmath_expr_t boxes);
static pmath_t make_optional_pattern(pmath_expr_t boxes); // ?x  ?x:v
static pmath_t make_derivative(      pmath_expr_t boxes, int order);
static pmath_t make_from_first_box(  pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_from_second_box( pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_matchfix(        pmath_expr_t boxes, pmath_symbol_t sym); // {}  {args}  \[LeftCeiling]arg\[RightCeiling]  ...
static pmath_t make_binary(          pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_pattern_op_other(pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_infix_unchecked( pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_infix(           pmath_expr_t boxes, pmath_symbol_t sym); // boxes will be freed, sym won't
static pmath_t make_relation(        pmath_expr_t boxes, pmath_symbol_t rel); // boxes will be freed, sym won't
static pmath_t make_repeated_pattern(pmath_expr_t boxes, pmath_t range); // x**  x***
static pmath_t make_unary_plus(pmath_expr_t boxes); // +x
static pmath_t make_unary_minus(pmath_expr_t boxes); // -x
static pmath_t make_pure_argument_range(pmath_expr_t boxes); // ##1
static pmath_t make_text_line(pmath_expr_t boxes, pmath_symbol_t sym); // ??name  <<file    boxes will be freed, sym won't
static pmath_t make_named_match(pmath_expr_t boxes, pmath_t range); // ~x  ~~x  ~~~x
static pmath_t make_superscript(   pmath_expr_t boxes, pmath_expr_t superscript_box);
static pmath_t make_subscript(     pmath_expr_t boxes, pmath_expr_t subscript_box);
static pmath_t make_subsuperscript(pmath_expr_t boxes, pmath_expr_t subsuperscript_box);
static pmath_t make_simple_dot_call(  pmath_expr_t boxes); // a.f  a.f()
static pmath_t make_dot_call(         pmath_expr_t boxes); // a.f(args)
static pmath_t make_prefix_call(      pmath_expr_t boxes); // f @ a
static pmath_t make_postfix_call(     pmath_expr_t boxes); // a // f
static pmath_t make_argumentless_call(pmath_expr_t boxes); // f()
static pmath_t make_simple_call(      pmath_expr_t boxes); // f(args)
static pmath_t make_pattern_or_typed_match(pmath_expr_t boxes); // x:p   ~:t  ~~:t  ~~~:t
static pmath_t make_arrow_function(pmath_expr_t boxes); // args |-> body
static pmath_t make_apply(pmath_expr_t boxes); // f @@ list
static pmath_t make_message_name(pmath_expr_t boxes); // sym::tag
static pmath_t make_typed_named_match(pmath_expr_t boxes); // ~x:t  ~~x:t  ~~~x:t
static pmath_t make_tag_assignment(pmath_expr_t boxes); // tag/: lhs:= rhs   tag/: lhs::= rhs
static pmath_t make_plus(pmath_expr_t boxes); // a + b - c ...
static pmath_t make_division(pmath_expr_t boxes); //  a/b/c...
static pmath_t make_and(pmath_expr_t boxes); // a && b & c ...
static pmath_t make_or(pmath_expr_t boxes); // a || b || c ...
static pmath_t make_part(pmath_expr_t boxes); // l[args]
static pmath_t make_range(pmath_expr_t boxes); // a..b   a..b..c  with any of (a,b,c) optional
static pmath_t make_multiplication(pmath_expr_t boxes);

#define HOLDCOMPLETE(result) pmath_expr_new_extended(\
    pmath_ref(pmath_System_HoldComplete), 1, result)

// evaluates MakeExpression(box) but also retains debug information
PMATH_PRIVATE pmath_t _pmath_makeexpression_with_debuginfo(pmath_t box) {
  pmath_t debug_info = pmath_get_debug_info(box);
  
  box = pmath_evaluate(
          pmath_expr_new_extended(
            pmath_ref(pmath_System_MakeExpression), 1, box));
            
  if(pmath_is_null(debug_info))
    return box;
    
  if(!pmath_is_expr_of(box, pmath_System_HoldComplete)) {
    pmath_unref(debug_info);
    return box;
  }
  
  if(pmath_expr_length(box) == 1) {
    pmath_t content = pmath_expr_get_item(box, 1);
    
    if(pmath_is_ministr(content) || pmath_refcount(content) == 2) {
      // one reference here and one in "box"
      
      pmath_unref(pmath_expr_extract_item(box, 1));
      
      content = pmath_try_set_debug_info(content, pmath_ref(debug_info));
      
      box = pmath_expr_set_item(box, 1, content);
    }
    else
      pmath_unref(content);
  }
  
  return pmath_try_set_debug_info(box, debug_info);
}

PMATH_PRIVATE pmath_t builtin_makeexpression(pmath_expr_t expr) {
  /* MakeExpression(boxes)
     returns $Failed on error and HoldComplete(result) on success.
  
     options:
       ParserArguments     {arg1, arg2, ...}
       ParseSymbols        True/False
   */
  pmath_t box;
  size_t exprlen = pmath_expr_length(expr);
  
  if(exprlen > 1)
    return make_expression_with_options(expr);
    
  if(exprlen != 1) {
    pmath_message_argxxx(exprlen, 1, 1);
    pmath_unref(expr);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  box = pmath_expr_get_item(expr, 1);
  pmath_unref(expr);
  
  if(pmath_is_string(box))
    return make_expression_from_string(box);
    
  if(pmath_is_expr(box)) {
    pmath_t head;
    uint16_t firstchar, secondchar;
    
    head = pmath_expr_get_item(box, 0);
    pmath_unref(head);
    
    expr = box;
    box = PMATH_NULL;
    
    exprlen = pmath_expr_length(expr);
    
    if(!pmath_same(head, pmath_System_List)) {
      // implicit evaluation sequence (newlines -> head = /\/ = PMATH_NULL) ...
      if(pmath_is_null(head))
        return make_implicit_evaluation_sequence(expr);
        
      if(pmath_same(head, pmath_System_StringBox))
        return make_expression_from_stringbox(expr);
        
      if(pmath_same(head, pmath_System_CompressedData))
        return make_expression_from_compresseddata(expr);
        
      if(pmath_same(head, pmath_System_FractionBox))
        return make_expression_from_fractionbox(expr);
        
      if(pmath_same(head, pmath_System_FrameBox))
        return make_expression_from_framebox(expr);
        
      if(pmath_same(head, pmath_System_GridBox))
        return make_expression_from_gridbox(expr);
        
      if(pmath_same(head, pmath_System_HoldComplete))
        return expr;
        
      if(pmath_same(head, pmath_System_InterpretationBox))
        return make_expression_from_interpretationbox(expr);
        
      if(pmath_same(head, pmath_System_OverscriptBox))
        return make_expression_from_overscriptbox(expr);
        
      if(pmath_same(head, pmath_System_RadicalBox))
        return make_expression_from_radicalbox(expr);
        
      if(pmath_same(head, pmath_System_RotationBox))
        return make_expression_from_rotationbox(expr);
        
      if(pmath_same(head, pmath_System_SqrtBox))
        return make_expression_from_sqrtbox(expr);
        
      if(pmath_same(head, pmath_System_StyleBox))
        return make_expression_from_stylebox(expr);
        
      if(pmath_same(head, pmath_System_TagBox))
        return make_expression_from_tagbox(expr);
        
      if(pmath_same(head, pmath_System_TemplateBox))
        return make_expression_from_templatebox(expr);
        
      if(pmath_same(head, pmath_System_UnderscriptBox))
        return make_expression_from_underscriptbox(expr);
        
      if(pmath_same(head, pmath_System_UnderoverscriptBox))
        return make_expression_from_underoverscriptbox(expr);
        
      pmath_message(PMATH_NULL, "inv", 1, expr);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    if(exprlen == 0) {
      pmath_unref(expr);
      return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
    }
    
    firstchar  = unichar_at(expr, 1);
    secondchar = unichar_at(expr, 2);
    
    // ()  and  (x) ...
    if(firstchar == '(' && unichar_at(expr, exprlen) == ')')
      return make_parenthesis(expr);
      
    // \[LeftInvisibleBracket]x\[RightInvisibleBracket]
    if(firstchar == PMATH_CHAR_LEFTINVISIBLEBRACKET && unichar_at(expr, exprlen) == PMATH_CHAR_RIGHTINVISIBLEBRACKET)
      return make_parenthesis(expr);
      
    // {}  and  {x}  and  {grid\[RightInvisibleBracket]
    if(firstchar == '{') {
      uint16_t lastchar = unichar_at(expr, exprlen);
      
      if(lastchar == '}')
        return make_matchfix(expr, pmath_System_List);
        
      if(lastchar == PMATH_CHAR_RIGHTINVISIBLEBRACKET)
        return make_matchfix(expr, pmath_System_Piecewise);
    }
    
    if(firstchar == char_LeftCeiling && unichar_at(expr, exprlen) == char_RightCeiling)
      return make_matchfix(expr, pmath_System_Ceiling);
      
    if(firstchar == char_LeftFloor && unichar_at(expr, exprlen) == char_RightFloor)
      return make_matchfix(expr, pmath_System_Floor);
      
    if(firstchar == PMATH_CHAR_LEFTBRACKETINGBAR && unichar_at(expr, exprlen) == PMATH_CHAR_RIGHTBRACKETINGBAR)
      return make_matchfix(expr, pmath_System_BracketingBar);
      
    if(firstchar == PMATH_CHAR_LEFTDOUBLEBRACKETINGBAR && unichar_at(expr, exprlen) == PMATH_CHAR_RIGHTDOUBLEBRACKETINGBAR)
      return make_matchfix(expr, pmath_System_DoubleBracketingBar);
      
    // comma sepearted list ...
    if(firstchar == ',' || secondchar == ',' || firstchar == PMATH_CHAR_INVISIBLECOMMA || secondchar == PMATH_CHAR_INVISIBLECOMMA)
      return make_comma_sequence(expr);
      
    // evaluation sequence ...
    if(firstchar == ';' || secondchar == ';' || firstchar == '\n' || secondchar == '\n')
      return make_evaluation_sequence(expr);
      
    if(exprlen == 1)
      return pmath_expr_set_item(expr, 0, pmath_ref(pmath_System_MakeExpression));
      
    // ?x  and  ?x:v
    if(firstchar == '?')
      return make_optional_pattern(expr);
      
    // x& x! x++ x-- x.. p** p*** +x -x !x #x ++x --x ..x ??x <<x ~x ~~x ~~~x
    if(exprlen == 2) {
      pmath_string_t str;
      
      // x &
      if(secondchar == '&')
        return make_from_first_box(expr, pmath_System_Function);
        
      // x!
      if(secondchar == '!')
        return make_from_first_box(expr, pmath_System_Factorial);
      
      // f'
      if(secondchar == '\'')
        return make_derivative(expr, 1);
      
      str = pmath_expr_get_item(expr, 2);
      if(pmath_is_string(str)) {
        // x!!
        if(string_equals(str, "!!")) {
          pmath_unref(str);
          return make_from_first_box(expr, pmath_System_Factorial2);
        }
        
        // x++
        if(string_equals(str, "++")) {
          pmath_unref(str);
          return make_from_first_box(expr, pmath_System_PostIncrement);
        }
        
        // x--
        if(string_equals(str, "--")) {
          pmath_unref(str);
          return make_from_first_box(expr, pmath_System_PostDecrement);
        }
        
        // p**  p***
        if(string_equals_char_repeated(str, '*')) {
          int len = pmath_string_length(str);
          
          // p**
          if(len == 2) {
            pmath_unref(str);
            return make_repeated_pattern(expr, pmath_ref(_pmath_object_range_from_one));
          }
            
          // p***
          if(len == 3) {
            pmath_unref(str);
            return make_repeated_pattern(expr, pmath_ref(_pmath_object_range_from_zero));
          }
        }
        
        // f''  f'''  f''''  etc.
        if(string_equals_char_repeated(str, '\'')) {
          int len = pmath_string_length(str);
          pmath_unref(str);
          return make_derivative(expr, len);
        }
      }
      pmath_unref(str);
      str = PMATH_NULL;
      
      // +x
      if(firstchar == '+' || firstchar == PMATH_CHAR_INVISIBLEPLUS)
        return make_unary_plus(expr);
        
      // -x
      if(firstchar == '-')
        return make_unary_minus(expr);
        
      if(firstchar == PMATH_CHAR_PIECEWISE)
        return make_from_second_box(expr, pmath_System_Piecewise);
        
      // !x
      if(firstchar == '!' || firstchar == 0x00AC)
        return make_from_second_box(expr, pmath_System_Not);
        
      // #x
      if(firstchar == '#')
        return make_from_second_box(expr, pmath_System_PureArgument);
      
      // ##x
      if(is_string_at(expr, 1, "##"))
        return make_pure_argument_range(expr);
        
      // ++x
      if(is_string_at(expr, 1, "++"))
        return make_from_second_box(expr, pmath_System_Increment);
        
      // --x
      if(is_string_at(expr, 1, "--"))
        return make_from_second_box(expr, pmath_System_Decrement);
        
      // ??x
      if(is_string_at(expr, 1, "??"))
        return make_text_line(expr, pmath_System_ShowDefinition);
        
      // <<x
      if(is_string_at(expr, 1, "<<"))
        return make_text_line(expr, pmath_System_Get);
        
      // ~x
      if(firstchar == '~')
        return make_named_match(expr, pmath_ref(_pmath_object_singlematch));
        
      // ~~x
      if(is_string_at(expr, 1, "~~"))
        return make_named_match(expr, pmath_ref(_pmath_object_multimatch));
        
      // ~~~x
      if(is_string_at(expr, 1, "~~~"))
        return make_named_match(expr, pmath_ref(_pmath_object_zeromultimatch));
        
      // x^y   Subscript(x, y, ...)   Subscript(x,y,...)^z
      if(secondchar == 0) {
        box = pmath_expr_get_item(expr, 2);
        
        if(pmath_is_expr_of_len(box, pmath_System_SuperscriptBox, 1))
          return make_superscript(expr, box);
          
        if(pmath_is_expr_of_len(box, pmath_System_SubscriptBox, 1))
          return make_subscript(expr, box);
          
        if(pmath_is_expr_of_len(box, pmath_System_SubsuperscriptBox, 2))
          return make_subsuperscript(expr, box);
          
        pmath_unref(box);
        box = PMATH_NULL;
      }
    }
    
    // a.f  f@x  f@@list  s::tag  f()  ~:t  ~~:t  ~~~:t  x:p  a//f  p/?c  l->r  l:=r  l+=r  l-=r  l:>r  l::=r  l..r
    if(exprlen == 3) {       // a.f
      if(secondchar == '.')
        return make_simple_dot_call(expr);
        
      // f@x
      if(secondchar == '@' || secondchar == PMATH_CHAR_INVISIBLECALL)
        return make_prefix_call(expr);
        
      // f()
      if(secondchar == '(' && unichar_at(expr, 3) == ')')
        return make_argumentless_call(expr);
        
      // ~:t  ~~:t  ~~~:t  x:p
      if(secondchar == ':')
        return make_pattern_or_typed_match(expr);
        
      // args |-> body
      if(secondchar == 0x21A6)
        return make_arrow_function(expr);
        
      // f@@list
      if(is_string_at(expr, 2, "@@"))
        return make_apply(expr);
        
      // s::tag
      if(is_string_at(expr, 2, "::"))
        return make_message_name(expr);
        
      // lhs:=rhs
      if(secondchar == PMATH_CHAR_ASSIGN)
        return make_binary(expr, pmath_System_Assign);
        
      // lhs::=rhs
      if(secondchar == PMATH_CHAR_ASSIGNDELAYED)
        return make_binary(expr, pmath_System_AssignDelayed);
        
      // lhs->rhs
      if(secondchar == PMATH_CHAR_RULE)
        return make_pattern_op_other(expr, pmath_System_Rule);
        
      // lhs:>rhs
      if(secondchar == PMATH_CHAR_RULEDELAYED)
        return make_pattern_op_other(expr, pmath_System_RuleDelayed);
        
      // p?f
      if(secondchar == '?')
        return make_pattern_op_other(expr, pmath_System_TestPattern);
        
      // lhs:=rhs
      if(is_string_at(expr, 2, ":="))
        return make_binary(expr, pmath_System_Assign);
        
      // lhs::=rhs
      if(is_string_at(expr, 2, "::="))
        return make_binary(expr, pmath_System_AssignDelayed);
        
      // lhs->rhs
      if(is_string_at(expr, 2, "->"))
        return make_pattern_op_other(expr, pmath_System_Rule);
        
      // lhs:>rhs
      if(is_string_at(expr, 2, ":>"))
        return make_pattern_op_other(expr, pmath_System_RuleDelayed);
        
      // lhs+=rhs
      if(is_string_at(expr, 2, "+="))
        return make_binary(expr, pmath_System_Increment);
        
      // lhs-=rhs
      if(is_string_at(expr, 2, "-="))
        return make_binary(expr, pmath_System_Decrement);
        
      // lhs*=rhs
      if(is_string_at(expr, 2, "*="))
        return make_binary(expr, pmath_System_TimesBy);
        
      // lhs/=rhs
      if(is_string_at(expr, 2, "/="))
        return make_binary(expr, pmath_System_DivideBy);
        
      // lhs//=rhs
      if(is_string_at(expr, 2, "//="))
        return make_binary(expr, pmath_System_AssignWith);
        
      // p/?cond
      if(is_string_at(expr, 2, "/?"))
        return make_pattern_op_other(expr, pmath_System_Condition);
        
      // arg // f
      if(is_string_at(expr, 2, "//"))
        return make_postfix_call(expr);
        
      // args |-> body
      if(is_string_at(expr, 2, "|->"))
        return make_arrow_function(expr);
    }
    
    // ~x:t  ~~x:t  ~~~x:t
    if(exprlen == 4 && unichar_at(expr, 3) == ':')
      return make_typed_named_match(expr);
      
    // t/:l:=r   t/:l::=r
    if(exprlen == 5 && is_string_at(expr, 2, "/:"))
      return make_tag_assignment(expr);
      
    // infix operators (except * ) ...
    if(exprlen & 1) {
      int tokprec;
      
      if(secondchar == '+' || secondchar == '-' || secondchar == PMATH_CHAR_INVISIBLEPLUS)
        return make_plus(expr);
        
      // single character infix operators (except + - * / && || and relations) ...
      head = inset_operator(secondchar);
      if(!pmath_is_null(head))
        return make_infix(expr, head);
        
      pmath_token_analyse(&secondchar, 1, &tokprec);
      
      // x/y/.../z
      if(tokprec == PMATH_PREC_DIV)
        return make_division(expr);
        
      // a && b && ...
      if(secondchar == 0x2227)
        return make_and(expr);
        
      // a || b || ...
      if(secondchar == 0x2228)
        return make_or(expr);
        
      // a && b && ...
      if(is_string_at(expr, 2, "&&"))
        return make_and(expr);
        
      // a || b || ...
      if(is_string_at(expr, 2, "||"))
        return make_or(expr);
        
      // a ++ b ++ ...
      if(is_string_at(expr, 2, "++"))
        return make_infix_unchecked(expr, pmath_System_StringExpression);
        
      // relations ...
      head = relation_at(expr, 2);
      if(!pmath_is_null(head))
        return make_relation(expr, head);
    }
    
    // f(x)  l[x]  l[[x]]
    if(exprlen == 4) {
      // f(x)
      if(secondchar == '(' && unichar_at(expr, 4) == ')')
        return make_simple_call(expr);
        
      // l[x]
      if(secondchar == '[' && unichar_at(expr, 4) == ']')
        return make_part(expr);
    }
    
    // a.f()
    if( exprlen == 5               &&
        secondchar == '.'          &&
        unichar_at(expr, 4) == '(' &&
        unichar_at(expr, 5) == ')')
    {
      return make_simple_dot_call(expr);
    }
    
    // a.f(x)
    if( exprlen == 6               &&
        secondchar == '.'          &&
        unichar_at(expr, 4) == '(' &&
        unichar_at(expr, 6) == ')')
    {
      return make_dot_call(expr);
    }
    
    // a..b..c
    if(is_string_at(expr, 2, "..") || is_string_at(expr, 1, ".."))
      return make_range(expr);
      
    if(is_underoverscript_at(expr, 1)) {
      if(try_parse_helper(pmath_System_Private_MakeLimitsExpression, &expr))
        return expr;
    }
    else if(exprlen > 2 && is_subsuperscript_at(expr, 2)) { // exprlen==2 case handled above
      if(try_parse_helper(pmath_System_Private_MakeScriptsExpression, &expr))
        return expr;
    }
    else if(secondchar != '*' && secondchar != PMATH_CHAR_TIMES && secondchar != PMATH_CHAR_INVISIBLETIMES) {
      if(try_parse_helper(pmath_System_Private_MakeJuxtapositionExpression, &expr))
        return expr;
    }
    
    // everything else is multiplication ...
    return make_multiplication(expr);
  }
  
  return HOLDCOMPLETE(box);
}

// only handles BMP chars U+0001 .. U+ffff, 0 on error
static uint16_t unichar_at(pmath_expr_t expr, size_t i) {
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  uint16_t result = 0;
  
  if(pmath_is_string(obj)) {
    const uint16_t *buf = pmath_string_buffer(&obj);
    int             len = pmath_string_length(obj);
    uint32_t u;
    
    if(len > 1) {
      const uint16_t *endbuf = pmath_char_parse(buf, len, &u);
      
      if(buf + len == endbuf && u <= 0xFFFF)
        result = (uint16_t)u;
    }
    else
      result = buf[0];
  }
  
  pmath_unref(obj);
  return result;
}

static pmath_bool_t is_empty_box(pmath_expr_t box) {
  if(pmath_is_string(box))
    return pmath_string_length(box) == 0;
  
  return pmath_is_expr_of_len(box, pmath_System_List, 0);
}

static pmath_bool_t string_equals(pmath_string_t str, const char *cstr) {
  const uint16_t *buf;
  size_t len = strlen(cstr);
  if((size_t)pmath_string_length(str) != len)
    return FALSE;
    
  buf = pmath_string_buffer(&str);
  while(len-- > 0)
    if(*buf++ != *cstr++)
      return FALSE;
      
  return TRUE;
}

static pmath_bool_t string_equals_char_repeated(pmath_string_t str, uint16_t ch) {
  int i;
  int len = pmath_string_length(str);
  const uint16_t *buf = pmath_string_buffer(&str);
  
  if(len < 1)
    return FALSE;
  
  for(i = 0; i < len; ++i) {
    if(buf[i] != ch)
      return FALSE;
  }
  
  return TRUE;
}

static pmath_bool_t is_string_at(pmath_expr_t expr, size_t i, const char *str) {
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  const uint16_t *buf;
  size_t len;
  
  if(!pmath_is_string(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  len = strlen(str);
  if(len != (size_t)pmath_string_length(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  
  buf = pmath_string_buffer(&obj);
  while(len-- > 0)
    if(*buf++ != (unsigned char)*str++) {
      pmath_unref(obj);
      return FALSE;
    }
    
  pmath_unref(obj);
  return TRUE;
}

static pmath_bool_t are_linebreaks_only_at(pmath_expr_t expr, size_t i) {
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  
  if(pmath_is_string(obj)) {
    pmath_bool_t is_linebreak = pmath_string_equals_latin1(obj, "\n");
    pmath_unref(obj);
    return is_linebreak;
  }
  
  if(pmath_is_expr_of(obj, pmath_System_List)) {
    size_t objlen = pmath_expr_length(obj);
    for(size_t i = objlen; i > 0; --i) {
      if(!are_linebreaks_only_at(obj, i)) {
        pmath_unref(obj);
        return FALSE;
      }
    }
    pmath_unref(obj);
    return TRUE;
  }
  
  pmath_unref(obj);
  return FALSE;
}

static pmath_bool_t is_subsuperscript_at(pmath_expr_t expr, size_t i) {
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  pmath_t head;
  
  if(!pmath_is_expr(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  head = pmath_expr_get_item(obj, 0);
  pmath_unref(obj);
  pmath_unref(head);
  return pmath_same(head, pmath_System_SubscriptBox) || 
         pmath_same(head, pmath_System_SuperscriptBox) || 
         pmath_same(head, pmath_System_SubsuperscriptBox);
}

static pmath_bool_t is_underoverscript_at(pmath_expr_t expr, size_t i) {
  pmath_string_t obj = pmath_expr_get_item(expr, i);
  pmath_t head;
  
  if(!pmath_is_expr(obj)) {
    pmath_unref(obj);
    return FALSE;
  }
  head = pmath_expr_get_item(obj, 0);
  pmath_unref(obj);
  pmath_unref(head);
  return pmath_same(head, pmath_System_UnderscriptBox) || 
         pmath_same(head, pmath_System_OverscriptBox) || 
         pmath_same(head, pmath_System_UnderoverscriptBox);
}

static pmath_bool_t parse(pmath_t *box) {
  return parse_seq(box, pmath_System_Sequence);
}

static pmath_bool_t parse_seq(pmath_t *box, pmath_symbol_t seq) {
// *box = PMATH_NULL if result is FALSE
  pmath_t obj;
  pmath_t debug_info = pmath_get_debug_info(*box);
  
  *box = pmath_evaluate(
           pmath_expr_new_extended(
             pmath_ref(pmath_System_MakeExpression), 1, *box));
             
  if(!pmath_is_expr(*box)) {
    pmath_unref(*box);
    pmath_unref(debug_info);
    *box = PMATH_NULL;
    return FALSE;
  }
  
  obj = pmath_expr_get_item(*box, 0);
  pmath_unref(obj);
  
  if(!pmath_same(obj, pmath_System_HoldComplete)) {
    pmath_message(PMATH_NULL, "inv", 1, *box);
    pmath_unref(debug_info);
    *box = PMATH_NULL;
    return FALSE;
  }
  
  if(pmath_expr_length(*box) != 1) {
    *box = pmath_expr_set_item(*box, 0, pmath_ref(seq));
    *box = pmath_try_set_debug_info(*box, debug_info);
    return TRUE;
  }
  
  obj = *box;
  *box = pmath_expr_get_item(*box, 1);
  pmath_unref(obj);
  
  if(pmath_is_ministr(*box) || pmath_refcount(*box) == 1)
    *box = pmath_try_set_debug_info(*box, debug_info);
  else
    pmath_unref(debug_info);
    
  return TRUE;
}

static pmath_t wrap_hold_with_debuginfo_from(
  pmath_t boxes_with_debuginfo, // will be freed
  pmath_t result                // will be freed
) {
  if(pmath_is_ministr(result) || pmath_refcount(result) == 1) {
    pmath_t debug_info = pmath_get_debug_info(boxes_with_debuginfo);
    
    result = pmath_try_set_debug_info(result, debug_info);
  }
  
  pmath_unref(boxes_with_debuginfo);
  
  return HOLDCOMPLETE(result);
}

static void handle_row_error_at(pmath_expr_t expr, size_t i) {
  if(i == 1) {
    pmath_message(PMATH_NULL, "bgn", 1,
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_RawBoxes), 1,
                    pmath_ref(expr)));
  }
  else {
    pmath_message(PMATH_NULL, "nxt", 2,
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_RawBoxes), 1,
                    pmath_expr_get_item_range(expr, 1, i - 1)),
                  pmath_expr_new_extended(
                    pmath_ref(pmath_System_RawBoxes), 1,
                    pmath_expr_get_item_range(expr, i, SIZE_MAX)));
  }
}

static pmath_t parse_at(pmath_expr_t expr, size_t i) {
  return parse_seq_at(expr, i, pmath_System_Sequence);
}

static pmath_t parse_seq_at(pmath_expr_t expr, size_t i, pmath_symbol_t seq) {
  pmath_t result = pmath_expr_get_item(expr, i);
  
  if(pmath_is_string(result)) {
    if(!parse(&result)) {
      handle_row_error_at(expr, i);
      
      return PMATH_UNDEFINED;
    }
    
    return result;
  }
  
  if(parse_seq(&result, seq))
    return result;
    
  return PMATH_UNDEFINED;
}

static pmath_bool_t try_parse_helper(pmath_symbol_t helper, pmath_expr_t *expr) {
  pmath_t call_helper = pmath_expr_new_extended(pmath_ref(helper), 1, pmath_ref(*expr));
  call_helper = pmath_evaluate(call_helper);
  if(pmath_is_expr_of(call_helper, pmath_System_HoldComplete) ||
      pmath_same(call_helper, pmath_System_DollarFailed))
  {
    pmath_unref(*expr);
    *expr = call_helper;
    return TRUE;
  }
  // TODO: warn if call_helper is not Default ?
  pmath_unref(call_helper);
  return FALSE;
}

static pmath_symbol_t inset_operator(uint16_t ch) { // do not free result!
  switch(ch) {
    case '|': return pmath_System_Alternatives;
//    case ':': return pmath_System_Pattern;
//    case '?': return pmath_System_TestPattern;
    case '^': return pmath_System_Power;
    
    case 0x00B7: return pmath_System_Dot;
    
//    case PMATH_CHAR_RULE:        return pmath_System_Rule;
//    case PMATH_CHAR_RULEDELAYED: return pmath_System_RuleDelayed;

//    case PMATH_CHAR_ASSIGN:        return pmath_System_Assign;
//    case PMATH_CHAR_ASSIGNDELAYED: return pmath_System_AssignDelayed;

    case 0x2190: return pmath_System_LeftArrow;
    case 0x2191: return pmath_System_UpArrow;
    //case 0x2192: return PMATH_SYMBOL_RIGHTARROW; // Rule
    case 0x2193: return pmath_System_DownArrow;
    case 0x2194: return pmath_System_LeftRightArrow;
    case 0x2195: return pmath_System_UpDownArrow;
    case 0x2196: return pmath_System_UpperLeftArrow;
    case 0x2197: return pmath_System_UpperRightArrow;
    case 0x2198: return pmath_System_LowerRightArrow;
    case 0x2199: return pmath_System_LowerLeftArrow;
    
    case 0x21D0: return pmath_System_DoubleLeftArrow;
    case 0x21D1: return pmath_System_DoubleUpArrow;
    case 0x21D2: return pmath_System_DoubleRightArrow;
    case 0x21D3: return pmath_System_DoubleDownArrow;
    case 0x21D4: return pmath_System_DoubleLeftRightArrow;
    case 0x21D5: return pmath_System_DoubleUpDownArrow;
    case 0x21D6: return pmath_System_DoubleUpperLeftArrow;
    case 0x21D7: return pmath_System_DoubleUpperRightArrow;
    case 0x21D8: return pmath_System_DoubleLowerRightArrow;
    case 0x21D9: return pmath_System_DoubleLowerLeftArrow;
    
    case 0x00B1: return pmath_System_PlusMinus;
    
    case 0x2213: return pmath_System_MinusPlus;
    
//    case 0x2227: return pmath_System_And;
//    case 0x2228: return pmath_System_Or;
    case 0x2229: return pmath_System_Intersection;
    case 0x222A: return pmath_System_Union;
    
    case 0x2236: return pmath_System_Colon;
    
    case 0x2295: return pmath_System_CirclePlus;
    case 0x2297: return pmath_System_CircleTimes;
    
    case 0x22C5: return pmath_System_Dot;
    
    case 0x2A2F: return pmath_System_Cross;
  }
  return PMATH_NULL;
}

static pmath_symbol_t relation_at(pmath_expr_t expr, size_t i) { // do not free result
  uint16_t ch = unichar_at(expr, i);
  
  switch(ch) {
    case 0:
      if(is_string_at(expr, i, "<="))  return pmath_System_LessEqual;
      if(is_string_at(expr, i, ">="))  return pmath_System_GreaterEqual;
      if(is_string_at(expr, i, "!="))  return pmath_System_Unequal;
      if(is_string_at(expr, i, "===")) return pmath_System_Identical;
      if(is_string_at(expr, i, "=!=")) return pmath_System_Unidentical;
      return PMATH_NULL;
      
    case '<': return pmath_System_Less;
    case '>': return pmath_System_Greater;
    case '=': return pmath_System_Equal;
  }
  
  switch(ch) {
    case 0x2208: return pmath_System_Element;
    case 0x2209: return pmath_System_NotElement;
    case 0x220B: return pmath_System_ReverseElement;
    case 0x220C: return pmath_System_NotReverseElement;
  }
  
  switch(ch) {
    case 0x2243: return pmath_System_TildeEqual;
    case 0x2244: return pmath_System_NotTildeEqual;
    case 0x2245: return pmath_System_TildeFullEqual;
    
    case 0x2247: return pmath_System_NotTildeFullEqual;
    case 0x2248: return pmath_System_TildeTilde;
    case 0x2249: return pmath_System_NotTildeTilde;
    
    case 0x224D: return pmath_System_CupCap;
    case 0x224E: return pmath_System_HumpDownHump;
    case 0x224F: return pmath_System_HumpEqual;
    case 0x2250: return pmath_System_DotEqual;
  }
  
  switch(ch) {
    case 0x2260: return pmath_System_Unequal;
    case 0x2261: return pmath_System_Congruent;
    case 0x2262: return pmath_System_NotCongruent;
    
    case 0x2264: return pmath_System_LessEqual;
    case 0x2265: return pmath_System_GreaterEqual;
    case 0x2266: return pmath_System_LessFullEqual;
    case 0x2267: return pmath_System_GreaterFullEqual;
    
    case 0x226A: return pmath_System_LessLess;
    case 0x226B: return pmath_System_GreaterGreater;
    
    case 0x226D: return pmath_System_NotCupCap;
    case 0x226E: return pmath_System_NotLess;
    case 0x226F: return pmath_System_NotGreater;
    case 0x2270: return pmath_System_NotLessEqual;
    case 0x2271: return pmath_System_NotGreaterEqual;
    case 0x2272: return pmath_System_LessTilde;
    case 0x2273: return pmath_System_GreaterTilde;
    case 0x2274: return pmath_System_NotLessTilde;
    case 0x2275: return pmath_System_NotGreaterTilde;
    case 0x2276: return pmath_System_LessGreater;
    case 0x2277: return pmath_System_GreaterLess;
    case 0x2278: return pmath_System_NotLessGreater;
    case 0x2279: return pmath_System_NotGreaterLess;
    case 0x227A: return pmath_System_Precedes;
    case 0x227B: return pmath_System_Succeeds;
    case 0x227C: return pmath_System_PrecedesEqual;
    case 0x227D: return pmath_System_SucceedsEqual;
    case 0x227E: return pmath_System_PrecedesTilde;
    case 0x227F: return pmath_System_SucceedsTilde;
    case 0x2280: return pmath_System_NotPrecedes;
    case 0x2281: return pmath_System_NotSucceeds;
    case 0x2282: return pmath_System_Subset;
    case 0x2283: return pmath_System_Superset;
    case 0x2284: return pmath_System_NotSubset;
    case 0x2285: return pmath_System_NotSuperset;
    case 0x2286: return pmath_System_SubsetEqual;
    case 0x2287: return pmath_System_SupersetEqual;
    case 0x2288: return pmath_System_NotSubsetEqual;
    case 0x2289: return pmath_System_NotSupersetEqual;
  }
  
  switch(ch) {
    case 0x22B3: return pmath_System_LeftTriangle;
    case 0x22B4: return pmath_System_RightTriangle;
    case 0x22B5: return pmath_System_LeftTriangleEqual;
    case 0x22B6: return pmath_System_RightTriangleEqual;
  }
  
  switch(ch) {
    case 0x22DA: return pmath_System_LessEqualGreater;
    case 0x22DB: return pmath_System_GreaterEqualLess;
    
    case 0x22E0: return pmath_System_NotPrecedesEqual;
    case 0x22E1: return pmath_System_NotSucceedsEqual;
    
    case 0x22EA: return pmath_System_NotLeftTriangle;
    case 0x22EB: return pmath_System_NotRightTriangle;
    case 0x22EC: return pmath_System_NotLeftTriangleEqual;
    case 0x22ED: return pmath_System_NotRightTriangleEqual;
  }
  
  return PMATH_NULL;
}

static void emit_grid_options(pmath_expr_t options) { // options wont be freed
  size_t i;
  
  for(i = 1; i <= pmath_expr_length(options); ++i) {
    pmath_t item = pmath_expr_get_item(options, i);
    
    if(pmath_is_rule(item)) {
      pmath_t lhs = pmath_expr_get_item(item, 1);
      pmath_unref(lhs);
      
      if(pmath_same(lhs, pmath_System_GridBoxColumnSpacing)) {
        item = pmath_expr_set_item(item, 1, pmath_ref(pmath_System_ColumnSpacing));
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      if(pmath_same(lhs, pmath_System_GridBoxRowSpacing)) {
        item = pmath_expr_set_item(item, 1, pmath_ref(pmath_System_RowSpacing));
        pmath_emit(item, PMATH_NULL);
        continue;
      }
      
      pmath_emit(item, PMATH_NULL);
      continue;
    }
    
    if(pmath_is_expr_of(item, pmath_System_List)) {
      emit_grid_options(item);
      pmath_unref(item);
      continue;
    }
    
    pmath_unref(item);
  }
}

static pmath_t parse_gridbox(pmath_expr_t expr, pmath_bool_t remove_styling) { // expr wont be freed, return PMATH_NULL on error
  pmath_expr_t options, matrix, row;
  size_t height, width, i, j;
  
  options = pmath_options_extract(expr, 1);
  
  if(pmath_is_null(options))
    return PMATH_NULL;
    
  matrix = pmath_expr_get_item(expr, 1);
  
  if(!_pmath_is_matrix(matrix, &height, &width, FALSE)) {
    pmath_unref(options);
    pmath_unref(matrix);
    return PMATH_NULL;
  }
  
  for(i = 1; i <= height; ++i) {
    row = pmath_expr_get_item(matrix, i);
    matrix = pmath_expr_set_item(matrix, i, PMATH_NULL);
    
    for(j = 1; j <= width; ++j) {
      pmath_t obj = pmath_expr_get_item(row, j);
      row = pmath_expr_set_item(row, j, PMATH_NULL);
      
      if(is_empty_box(obj)) {
        pmath_unref(obj);
        obj = PMATH_NULL;
      }
      else if(!parse(&obj)) {
        pmath_unref(options);
        pmath_unref(matrix);
        pmath_unref(row);
        return PMATH_NULL;
      }
      
      row = pmath_expr_set_item(row, j, obj);
    }
    
    matrix = pmath_expr_set_item(matrix, i, row);
  }
  
  if(remove_styling) {
    pmath_unref(options);
    return wrap_hold_with_debuginfo_from(pmath_ref(expr), matrix);
  }
  
  pmath_gather_begin(PMATH_NULL);
  pmath_emit(matrix, PMATH_NULL);
  emit_grid_options(options);
  pmath_unref(options);
  
  row = pmath_gather_end();
  row = pmath_expr_set_item(row, 0, pmath_ref(pmath_System_Grid));
  return wrap_hold_with_debuginfo_from(pmath_ref(expr), row);
}

static pmath_t make_expression_with_options(pmath_expr_t expr) {
  pmath_expr_t options = pmath_options_extract(expr, 1);
  
  if(!pmath_is_null(options)) {
    pmath_t args = pmath_option_value(PMATH_NULL, pmath_System_ParserArguments, options);
    pmath_t syms = pmath_option_value(PMATH_NULL, pmath_System_ParseSymbols,    options);
    pmath_t box;
    
    if(!pmath_same(args, pmath_System_Automatic)) {
      args = pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args);
    }
    
    if(!pmath_same(syms, pmath_System_Automatic)) {
      syms = pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, syms);
    }
    
    box = pmath_expr_get_item(expr, 1);
    pmath_unref(expr);
    expr = _pmath_makeexpression_with_debuginfo(box);
    
    pmath_unref(options);
    if(!pmath_same(args, pmath_System_Automatic)) {
      pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSERARGUMENTS, args));
    }
    else
      pmath_unref(args);
      
    if(!pmath_same(syms, pmath_System_Automatic)) {
      pmath_unref(pmath_thread_local_save(PMATH_THREAD_KEY_PARSESYMBOLS, syms));
    }
    else
      pmath_unref(syms);
  }
  
  return expr;
}

static pmath_t get_parser_argument_from_string(pmath_string_t string) { // will be freed
  const uint16_t *str = pmath_string_buffer(&string);
  int             len = pmath_string_length(string);
  
  pmath_t result;
  pmath_t args;
  
  size_t argi = 1;
  if(len > 2) {
    int i;
    argi = 0;
    for(i = 1; i < len - 1; ++i)
      argi = 10 * argi + str[i] - '0';
  }
  
  args = pmath_thread_local_load(PMATH_THREAD_KEY_PARSERARGUMENTS);
  if(!pmath_is_expr(args)) {
    pmath_message(PMATH_NULL, "inv", 1, string);
    return pmath_ref(pmath_System_DollarFailed);
    //return HOLDCOMPLETE(args);
  }
  
  pmath_unref(string);
  result = pmath_expr_get_item(args, argi);
  pmath_unref(args);
  return HOLDCOMPLETE(result);
}

static pmath_t make_expression_from_name_token(pmath_string_t string) {
  pmath_t         obj;
  pmath_token_t   tok;
  const uint16_t *str = pmath_string_buffer(&string);
  int             len = pmath_string_length( string);
  
  int i = 1;
  while(i < len) {
    if(str[i] == '`') {
      if(i + 1 == len)
        break;
        
      ++i;
      tok = pmath_token_analyse(str + i, 1, NULL);
      if(tok != PMATH_TOK_NAME)
        break;
        
      ++i;
      continue;
    }
    
    tok = pmath_token_analyse(str + i, 1, NULL);
    if( tok != PMATH_TOK_NAME &&
        tok != PMATH_TOK_DIGIT)
    {
      break;
    }
    
    ++i;
  }
  
  if(i < len) {
    pmath_message(PMATH_NULL, "inv", 1, string);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  obj = pmath_thread_local_load(PMATH_THREAD_KEY_PARSESYMBOLS);
  pmath_unref(obj);
  
  obj = pmath_symbol_find(
          pmath_ref(string),
          pmath_same(obj, pmath_System_True) || pmath_same(obj, PMATH_UNDEFINED));
          
  if(!pmath_is_null(obj)) {
    pmath_unref(string);
    return HOLDCOMPLETE(obj);
  }
  
  pmath_message(PMATH_NULL, "nonewsym", 1, pmath_ref(string));
  return HOLDCOMPLETE(pmath_expr_new_extended(
                        pmath_ref(pmath_System_Symbol), 1,
                        string));
}

static pmath_t make_expression_from_string_token(pmath_string_t string) {
  const uint16_t *str = pmath_string_buffer(&string);
  int             len = pmath_string_length( string);
  
  pmath_string_t result = pmath_string_new_raw(len - 1);
  uint16_t *resbuf;
  if(pmath_string_begin_write(&result, &resbuf, NULL)) {
    int j = 0;
    int i = 1;
    int k = 0;
    
    while(i < len - 1) {
      if(k == 0 && str[i] == '\\') {
        uint32_t u;
        const uint16_t *end;
        
        if(i + 1 < len && str[i + 1] <= ' ') {
          ++i;
          while(i < len && str[i] <= ' ')
            ++i;
            
          continue;
        }
        
        end = pmath_char_parse(str + i, len - i, &u);
        
        if(u <= 0xFFFF) {
          resbuf[j++] = (uint16_t)u;
          i = (int)(end - str);
        }
        else if(u <= 0x10FFFF) {
          u -= 0x10000;
          resbuf[j++] = 0xD800 | (uint16_t)((u >> 10) & 0x03FF);
          resbuf[j++] = 0xDC00 | (uint16_t)(u & 0x03FF);
          i = (int)(end - str);
        }
        else {
          // TODO: error/warning
          resbuf[j++] = str[i++];
        }
      }
      else {
        if(str[i] == PMATH_CHAR_LEFT_BOX)
          ++k;
        else if(str[i] == PMATH_CHAR_RIGHT_BOX)
          --k;
          
        resbuf[j++] = str[i++];
      }
    }
    
    pmath_string_end_write(&result, &resbuf);
    if(i + 1 == len && str[i] == '"') {
      pmath_unref(string);
      return HOLDCOMPLETE(pmath_string_part(result, 0, j));
    }
  }
  pmath_unref(result);
  pmath_message(PMATH_NULL, "inv", 1, string);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_string_t unescape_chars(pmath_string_t str) {
  const uint16_t *buf = pmath_string_buffer(&str);
  int             len = pmath_string_length( str);
  int i;
  pmath_string_t result;
  
  i = 0;
  while(i < len && buf[i] != '\\')
    ++i;
    
  if(i == len)
    return str;
    
  result = pmath_string_new(len);
  while(i < len) {
    uint32_t u;
    uint16_t u16[2];
    const uint16_t *endchr;
    int endpos;
    
    endchr = pmath_char_parse(buf + i, len - i, &u);
    endpos = (int)(endchr - buf);
    
    result = pmath_string_insert_ucs2(result, INT_MAX, buf, i);
    
    if(u <= 0xFFFF) {
      u16[0] = (uint16_t)u;
      
      result = pmath_string_insert_ucs2(result, INT_MAX, u16, 1);
    }
    else if(u <= 0x10FFFF) {
      u -= 0x10000;
      u16[0] = 0xD800 | (uint16_t)((u >> 10) & 0x03FF);
      u16[1] = 0xDC00 | (uint16_t)(u & 0x03FF);
      
      result = pmath_string_insert_ucs2(result, INT_MAX, u16, 2);
    }
    else { // error
      result = pmath_string_insert_ucs2(result, INT_MAX, buf + i, endpos - i);
    }
    
    buf = endchr;
    i   = 0;
    len -= endpos;
  }
  
  pmath_unref(str);
  return result;
}

static pmath_t make_expression_from_string(pmath_string_t string) { // will be freed
  pmath_token_t   tok;
  const uint16_t *str = pmath_string_buffer(&string);
  int             len = pmath_string_length( string);
  
  if(len == 0) {
    pmath_unref(string);
    return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  if(str[0] == '"')
    return make_expression_from_string_token(string);
    
  if(len > 1 && str[0] == '`' && str[len - 1] == '`')
    return get_parser_argument_from_string(string);
    
    
  string = unescape_chars(string);
  str = pmath_string_buffer(&string);
  len = pmath_string_length( string);
  if(len == 0) {
    pmath_unref(string);
    return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  tok = pmath_token_analyse(str, 1, NULL);
  if(tok == PMATH_TOK_DIGIT) {
    pmath_number_t result = _pmath_parse_number(string);
    
    if(pmath_is_null(result))
      return pmath_ref(pmath_System_DollarFailed);
      
    return HOLDCOMPLETE(result);
  }
  
  if(tok == PMATH_TOK_NAME)
    return make_expression_from_name_token(string);
    
  if(tok == PMATH_TOK_NAME2) {
    pmath_t obj = pmath_thread_local_load(PMATH_THREAD_KEY_PARSESYMBOLS);
    pmath_unref(obj);
    
    obj = pmath_symbol_find(
            pmath_ref(string),
            pmath_same(obj, pmath_System_True) || pmath_same(obj, PMATH_UNDEFINED));
            
    if(!pmath_is_null(obj)) {
      pmath_unref(string);
      return HOLDCOMPLETE(obj);
    }
    
    return HOLDCOMPLETE(pmath_expr_new_extended(
                          pmath_ref(pmath_System_Symbol), 1,
                          string));
  }
  
  // now come special cases of generally longer expressions:
  
  if(tok == PMATH_TOK_NEWLINE || tok == PMATH_TOK_SPACE) {
    pmath_unref(string);
    return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  if(len == 1 && str[0] == '#') {
    pmath_unref(string);
    return HOLDCOMPLETE(
             pmath_expr_new_extended(
               pmath_ref(pmath_System_PureArgument), 1,
               PMATH_FROM_INT32(1)));
  }
  
  if(len == 1 && str[0] == ',') {
    pmath_unref(string);
    return pmath_expr_new_extended(
             pmath_ref(pmath_System_HoldComplete), 2,
             PMATH_NULL,
             PMATH_NULL);
  }
  
  if(len == 1 && str[0] == '~') {
    pmath_unref(string);
    return HOLDCOMPLETE(pmath_ref(_pmath_object_singlematch));
  }
  
  if(len == 2 && str[0] == '#' && str[1] == '#') {
    pmath_unref(string);
    return HOLDCOMPLETE(
             pmath_expr_new_extended(
               pmath_ref(pmath_System_PureArgument), 1,
               pmath_ref(_pmath_object_range_from_one)));
  }
  
  if(len == 2 && str[0] == '~' && str[1] == '~') {
    pmath_unref(string);
    return HOLDCOMPLETE(pmath_ref(_pmath_object_multimatch));
  }
  
  if(len == 2 && str[0] == '.' && str[1] == '.') {
    pmath_unref(string);
    return HOLDCOMPLETE(
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Range), 2,
               pmath_ref(pmath_System_Automatic),
               pmath_ref(pmath_System_Automatic)));
  }
  
  if(len == 3 && str[0] == '~' && str[1] == '~' && str[2] == '~') {
    pmath_unref(string);
    return HOLDCOMPLETE(pmath_ref(_pmath_object_zeromultimatch));
  }
  
  if(len == 3 && str[0] == '/' && str[1] == '\\' && str[2] == '/') {
    pmath_unref(string);
    return HOLDCOMPLETE(PMATH_NULL);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, string);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_string_t box_as_string(pmath_t box) {
  while(!pmath_is_null(box)) {
    if(pmath_is_string(box)) {
      const uint16_t *buf = pmath_string_buffer(&box);
      int             len = pmath_string_length(box);
      
      if(len >= 1 && buf[0] == '"') {
        pmath_t held_string = make_expression_from_string_token(pmath_ref(box));
        
        if( pmath_is_expr_of_len(held_string, pmath_System_HoldComplete, 1)) {
          pmath_t str = pmath_expr_get_item(held_string, 1);
          pmath_unref(held_string);
          
          if(pmath_is_string(str)) {
            pmath_unref(box);
            return str;
          }
          pmath_unref(str);
        }
      }
      
      return box;
    }
    
    if(pmath_is_expr(box) && pmath_expr_length(box) == 1) {
      pmath_t obj = pmath_expr_get_item(box, 1);
      pmath_unref(box);
      box = obj;
    }
    else {
      pmath_unref(box);
      return PMATH_NULL;
    }
  }
  
  return PMATH_NULL;
}

static pmath_t make_expression_from_stringbox(pmath_expr_t box) {
  size_t i, len;
  pmath_string_t string = PMATH_C_STRING("");
  static const uint16_t left_box_char  = PMATH_CHAR_LEFT_BOX;
  static const uint16_t right_box_char = PMATH_CHAR_RIGHT_BOX;
  
  len = pmath_expr_length(box);
  for(i = 1; i <= len; ++i) {
    pmath_t part = pmath_expr_get_item(box, i);
    
    if(pmath_is_string(part)) {
      string = pmath_string_concat(string, part);
      continue;
    }
    
    string = pmath_string_insert_ucs2(string, INT_MAX, &left_box_char, 1);
    
    pmath_write(
      part,
      PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR,
      _pmath_write_to_string,
      &string);
      
    pmath_unref(part);
    string = pmath_string_insert_ucs2(string, INT_MAX, &right_box_char, 1);
  }
  
  pmath_unref(box);
  return make_expression_from_string(string);
}

static pmath_t make_expression_from_compresseddata(pmath_expr_t box) {
  if(pmath_expr_length(box) == 1) {
    pmath_t data = pmath_expr_get_item(box, 1);
    if(pmath_is_string(data)) {
      data = pmath_decompress_from_string(data);
      if(!pmath_same(data, PMATH_UNDEFINED))
        return wrap_hold_with_debuginfo_from(box, data);
    }
    pmath_unref(data);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_fractionbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len == 2) {
    pmath_t num = pmath_expr_get_item(box, 1);
    pmath_t den = pmath_expr_get_item(box, 2);
    
    if(parse(&num) && parse(&den)) {
      if(pmath_is_integer(num) && pmath_is_integer(den))
        return wrap_hold_with_debuginfo_from(box, pmath_rational_new(num, den));
        
      if(pmath_same(num, INT(1))) {
        pmath_unref(num);
        
        return wrap_hold_with_debuginfo_from(box, INV(den));
      }
      
      return wrap_hold_with_debuginfo_from(box, DIV(num, den));
    }
    
    pmath_unref(num);
    pmath_unref(den);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_framebox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 1) {
    pmath_t content = pmath_expr_get_item(box, 1);
    
    if(parse(&content)) {
      content = pmath_expr_new_extended(
                  pmath_ref(pmath_System_Framed), 1,
                  content);
                  
      return wrap_hold_with_debuginfo_from(box, content);
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_gridbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 1) {
    pmath_t result = parse_gridbox(box, TRUE);
    
    if(!pmath_is_null(result)) {
      pmath_unref(box);
      return result;
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_gridbox_column(pmath_t box, pmath_expr_t gridbox) {
  pmath_t held   = parse_gridbox(gridbox, FALSE);
  pmath_t grid   = pmath_expr_get_item(held, 1);
  pmath_t matrix = pmath_expr_get_item(grid, 1);
  pmath_t row    = pmath_expr_get_item(matrix, 1);
  
  if(pmath_expr_length(row) == 1) {
    size_t i;
    
    pmath_unref(held);
    pmath_unref(grid);
    pmath_unref(row);
    pmath_unref(gridbox);
    
    for(i = pmath_expr_length(matrix); i > 0; --i) {
      row = pmath_expr_get_item(matrix, i);
      
      matrix = pmath_expr_set_item(matrix, i,
                                   pmath_expr_get_item(row, 1));
                                   
      pmath_unref(row);
    }
    
    return wrap_hold_with_debuginfo_from(
             box,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Column), 1,
               matrix));
  }
  
  pmath_unref(grid);
  pmath_unref(matrix);
  pmath_unref(row);
  
  if(!pmath_is_null(held)) {
    pmath_unref(box);
    return held;
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_interpretationbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 2) {
    pmath_expr_t options = pmath_options_extract(box, 2);
    pmath_unref(options);
    
    if(!pmath_is_null(options)) {
      pmath_t value = pmath_expr_get_item(box, 2);
      
      return wrap_hold_with_debuginfo_from(box, value);
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_overscriptbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len == 2) {
    pmath_t base = pmath_expr_get_item(box, 1);
    pmath_t over = pmath_expr_get_item(box, 2);
    
    if(parse(&base) && parse(&over)) {
      pmath_t result;
      if(pmath_is_expr_of_len(over, pmath_System_Sequence, 0)) {
        pmath_unref(over);
        result = base;
      }
      else 
        result = pmath_expr_new_extended(pmath_ref(pmath_System_Overscript), 2, base, over);
      
      return wrap_hold_with_debuginfo_from(box, result);
    }
    
    pmath_unref(base);
    pmath_unref(over);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_radicalbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  pmath_bool_t is_surd = FALSE;
  
  if(len > 2) {
    pmath_t options = pmath_options_extract(box, 2);
    if(!pmath_is_null(options)) {
      pmath_t surdform = pmath_option_value(pmath_System_RadicalBox, pmath_System_SurdForm, options);
      is_surd = pmath_same(surdform, pmath_System_True);
      pmath_unref(surdform);
      pmath_unref(options);
      
      len = 2;
    }
  }
  
  if(len == 2) {
    pmath_t base     = pmath_expr_get_item(box, 1);
    pmath_t exponent = pmath_expr_get_item(box, 2);
    
    if(parse(&base) && parse(&exponent)) {
      if(is_surd)
        base = FUNC2(pmath_ref(pmath_System_Surd), base, exponent);
      else
        base = POW(base, INV(exponent));
        
      return wrap_hold_with_debuginfo_from(box, base);
    }
    
    pmath_unref(base);
    pmath_unref(exponent);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_rotationbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 1) {
    pmath_t options = pmath_options_extract(box, 1);
    
    if(!pmath_is_null(options)) {
      pmath_t content = pmath_expr_get_item(box, 1);
      
      pmath_t angle = pmath_option_value(
                        pmath_System_RotationBox,
                        pmath_System_BoxRotation,
                        options);
                        
      pmath_unref(options);
      
      if(parse(&content)) {
        return wrap_hold_with_debuginfo_from(
                 box,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_Rotate), 2,
                   content,
                   angle));
      }
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_sqrtbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  pmath_bool_t is_surd = FALSE;
  
  if(len > 1) {
    pmath_t options = pmath_options_extract(box, 1);
    if(!pmath_is_null(options)) {
      pmath_t surdform = pmath_option_value(pmath_System_SqrtBox, pmath_System_SurdForm, options);
      is_surd = pmath_same(surdform, pmath_System_True);
      pmath_unref(surdform);
      pmath_unref(options);
      
      len = 1;
    }
  }
  
  if(len == 1) {
    pmath_t base = pmath_expr_get_item(box, 1);
    
    if(parse(&base)) {
      if(is_surd)
        base = FUNC2(pmath_ref(pmath_System_Surd), base, INT(2));
      else
        base = SQRT(base);
        
      return wrap_hold_with_debuginfo_from(box, base);
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_stylebox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 1) {
    pmath_t content = pmath_expr_get_item(box, 1);
    
    if(parse(&content)) {
      content = pmath_expr_new_extended(
                  pmath_ref(pmath_System_Style), 1,
                  content);
                  
      return wrap_hold_with_debuginfo_from(box, content);
    }
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_tagbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len == 2) {
    pmath_t view = pmath_expr_get_item(box, 1);
    pmath_t tag  = pmath_expr_get_item(box, 2);
    
    if(pmath_is_string(tag)) {
    
      if( pmath_string_equals_latin1(tag, "Column") &&
          pmath_is_expr_of(view, pmath_System_GridBox))
      {
        pmath_unref(tag);
        return make_expression_from_gridbox_column(box, view);
      }
      
      if(pmath_string_equals_latin1(tag, "Placeholder")) {
        if(pmath_is_expr_of(view, pmath_System_FrameBox)) {
          pmath_t tmp = pmath_expr_get_item(view, 1);
          pmath_unref(view);
          view = tmp;
        }
        
        if(parse(&view)) {
          pmath_unref(tag);
          return wrap_hold_with_debuginfo_from(
                   box,
                   pmath_expr_new_extended(
                     pmath_ref(pmath_System_Placeholder), 1,
                     view));
        }
        
        pmath_unref(view);
        pmath_unref(tag);
        goto FAILED;
      }
      
      if(pmath_string_equals_latin1(tag, "Grid") && pmath_is_expr_of(view, pmath_System_GridBox)) {
        pmath_t grid = parse_gridbox(view, FALSE);
        
        pmath_unref(view);
        pmath_unref(tag);
        
        if(!pmath_is_null(grid)) {
          pmath_unref(box);
          return grid;
        }
        
        goto FAILED;
      }
    }
    
    if(pmath_same(tag, pmath_System_Column) && pmath_is_expr_of(view, pmath_System_GridBox)) {
      pmath_unref(tag);
      return make_expression_from_gridbox_column(box, view);
    }
    
    if(parse(&view)) {
      return wrap_hold_with_debuginfo_from(
               box,
               pmath_expr_new_extended(
                 tag, 1,
                 view));
    }
    
    pmath_unref(view);
    pmath_unref(tag);
  }
  
FAILED:
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_templatebox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len >= 2) {
    pmath_t tag;
    pmath_t func;
    pmath_t args = pmath_expr_get_item(box, 1);
    size_t i;
    size_t argcount;
    
    if(!pmath_is_expr_of(args, pmath_System_List)) {
      pmath_unref(args);
      pmath_message(PMATH_NULL, "inv", 1, box);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    argcount = pmath_expr_length(args);
    func = pmath_expr_new_extended(
             pmath_ref(pmath_System_Private_FlattenTemplateSequence), 2,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Private_FindTemplateInterpretationFunction), 1, 
               pmath_ref(box)),
             pmath_integer_new_uiptr(argcount));
    func = pmath_evaluate(func);
    if(pmath_is_expr_of_len(func, pmath_System_Function, 1)) {
      pmath_t body = pmath_expr_get_item(func, 1);
      pmath_unref(func);
      func = pmath_expr_new_extended(
               pmath_ref(pmath_System_Function), 3,
               PMATH_NULL,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_MakeExpression), 1,
                 body),
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_List), 1,
                 pmath_ref(pmath_System_HoldAllComplete)));
      
      pmath_unref(box);
      box = pmath_expr_set_item(args, 0, func);
      return box;
    }
    pmath_unref(func);
    
    tag = pmath_expr_get_item(box, 2);
    if(pmath_is_string(tag)) {
      pmath_unref(box);
      if(argcount > 1) {
        box = pmath_expr_new(pmath_ref(pmath_System_List), argcount + (argcount - 1));
        box = pmath_expr_set_item(box, 1, pmath_expr_get_item(args, 1));
        for(i = 1; i < argcount; ++i) {
          box = pmath_expr_set_item(box, 2 * i, PMATH_C_STRING(","));
          box = pmath_expr_set_item(box, 2 * i + 1, pmath_expr_get_item(args, i + 1));
        }
        pmath_unref(args);
        args = box;
      }
      box = pmath_expr_new_extended(
              pmath_ref(pmath_System_List), 4,
              tag,
              PMATH_C_STRING("("),
              args,
              PMATH_C_STRING(")"));
      box = pmath_expr_new_extended(
              pmath_ref(pmath_System_MakeExpression), 1,
              box);
      return box;
    }
    pmath_unref(tag);
    pmath_unref(args);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_underscriptbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len == 2) {
    pmath_t base  = pmath_expr_get_item(box, 1);
    pmath_t under = pmath_expr_get_item(box, 2);
    
    if(parse(&base) && parse(&under)) {
      pmath_t result;
      if(pmath_is_expr_of_len(under, pmath_System_Sequence, 0)) {
        pmath_unref(under);
        result = base;
      }
      else 
        result = pmath_expr_new_extended(pmath_ref(pmath_System_Underscript), 2, base, under);
      
      return wrap_hold_with_debuginfo_from(box, result);
    }
    
    pmath_unref(base);
    pmath_unref(under);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_expression_from_underoverscriptbox(pmath_expr_t box) {
  size_t len = pmath_expr_length(box);
  
  if(len == 3) {
    pmath_t base  = pmath_expr_get_item(box, 1);
    pmath_t under = pmath_expr_get_item(box, 2);
    pmath_t over  = pmath_expr_get_item(box, 3);
    
    if(parse(&base) && parse(&under) && parse(&over)) {
      pmath_t result;
      if(pmath_is_expr_of_len(under, pmath_System_Sequence, 0)) {
        pmath_unref(under);
        if(pmath_is_expr_of_len(over, pmath_System_Sequence, 0)) {
          pmath_unref(over);
          result = base;
        }
        else
          result = pmath_expr_new_extended(pmath_ref(pmath_System_Overscript), 2, base, over);
      }
      else if(pmath_is_expr_of_len(over, pmath_System_Sequence, 0)) {
        pmath_unref(over);
        result = pmath_expr_new_extended(pmath_ref(pmath_System_Underscript), 2, base, under);
      }
      else {
        result = pmath_expr_new_extended(
                   pmath_ref(pmath_System_Underoverscript), 3,
                   base, under, over);
      }
      
      return wrap_hold_with_debuginfo_from(box, result);
    }
    
    pmath_unref(base);
    pmath_unref(over);
  }
  
  pmath_message(PMATH_NULL, "inv", 1, box);
  return pmath_ref(pmath_System_DollarFailed);
}

// (x)
static pmath_t make_parenthesis(pmath_expr_t boxes) {
  pmath_t box;
  size_t exprlen = pmath_expr_length(boxes);
  
  if(exprlen == 2) {
    pmath_unref(boxes);
    return pmath_expr_new(
             pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  if(exprlen > 3) {
    pmath_message(PMATH_NULL, "inv", 1, boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  box = pmath_expr_get_item(boxes, 2);
  pmath_unref(boxes);
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_MakeExpression), 1, box);
}

// a,b,c ...
static pmath_t make_comma_sequence(pmath_expr_t expr) {
  pmath_t prev = PMATH_NULL;
  uint16_t ch = unichar_at(expr, 1);
  pmath_bool_t last_was_comma = ch == ',' || ch == PMATH_CHAR_INVISIBLECOMMA;
  size_t i;
  size_t exprlen = pmath_expr_length(expr);
  
  if(!last_was_comma) {
    prev = parse_at(expr, 1);
    
    if(is_parse_error(prev)) {
      pmath_unref(expr);
      return pmath_ref(pmath_System_DollarFailed);
    }
    i = 2;
  }
  else
    i = 1;
    
  pmath_gather_begin(PMATH_NULL);
  
  while(i <= exprlen) {
    ch = unichar_at(expr, i);
    if(ch == ',' || ch == PMATH_CHAR_INVISIBLECOMMA) {
      last_was_comma = TRUE;
      pmath_emit(prev, PMATH_NULL);
      prev = PMATH_NULL;
    }
    else if(!last_was_comma) {
      last_was_comma = FALSE;
      pmath_message(PMATH_NULL, "inv", 1, expr);
      pmath_unref(pmath_gather_end());
      pmath_unref(prev);
      return pmath_ref(pmath_System_DollarFailed);
    }
    else {
      last_was_comma = FALSE;
      prev = parse_at(expr, i);
      if(is_parse_error(prev)) {
        pmath_unref(expr);
        pmath_unref(pmath_gather_end());
        return pmath_ref(pmath_System_DollarFailed);
      }
    }
    ++i;
  }
  pmath_emit(prev, PMATH_NULL);
  
  pmath_unref(expr);
  return pmath_expr_set_item(
           pmath_gather_end(), 0,
           pmath_ref(pmath_System_HoldComplete));
}

// a; b; c\[RawNewline]d ...
static pmath_t make_evaluation_sequence(pmath_expr_t boxes) {
  pmath_t prev = PMATH_NULL;
  pmath_bool_t last_was_semicolon = TRUE;
  size_t i;
  size_t exprlen = pmath_expr_length(boxes);
  
  i = 1;
  while(i <= exprlen && unichar_at(boxes, i) == '\n')
    ++i;
    
  while(i <= exprlen && unichar_at(boxes, exprlen) == '\n')
    --exprlen;
    
  if(i > exprlen) {
    pmath_unref(boxes);
    return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  if(i == exprlen) {
    prev = parse_at(boxes, i);
    pmath_unref(boxes);
    
    if(is_parse_error(prev))
      return pmath_ref(pmath_System_DollarFailed);
      
    return HOLDCOMPLETE(prev);
  }
  
  pmath_gather_begin(PMATH_NULL);
  
  while(i <= exprlen) {
    uint16_t ch = unichar_at(boxes, i);
    if(ch == ';') {
      last_was_semicolon = TRUE;
      pmath_emit(prev, PMATH_NULL);
      prev = PMATH_NULL;
    }
    else if(ch == '\n') {
      last_was_semicolon = TRUE;
      if(!pmath_is_null(prev)) {
        pmath_emit(prev, PMATH_NULL);
        prev = PMATH_NULL;
      }
    }
    else if(!last_was_semicolon) {
      last_was_semicolon = FALSE;
      pmath_message(PMATH_NULL, "inv", 1, boxes);
      pmath_unref(pmath_gather_end());
      pmath_unref(prev);
      return pmath_ref(pmath_System_DollarFailed);
    }
    else {
      last_was_semicolon = FALSE;
      prev = parse_at(boxes, i);
      if(is_parse_error(prev)) {
        pmath_unref(boxes);
        pmath_unref(pmath_gather_end());
        return pmath_ref(pmath_System_DollarFailed);
      }
    }
    ++i;
  }
  pmath_emit(prev, PMATH_NULL);
  
  return wrap_hold_with_debuginfo_from(
           boxes,
           pmath_expr_set_item(
             pmath_gather_end(), 0,
             pmath_ref(pmath_System_EvaluationSequence)));
}

// implicit evaluation sequence (newlines -> head = /\/ = PMATH_NULL) ...
static pmath_t make_implicit_evaluation_sequence(pmath_expr_t boxes) {
  size_t i, exprlen;
  pmath_t result;
  
  exprlen = pmath_expr_length(boxes);
  if(exprlen == 0) {
    pmath_unref(boxes);
    return pmath_expr_new(pmath_ref(pmath_System_HoldComplete), 0);
  }
  
  result = pmath_expr_new(pmath_ref(pmath_System_EvaluationSequence), exprlen);
  
  for(i = 1; i <= exprlen; ++i) {
    pmath_t box = pmath_expr_get_item(boxes, i);
    
    if(!parse(&box)) {
      pmath_unref(box);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    result = pmath_expr_set_item(result, i, box);
  }
  
  return wrap_hold_with_debuginfo_from(boxes, result);
}

// ?x  ?x:v
static pmath_t make_optional_pattern(pmath_expr_t boxes) {
  pmath_t box = parse_at(boxes, 2);
  
  if(!is_parse_error(box)) {
    size_t exprlen = pmath_expr_length(boxes);
    
    box = pmath_expr_new_extended(
            pmath_ref(pmath_System_Pattern), 2,
            box,
            pmath_ref(_pmath_object_singlematch));
            
    if(exprlen == 2) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Optional), 1,
                 box));
    }
    
    if(exprlen == 4 && unichar_at(boxes, 3) == ':') {
      pmath_t value = parse_at(boxes, 4);
      
      if(!is_parse_error(value)) {
        return wrap_hold_with_debuginfo_from(
                 boxes,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_Optional), 2,
                   box,
                   value));
      }
    }
    
    pmath_unref(box);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_derivative(pmath_expr_t boxes, int order) {
  pmath_t box = parse_at(boxes, 1);
  
  if(!is_parse_error(box)) {
    pmath_t snd = pmath_expr_get_item(boxes, 2);
    pmath_t snd_debug_info = pmath_get_debug_info(snd);
    pmath_unref(snd);
    snd = pmath_expr_new_extended(
            pmath_ref(pmath_System_Derivative), 1,
            PMATH_FROM_INT32(order));
    snd = pmath_try_set_debug_info(snd, snd_debug_info);
    
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               snd, 1,
               box));
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_from_first_box(
  pmath_expr_t   boxes,   // will be freed
  pmath_symbol_t sym      // wont be freed
) {
  pmath_t box = parse_at(boxes, 1);
  
  if(!is_parse_error(box)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(sym), 1,
               box));
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_from_second_box(
  pmath_expr_t boxes,  // will be freed
  pmath_symbol_t sym  // wont be freed
) {
  pmath_t box = parse_at(boxes, 2);
  
  if(!is_parse_error(box)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(sym), 1,
               box));
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// {}  {args}  \[LeftCeiling]arg\[RightCeiling]  ...
static pmath_t make_matchfix(pmath_expr_t boxes, pmath_symbol_t sym) {
  pmath_t args;
  size_t exprlen = pmath_expr_length(boxes);
  
  if(exprlen == 2) {
    pmath_unref(boxes);
    if(pmath_same(sym, pmath_System_List))
      return HOLDCOMPLETE(pmath_ref(_pmath_object_emptylist));
    else
      return HOLDCOMPLETE(pmath_expr_new(pmath_ref(sym), 0));
  }
  
  if(exprlen != 3) {
    pmath_message(PMATH_NULL, "inv", 1, boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  args = _pmath_makeexpression_with_debuginfo(pmath_expr_get_item(boxes, 2));
  
  if(pmath_is_expr(args)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_set_item(args, 0, pmath_ref(sym)));
  }
  
  pmath_unref(boxes);
  pmath_unref(args);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_binary(
  pmath_expr_t boxes,  // will be freed
  pmath_symbol_t sym   // wont be freed
) {
  pmath_t lhs = parse_at(boxes, 1);
  
  if(!is_parse_error(lhs)) {
    pmath_t rhs;
    
    if( pmath_same(sym, pmath_System_Assign) &&
        unichar_at(boxes, 3) == '.')
    {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Unassign), 1,
                 lhs));
    }
    
    rhs = parse_at(boxes, 3);
    if(!is_parse_error(rhs)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(sym), 2,
                 lhs,
                 rhs));
    }
    
    pmath_unref(lhs);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_pattern_op_other(pmath_expr_t boxes, pmath_symbol_t sym) { // boxes will be freed, sym won't
  pmath_t pat = parse_seq_at(boxes, 1, pmath_System_PatternSequence);
  
  if(!is_parse_error(pat)) {
    pmath_t other = parse_at(boxes, 3);
    if(!is_parse_error(other)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(sym), 2,
                 pat,
                 other));
    }
    
    pmath_unref(pat);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_infix_unchecked(
  pmath_expr_t boxes,  // will be freed
  pmath_symbol_t sym  // wont be freed
) {
  size_t i, exprlen;
  pmath_expr_t result;
  
  exprlen = pmath_expr_length(boxes);
  
  result = pmath_expr_new(pmath_ref(sym), (1 + exprlen) / 2);
  for(i = 0; i <= exprlen / 2; ++i) {
    pmath_t arg = parse_at(boxes, 2 * i + 1);
    
    if(is_parse_error(arg)) {
      pmath_unref(result);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    result = pmath_expr_set_item(result, i + 1, arg);
  }
  
  return wrap_hold_with_debuginfo_from(boxes, result);
}

static pmath_t make_infix(
  pmath_expr_t boxes,  // will be freed
  pmath_symbol_t sym  // wont be freed
) {
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(boxes);
  
  for(i = 4; i < exprlen; i += 2) {
    if(!pmath_same(inset_operator(unichar_at(boxes, i)), sym)) {
      handle_row_error_at(boxes, i);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
  }
  
  return make_infix_unchecked(boxes, sym);
}

// a < b <= c ...
static pmath_t make_relation(
  pmath_expr_t   boxes,  // will be freed
  pmath_symbol_t rel     // wont be freed
) {
  pmath_expr_t result = pmath_ref(boxes);
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(boxes);
  
  for(i = 4; i < exprlen; ++i) {
    if(!pmath_same(rel, relation_at(boxes, i))) {
      pmath_t arg = parse_at(boxes, 1);
      
      if(is_parse_error(arg)) {
        pmath_unref(boxes);
        return pmath_ref(pmath_System_DollarFailed);
      }
      
      result = pmath_expr_set_item(result, 1, arg);
      for(i = 3; i <= exprlen; i += 2) {
        rel = relation_at(boxes, i - 1);
        
        if(pmath_is_null(rel)) {
          handle_row_error_at(boxes, i - 1);
          pmath_unref(boxes);
          pmath_unref(result);
          return pmath_ref(pmath_System_DollarFailed);
        }
        
        result = pmath_expr_set_item(result, i - 1, pmath_ref(rel));
        
        arg = parse_at(boxes, i);
        if(is_parse_error(arg)) {
          pmath_unref(boxes);
          pmath_unref(result);
          return pmath_ref(pmath_System_DollarFailed);
        }
        
        result = pmath_expr_set_item(result, i, arg);
      }
      
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_set_item(
                 result, 0,
                 pmath_ref(pmath_System_Inequation)));
    }
  }
  
  pmath_unref(result);
  result = pmath_expr_new(pmath_ref(rel), (1 + exprlen) / 2);
  for(i = 0; i <= exprlen / 2; ++i) {
    pmath_t arg = parse_at(boxes, 2 * i + 1);
    
    if(is_parse_error(arg)) {
      pmath_unref(result);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    result = pmath_expr_set_item(result, i + 1, arg);
  }
  
  return wrap_hold_with_debuginfo_from(boxes, result);
}

// x**  x***
static pmath_t make_repeated_pattern(
  pmath_expr_t boxes,   // will be freed
  pmath_t      range    // will be freed
) {
  pmath_t box = parse_at(boxes, 1);
  
  if(!is_parse_error(box)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Repeated), 2,
               box,
               range));
  }
  
  pmath_unref(boxes);
  pmath_unref(range);
  return pmath_ref(pmath_System_DollarFailed);
}

// +x
static pmath_t make_unary_plus(pmath_expr_t boxes) {
  pmath_t box = pmath_expr_get_item(boxes, 2);
  pmath_unref(boxes);
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_MakeExpression), 1, box);
}

// -x
static pmath_t make_unary_minus(pmath_expr_t boxes) {
  pmath_t box = parse_at(boxes, 2);
  
  if(!is_parse_error(box)) {
    if(pmath_is_number(box)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_number_neg(box));
    }
    
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Times), 2,
               PMATH_FROM_INT32(-1),
               box));
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// ##1
static pmath_t make_pure_argument_range(pmath_expr_t boxes) {
  pmath_t box = parse_at(boxes, 2);
  
  if(is_parse_error(box)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(pmath_is_integer(box)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_PureArgument), 1,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Range), 2,
                 box,
                 pmath_ref(pmath_System_Automatic))));
  }
  
  pmath_unref(box);
  pmath_message(PMATH_NULL, "inv", 1, boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// ??name  <<file
static pmath_t make_text_line(
  pmath_expr_t boxes,  // will be freed
  pmath_symbol_t sym  // wont be freed
) {
  pmath_t box = pmath_expr_get_item(boxes, 2);
  
  if(!pmath_is_string(box)              ||
      pmath_string_length(box)     == 0 ||
      pmath_string_buffer(&box)[0] == '"')
  {
    if(!parse(&box)) {
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
  }
  
  return wrap_hold_with_debuginfo_from(
           boxes,
           pmath_expr_new_extended(
             pmath_ref(sym), 1,
             box));
}

// ~x  ~~x  ~~~x
static pmath_t make_named_match(
  pmath_expr_t boxes,   // will be freed
  pmath_t range        // will be freed
) {
  pmath_t box = parse_at(boxes, 2);
  
  if(!is_parse_error(box)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Pattern), 2,
               box,
               range));
  }
  
  pmath_unref(boxes);
  pmath_unref(range);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_superscript(pmath_expr_t boxes, pmath_expr_t superscript_box) {
  pmath_t base = pmath_expr_get_item(boxes, 1);
  pmath_t exp  = pmath_expr_get_item(superscript_box,  1);
  
  pmath_unref(superscript_box);
  
  if(try_parse_helper(pmath_System_Private_MakeScriptsExpression, &boxes)) {
    pmath_unref(base);
    pmath_unref(exp);
    return boxes;
  }
  
  if(parse(&base)) {
    if(pmath_is_string(exp)) {
      pmath_string_t exp_debug_info;
      int order = 0;
      int len = pmath_string_length(exp);
      const uint16_t *exp_buf = pmath_string_buffer(&exp);
      int i;
      for(i = 0; i < len; ++i) {
        switch(exp_buf[i]) {
          case 0x2032: order+= 1; break; // \[Prime]
          case 0x2033: order+= 2; break; // \[DoublePrime]
          case 0x2034: order+= 3; break; // \[TriplePrime]
          case 0x2035: order+= 1; break; // \[ReversedPrime]
          case 0x2036: order+= 2; break; // \[ReversedDoublePrime]
          case 0x2037: order+= 3; break; // \[ReversedTriplePrime]
          case 0x2057: order+= 4; break; // \[QuadruplePrime]
          default: 
            goto NO_DERIVATIVE;
        }
      }
      exp_debug_info = pmath_get_debug_info(exp);
      pmath_unref(exp);
      exp = pmath_expr_new_extended(
              pmath_ref(pmath_System_Derivative), 1,
              PMATH_FROM_INT32(order));
      exp = pmath_try_set_debug_info(exp, exp_debug_info);
      return wrap_hold_with_debuginfo_from(boxes, pmath_expr_new_extended(exp, 1, base));
    NO_DERIVATIVE: ;
    }
    
    if(pmath_is_expr_of(exp, pmath_System_TagBox)) {
      pmath_t tag = pmath_expr_get_item(exp, 2);
      pmath_unref(tag);
      
      if(pmath_same(tag, pmath_System_Derivative)) {
        if(parse(&exp)) 
          return wrap_hold_with_debuginfo_from(boxes, pmath_expr_new_extended(exp, 1, base));
      
        pmath_unref(boxes);
        pmath_unref(base);
        pmath_unref(exp);
        return pmath_ref(pmath_System_DollarFailed);
      }
      
      // TODO: InverseFunction
      
      // Note that Mma parses SuperscriptBox("f", TagBox(..., g)) as g(f^...) for other symbols g
      // and SuperscriptBox("f", TagBox(..., "g")) as f^(g(...)) for string tags "g"
    }
    
    if(parse(&exp)) {
      return wrap_hold_with_debuginfo_from(boxes, POW(base, exp));
    }
  }
  
  pmath_unref(boxes);
  pmath_unref(base);
  pmath_unref(exp);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_subscript(pmath_expr_t boxes, pmath_expr_t subscript_box) {
  pmath_t base = pmath_expr_get_item(boxes, 1);
  
  if(try_parse_helper(pmath_System_Private_MakeScriptsExpression, &boxes)) {
    pmath_unref(base);
    pmath_unref(subscript_box);
    return boxes;
  }
  
  if(parse(&base)) {
    pmath_t idx = pmath_expr_get_item(subscript_box, 1);
    pmath_unref(subscript_box);
    
    idx = _pmath_makeexpression_with_debuginfo(idx);
    
    if(pmath_is_expr(idx)) {
      pmath_t head = pmath_expr_get_item(idx, 0);
      pmath_unref(head);
      
      if(pmath_same(head, pmath_System_HoldComplete)) {
        size_t exprlen = pmath_expr_length(idx) + 1;
        
        pmath_t result = pmath_expr_new(
                           pmath_ref(pmath_System_Subscript),
                           exprlen);
                           
        result = pmath_expr_set_item(result, 1, base);
        
        for(; exprlen > 1; --exprlen) {
          result = pmath_expr_set_item(
                     result, exprlen,
                     pmath_expr_get_item(idx, exprlen - 1));
        }
        
        pmath_unref(idx);
        return wrap_hold_with_debuginfo_from(boxes, result);
      }
    }
    
    pmath_unref(idx);
  }
  else
    pmath_unref(subscript_box);
    
  pmath_unref(boxes);
  pmath_unref(base);
  return pmath_ref(pmath_System_DollarFailed);
}

static pmath_t make_subsuperscript(pmath_expr_t boxes, pmath_expr_t subsuperscript_box) {
  pmath_t idx;
  pmath_t exp;
  pmath_t debug_info;
  
  if(try_parse_helper(pmath_System_Private_MakeScriptsExpression, &boxes)) {
    pmath_unref(subsuperscript_box);
    return boxes;
  }
  
  idx        = pmath_expr_get_item(subsuperscript_box,  1);
  exp        = pmath_expr_get_item(subsuperscript_box,  2);
  debug_info = pmath_get_debug_info(subsuperscript_box);
  pmath_unref(subsuperscript_box);
  subsuperscript_box = pmath_expr_new_extended(
                         pmath_ref(pmath_System_SubscriptBox), 1,
                         idx);
  subsuperscript_box = pmath_try_set_debug_info(subsuperscript_box, debug_info);
  
  debug_info = pmath_get_debug_info(boxes);
  boxes = pmath_expr_set_item(boxes, 2, subsuperscript_box);
  boxes = pmath_try_set_debug_info(boxes, pmath_ref(debug_info));
  
  boxes = pmath_expr_new_extended(
            pmath_ref(pmath_System_List), 2,
            boxes,
            pmath_expr_new_extended(
              pmath_ref(pmath_System_SuperscriptBox), 1,
              exp));
  boxes = pmath_try_set_debug_info(boxes, debug_info);
  
  return pmath_expr_new_extended(
           pmath_ref(pmath_System_MakeExpression), 1,
           boxes);
}

// a.f   a.f()
static pmath_t make_simple_dot_call(pmath_expr_t boxes) {
  pmath_t arg = parse_at(boxes, 1);
  
  if(!is_parse_error(arg)) {
    pmath_t f = parse_at(boxes, 3);
    
    if(!is_parse_error(f)) {
      return wrap_hold_with_debuginfo_from(boxes, pmath_expr_new_extended(f, 1, arg));
    }
    
    pmath_unref(arg);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// a.f(args)
static pmath_t make_dot_call(pmath_expr_t boxes) {
  pmath_t args, arg1, f;
  
  arg1 = parse_at(boxes, 1);
  if(is_parse_error(arg1)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  f = parse_at(boxes, 3);
  if(is_parse_error(f)) {
    pmath_unref(arg1);
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  args = _pmath_makeexpression_with_debuginfo(pmath_expr_get_item(boxes, 5));
  
  if(pmath_is_expr(args)) {
    size_t i, argslen;
    
    argslen = pmath_expr_length(args);
    
    args = pmath_expr_resize(args, argslen + 1);
    
    for(i = argslen; i > 0; --i) {
      args = pmath_expr_set_item(
               args, i + 1,
               pmath_expr_get_item(args, i));
    }
    
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_set_item(
               pmath_expr_set_item(
                 args, 0,
                 f), 1,
               arg1));
  }
  
  pmath_unref(arg1);
  pmath_unref(f);
  pmath_unref(args);
  pmath_message(PMATH_NULL, "inv", 1, boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// f @ a
static pmath_t make_prefix_call(pmath_expr_t boxes) {
  pmath_t f = parse_at(boxes, 1);
  
  if(!is_parse_error(f)) {
    pmath_t arg = parse_seq_at(boxes, 3, PMATH_MAGIC_PATTERN_SEQUENCE);
    
    if(!is_parse_error(arg)) {
      if(pmath_is_expr_of(arg, PMATH_MAGIC_PATTERN_SEQUENCE)) 
        arg = pmath_expr_set_item(arg, 0, f);
      else
        arg = pmath_expr_new_extended(f, 1, arg);
        
      return wrap_hold_with_debuginfo_from(boxes, arg);
    }
    
    pmath_unref(f);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// a // f
static pmath_t make_postfix_call(pmath_expr_t boxes) {
  pmath_t arg = parse_at(boxes, 1);
  
  if(!is_parse_error(arg)) {
    pmath_t f = parse_at(boxes, 3);
    
    if(!is_parse_error(f)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(f, 1, arg));
    }
    
    pmath_unref(arg);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// f()
static pmath_t make_argumentless_call(pmath_expr_t boxes) {
  pmath_t box = parse_at(boxes, 1);
  
  if(!is_parse_error(box)) {
    return wrap_hold_with_debuginfo_from(boxes, pmath_expr_new(box, 0));
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// f(args)
static pmath_t make_simple_call(pmath_expr_t boxes) {
  pmath_t args = _pmath_makeexpression_with_debuginfo(
                   pmath_expr_get_item(boxes, 3));
                   
  if(pmath_is_expr(args)) {
    pmath_t f = parse_at(boxes, 1);
    
    if(!is_parse_error(f)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_set_item(args, 0, f));
    }
  }
  
  pmath_unref(args);
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// x:p   ~:t  ~~:t  ~~~:t
static pmath_t make_pattern_or_typed_match(pmath_expr_t boxes) {
  pmath_t x, box;
  uint16_t firstchar = unichar_at(boxes, 1);
  
  box = parse_at(boxes, 3);
  if(is_parse_error(box)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(firstchar == '~') {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_SingleMatch), 1,
               box));
  }
  
  if(is_string_at(boxes, 1, "~~")) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Repeated), 2,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_SingleMatch), 1,
                 box),
               pmath_ref(_pmath_object_range_from_one)));
  }
  
  if(is_string_at(boxes, 1, "~~~")) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Repeated), 2,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_SingleMatch), 1,
                 box),
               pmath_ref(_pmath_object_range_from_zero)));
  }
  
  x = parse_at(boxes, 1);
  
  if(!is_parse_error(x)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Pattern), 2,
               x,
               box));
  }
  
  pmath_unref(boxes);
  pmath_unref(box);
  return pmath_ref(pmath_System_DollarFailed);
}

// args |-> body
static pmath_t make_arrow_function(pmath_expr_t boxes) {
  pmath_t args = parse_at(boxes, 1);
  
  if(!is_parse_error(args)) {
    pmath_t body = parse_at(boxes, 3);
    
    if(!is_parse_error(body)) {
      if(pmath_is_expr_of(args, pmath_System_Sequence)) {
        args = pmath_expr_set_item(
                 args, 0,
                 pmath_ref(pmath_System_List));
      }
      else {
        args = pmath_expr_new_extended(
                 pmath_ref(pmath_System_List), 1,
                 args);
      }
      
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Function), 2,
                 args,
                 body));
    }
    
    pmath_unref(args);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// f @@ list
static pmath_t make_apply(pmath_expr_t boxes) {
  pmath_t f = parse_at(boxes, 1);
  
  if(!is_parse_error(f)) {
    pmath_t list = parse_at(boxes, 3);
    
    if(!is_parse_error(list)) {
      return wrap_hold_with_debuginfo_from(
               boxes,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Apply), 2,
                 list,
                 f));
    }
    
    pmath_unref(f);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// sym::tag
static pmath_t make_message_name(pmath_expr_t boxes) {
  pmath_t sym;
  pmath_string_t tag = box_as_string(pmath_expr_get_item(boxes, 3));
  
  if(pmath_is_null(tag)) {
    pmath_message(PMATH_NULL, "inv", 1, boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  sym = parse_at(boxes, 1);
  if(!is_parse_error(sym)) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_MessageName), 2,
               sym,
               tag));
  }
  
  pmath_unref(tag);
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// ~x:t  ~~x:t  ~~~x:t
static pmath_t make_typed_named_match(pmath_expr_t boxes) {
  pmath_t x, t;
  
  x = parse_at(boxes, 2);
  if(is_parse_error(x)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  t = parse_at(boxes, 4);
  if(is_parse_error(t)) {
    pmath_unref(x);
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  if(unichar_at(boxes, 1) == '~') {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Pattern), 2,
               x,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_SingleMatch), 1,
                 t)));
  }
  
  if(is_string_at(boxes, 1, "~~")) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Pattern), 2,
               x,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Repeated), 2,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_SingleMatch), 1,
                   t),
                 pmath_ref(_pmath_object_range_from_one))));
  }
  
  if(is_string_at(boxes, 1, "~~~")) {
    return wrap_hold_with_debuginfo_from(
             boxes,
             pmath_expr_new_extended(
               pmath_ref(pmath_System_Pattern), 2,
               x,
               pmath_expr_new_extended(
                 pmath_ref(pmath_System_Repeated), 2,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_SingleMatch), 1,
                   t),
                 pmath_ref(_pmath_object_range_from_zero))));
  }
  
  pmath_unref(x);
  pmath_unref(t);
  pmath_message(PMATH_NULL, "inv", 1, boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// tag/: lhs:= rhs   tag/: lhs::= rhs
static pmath_t make_tag_assignment(pmath_expr_t boxes) {
  pmath_t tag, head;
  
  if(     unichar_at(boxes, 4) == PMATH_CHAR_ASSIGN)        head = pmath_System_TagAssign;
  else if(unichar_at(boxes, 4) == PMATH_CHAR_ASSIGNDELAYED) head = pmath_System_TagAssignDelayed;
  else if(is_string_at(boxes, 4, ":="))                     head = pmath_System_TagAssign;
  else if(is_string_at(boxes, 4, "::="))                    head = pmath_System_TagAssignDelayed;
  else {
    handle_row_error_at(boxes, 4);
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  tag = parse_at(boxes, 1);
  if(!is_parse_error(tag)) {
    pmath_t lhs = parse_at(boxes, 3);
    
    if(!is_parse_error(lhs)) {
      pmath_t rhs;
      
      if( pmath_same(head, pmath_System_TagAssign) &&
          unichar_at(boxes, 5) == '.')
      {
        return wrap_hold_with_debuginfo_from(
                 boxes,
                 pmath_expr_new_extended(
                   pmath_ref(pmath_System_TagUnassign), 2,
                   tag,
                   lhs));
      }
      
      rhs = parse_at(boxes, 5);
      if(!is_parse_error(rhs)) {
        return wrap_hold_with_debuginfo_from(
                 boxes,
                 pmath_expr_new_extended(
                   pmath_ref(head), 3,
                   tag,
                   lhs,
                   rhs));
      }
      
      pmath_unref(lhs);
    }
    
    pmath_unref(tag);
  }
  
  pmath_unref(boxes);
  return pmath_ref(pmath_System_DollarFailed);
}

// a + b - c ...
static pmath_t make_plus(pmath_expr_t boxes) {
  size_t i, exprlen;
  pmath_expr_t result;
  pmath_t arg = parse_at(boxes, 1);
  
  if(is_parse_error(arg)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  exprlen = pmath_expr_length(boxes);
  
  result = pmath_expr_set_item(
             pmath_expr_new(
               pmath_ref(pmath_System_Plus),
               (1 + exprlen) / 2),
             1,
             arg);
             
  for(i = 1; i <= exprlen / 2; ++i) {
    uint16_t ch;
    
    arg = parse_at(boxes, 2 * i + 1);
    if(is_parse_error(arg)) {
      pmath_unref(result);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    ch = unichar_at(boxes, 2 * i);
    if(ch == '-') {
      if(pmath_is_number(arg)) {
        arg = pmath_number_neg(arg);
      }
      else {
        arg = pmath_expr_new_extended(
                pmath_ref(pmath_System_Times), 2,
                PMATH_FROM_INT32(-1),
                arg);
      }
    }
    else if(ch != '+' && ch != PMATH_CHAR_INVISIBLEPLUS) {
      pmath_unref(arg);
      pmath_unref(result);
      handle_row_error_at(boxes, 2 * i);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    result = pmath_expr_set_item(result, i + 1, arg);
  }
  
  return wrap_hold_with_debuginfo_from(boxes, result);
}

//  a/b/c...
static pmath_t make_division(pmath_expr_t boxes) {
  pmath_expr_t result;
  size_t previous_rational;
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(boxes);
  
  previous_rational = 0;
  
  for(i = 4; i < exprlen; i += 2) {
    uint16_t ch = unichar_at(boxes, i);
    int tokprec;
    
    pmath_token_analyse(&ch, 1, &tokprec);
    
    if(tokprec != PMATH_PREC_DIV) {
      handle_row_error_at(boxes, i);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
  }
  
  result = pmath_expr_new(pmath_ref(pmath_System_Times), (1 + exprlen) / 2);
  for(i = 0; i <= exprlen / 2; ++i) {
    pmath_t arg = parse_at(boxes, 2 * i + 1);
    
    if(is_parse_error(arg)) {
      pmath_unref(result);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    if(i > 0) {
      if( pmath_is_integer(arg) &&
          !pmath_same(arg, INT(0)))
      {
        arg = pmath_rational_new(INT(1), arg);
        
        if(previous_rational == i) {
          pmath_rational_t prev = pmath_expr_get_item(result, i);
          result = pmath_expr_set_item(result, i, PMATH_UNDEFINED);
          
          arg = _mul_nn(prev, arg);
        }
        
        previous_rational = i + 1;
      }
      else
        arg = INV(arg);
    }
    else if(pmath_is_rational(arg))
      previous_rational = 1;
      
    result = pmath_expr_set_item(result, i + 1, arg);
  }
  
  if(previous_rational > 0) {
    pmath_t first = pmath_expr_get_item(result, 1);
    if(pmath_same(first, INT(1)))
      result = pmath_expr_set_item(result, 1, PMATH_UNDEFINED);
    else
      pmath_unref(first);
      
    result = _pmath_expr_shrink_associative(result, PMATH_UNDEFINED);
  }
  
  return wrap_hold_with_debuginfo_from(boxes, result);
}

// a && b & c ...
static pmath_t make_and(pmath_expr_t boxes) {
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(boxes);
  
  for(i = 4; i < exprlen; i += 2) { // token at index 2 already checked
    pmath_string_t op = pmath_expr_get_item(boxes, i);
    
    if( (pmath_string_length(op) != 1 ||
         *pmath_string_buffer(&op) != 0x2227) &&
        !string_equals(op, "&&"))
    {
      pmath_unref(op);
      handle_row_error_at(boxes, i);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    pmath_unref(op);
  }
  
  return make_infix_unchecked(boxes, pmath_System_And);
}

// a || b || c ...
static pmath_t make_or(pmath_expr_t boxes) {
  size_t i, exprlen;
  
  exprlen = pmath_expr_length(boxes);
  
  for(i = 4; i < exprlen; i += 2) { // token at index 2 already checked
    pmath_string_t op = pmath_expr_get_item(boxes, i);
    
    if( (pmath_string_length(op) != 1 ||
         *pmath_string_buffer(&op) != 0x2228) &&
        !string_equals(op, "||"))
    {
      pmath_unref(op);
      handle_row_error_at(boxes, i);
      pmath_unref(boxes);
      return pmath_ref(pmath_System_DollarFailed);
    }
    
    pmath_unref(op);
  }
  
  return make_infix_unchecked(boxes, pmath_System_Or);
}

// l[args]
static pmath_t make_part(pmath_expr_t boxes) {
  pmath_t args, list;
  size_t i;
  
  list = parse_at(boxes, 1);
  if(is_parse_error(list)) {
    pmath_unref(boxes);
    return pmath_ref(pmath_System_DollarFailed);
  }
  
  args = _pmath_makeexpression_with_debuginfo(pmath_expr_get_item(boxes, 3));
  
  if(pmath_is_expr(args)) {
    size_t argslen = pmath_expr_length(args);
    
    pmath_expr_t result = pmath_expr_set_item(
                            pmath_expr_new(
                              pmath_ref(pmath_System_Part),
                              argslen + 1),
                            1, list);
                            
    for(i = argslen + 1; i > 1; --i)
      result = pmath_expr_set_item(
                 result, i,
                 pmath_expr_get_item(args, i - 1));
                 
    pmath_unref(args);
    return wrap_hold_with_debuginfo_from(boxes, result);
  }
  
  pmath_unref(boxes);
  pmath_unref(list);
  pmath_unref(args);
  return pmath_ref(pmath_System_DollarFailed);
}

// a..b   a..b..c  with any of (a,b,c) optional
static pmath_t make_range(pmath_expr_t boxes) {
  size_t have_arg = FALSE;
  size_t i, exprlen;
  pmath_t arg;
  pmath_t result;
  
  exprlen = pmath_expr_length(boxes);
  
  pmath_gather_begin(PMATH_NULL);
  
  for(i = 1; i <= exprlen; ++i) {
    if(is_string_at(boxes, i, "..")) {
      if(!have_arg)
        pmath_emit(pmath_ref(pmath_System_Automatic), PMATH_NULL);
        
      have_arg = FALSE;
    }
    else {
      if(have_arg) {
        pmath_unref(pmath_gather_end());
        handle_row_error_at(boxes, i);
        pmath_unref(boxes);
        return pmath_ref(pmath_System_DollarFailed);
      }
      
      have_arg = TRUE;
      
      if(are_linebreaks_only_at(boxes, i))
        arg = pmath_ref(pmath_System_Automatic);
      else
        arg = parse_at(boxes, i);
        
      if(is_parse_error(arg)) {
        pmath_unref(boxes);
        pmath_unref(pmath_gather_end());
        return pmath_ref(pmath_System_DollarFailed);
      }
      
      pmath_emit(arg, PMATH_NULL);
    }
  }
  
  if(!have_arg)
    pmath_emit(pmath_ref(pmath_System_Automatic), PMATH_NULL);
    
  result = pmath_gather_end();
  result = pmath_expr_set_item(result, 0, pmath_ref(pmath_System_Range));
  return wrap_hold_with_debuginfo_from(boxes, result);
}

static pmath_t make_multiplication(pmath_expr_t boxes) {
  size_t i, exprlen;
  uint16_t ch;
  
  exprlen = pmath_expr_length(boxes);
  
  pmath_gather_begin(PMATH_NULL);
  
  i = 1;
  while(i <= exprlen) {
    pmath_t box = parse_at(boxes, i);
    if(is_parse_error(box)) {
      pmath_unref(boxes);
      pmath_unref(pmath_gather_end());
      return pmath_ref(pmath_System_DollarFailed);
    }
    pmath_emit(box, PMATH_NULL);
    
    ch = (i + 1 >= exprlen) ? 0 : unichar_at(boxes, i + 1);
    if(ch == ' ' || ch == '*' || ch == PMATH_CHAR_TIMES || ch == PMATH_CHAR_INVISIBLETIMES)
      i += 2;
    else
      ++i;
  }
  
  return wrap_hold_with_debuginfo_from(
           boxes,
           pmath_expr_set_item(
             pmath_gather_end(), 0,
             pmath_ref(pmath_System_Times)));
}
