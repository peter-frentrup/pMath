#ifndef __PMATH_LANGUAGE__SCANNER_H__
#define __PMATH_LANGUAGE__SCANNER_H__

#include <pmath-core/strings.h>

/**\defgroup parsing_code Parsing Code
   \brief Translating pMath code or boxes to pMath objects.

    The pMath language is defined as a 2-dimensional layout rather than one-dimensional
    text to support standard mathematical notation (such as sums, fractions, etc.).
    A low-level representation of this 2-dimensional box structure is based on the
    \ref pmath_span_array_t, which may be generated from textual code (and possibly
    embedded boxes) with pmath_spans_from_string(). A span-array can be converted to
    its boxes expression with pmath_boxes_from_spans_ex(). Thoses boxes then can be
    transformed to an unevaluated expression (wrapped in ``System`HoldComplete``)
    by applying ``System`MakeExpression`` to the boxes with pmath_evaluate().
    To finally execute the parsed code, evaluate that with the HoldComplete head removed.

   \section Example
    The boxed form of \f$ \sum_{i=1}^n f(i) \f$ is \n
    <tt>{UnderoverscriptBox("\[Sum]", {"i", "=", "1"}, "n"), {"f", "(", "i", ")"}}</tt>
    \n or \n
    <tt>{{"\[Sum]", SubsuperscriptBox({"i", "=", "1"}, "n")}, {"f", "(", "i", ")"}}</tt>
    \n (<tt>\[Sum]</tt> is the Unicode character U+2211: "N-ARY SUMMATION").

    It will be translated to <tt>HoldComplete(Sum(f(i), i->1..n))</tt> by
    <tt>System`MakeExpression</tt>.

    Front-ends have to convert their own representation of the (two-dimensional) code to
    the boxed form by providing appropriate callback functions to pmath_boxes_from_spans_ex()
    and pmath_boxes_from_spans_ex().

   @{
 */

/**\class pmath_span_array_t
   \brief Internal flat representaion of spans.

   The box-form of the expression <tt>aa*bbb+cc^-dd*ee</tt> is
   <tt>{{"aa", "*", "bbb"}, "+", {{"cc", "^", {"-", "dd"}}, "*", "ee"}}</tt>

   But those lists are not practicable for front-ends, so pMath provides a flat
   representation called \em span-array. It is an array of spans and flags (see
   pmath_span_at(), pmath_span_array_is_token_end(),
   pmath_span_array_is_operand_start()).

   The above code would be scanned to the following span array (the text itself
   is not stored):
   \verbatim
                aa*bbb+cc^-dd*ee
operand start:  x  x   x  xx  x
token end:       xx  xx xxx xx x
       /                  \_/
spans <         \____/ \____/
       \               \_______/
        \       \______________/
index:          0    5    10   15
   \endverbatim

   So at index 0 is a span which ends with index 15. It has a next span (a
   shorter span that starts at the same position) which ends with index 5. \n
   Another span is at index 7. It ends with index 15 and has a next span which
   ends with index 12. \n
   The last span is at index 10 and also ends with index 12.

   No two spans may cross-overlap so there is a definite hierachy. You create
   such a span-array with pmath_spans_from_string() and destroy it with
   pmath_span_array_free(). Conversion to and from boxes expressions can be done
   via pmath_boxes_from_spans() and pmath_spans_from_boxes() respectively.
 */
typedef struct _pmath_span_array_t pmath_span_array_t;

/**\class pmath_span_t
   \brief Represents a span in a \ref pmath_span_array_t "span-array".
 */
typedef struct _pmath_span_t       pmath_span_t;

/**\brief Destroy a span-array and all its spans.
   \memberof pmath_span_array_t
   \param spans  The span-array.
 */
PMATH_API void pmath_span_array_free(pmath_span_array_t *spans);

/**\brief Get a span-array's length.
   \memberof pmath_span_array_t
   \param spans  The span-array.
   \return Its length or 0 if it's PMATH_NULL.
 */
PMATH_API int pmath_span_array_length(pmath_span_array_t *spans);

/**\brief Test the token-end-flag at an index.
   \memberof pmath_span_array_t
   \param spans  The span-array.
   \param pos    The position. Must be between <tt>0</tt> and
                 <tt>pmath_span_array_length(spans)-1</tt>.
   \return Whether a token ends at the specified position.
 */
PMATH_API pmath_bool_t pmath_span_array_is_token_end(
  pmath_span_array_t *spans,
  int                 pos);

/**\brief Test the operator-start-flag at an index.
   \memberof pmath_span_array_t
   \param spans  The span-array.
   \param pos    The position. Must be between <tt>0</tt> and
                 <tt>pmath_span_array_length(spans)-1</tt>.
   \return Whether an operator starts at the specified position.
 */
PMATH_API pmath_bool_t pmath_span_array_is_operand_start(
  pmath_span_array_t *spans,
  int                 pos);

/**\brief Get a span starting at an index.
   \memberof pmath_span_array_t
   \param spans  The span-array.
   \param pos    The position. Must be between <tt>0</tt> and
                 <tt>pmath_span_array_length(spans)-1</tt>.
   \return The largest span stating at \a pos or PMATH_NULL if there is no span.
 */
PMATH_API pmath_span_t *pmath_span_at(pmath_span_array_t *spans, int pos);

/**\brief Get the next-shorter span starting at the same position.
   \memberof pmath_span_t
   \param span  A span.
   \return The next-shorter span stating at \a pos or PMATH_NULL if there is no span.
 */
PMATH_API pmath_span_t *pmath_span_next(pmath_span_t *span);

/**\brief Get end of a span.
   \memberof pmath_span_t
   \param span  A span.
   \return The last index which is covered by the span.
 */
PMATH_API int pmath_span_end(pmath_span_t *span);

/**\brief Parses pMath code to a span array.
   \memberof pmath_span_array_t
   \param code                        A pointer to a pMath string.
   \param line_reader                 An optional function to be called, when
                                      there is more input needed. Its result
                                      will be appended to *code.
   \param subsuperscriptbox_at_index  An optional function that returns TRUE iff
                                      at a given position in the code (indicated
                                      by the \ref PMATH_CHAR_BOX character) is a
                                      SubscriptBox, SuperscriptBox or
                                      SubsuperscriptBox.
   \param underoverscriptbox_at_index [optional] If there is an UnderscriptBox,
                                      OverscriptBox or UnderoverscriptBox at a
                                      given position in the code (indicated by
                                      the \ref PMATH_CHAR_BOX character) its base
                                      (e.g. middle part of UnderoverscriptBox)
                                      should be returned by this function,
                                      otherwise PMATH_NULL should be returned.
   \param error                       A function that will be called on syntax
                                      errors. The first argument is \c *code.
                                      It must not be freed. The second argument
                                      is the position in the code. The third
                                      argument is \a data. The fourth argument
                                      is TRUE if the error is critical and FALSE
                                      if it is just a warning (Syntax::newl)
                                      This function is optional, if it is
                                      NULL, no messages will be generated
                                      during the scanning.
   \param data                        An arbitrary pointer, that will be
                                      provided as the last argument to the
                                      callback functions.
   \return A span-array that can be used by pmath_boxes_from_spans to convert
           the code to boxed form, which, in turn, is used by
           System`MakeExpression(). The span-array must be freed with
           pmath_span_array_free() when it is no longer needed.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_span_array_t *pmath_spans_from_string(
  pmath_string_t   *code,
  pmath_string_t  (*line_reader)(void*),
  pmath_bool_t    (*subsuperscriptbox_at_index)(int, void*),
  pmath_string_t  (*underoverscriptbox_at_index)(int, void*),
  void            (*error)(pmath_string_t, int, void*, pmath_bool_t), //does not free 1st arg
  void             *data);

/**\brief Convert a span-array with the according code to boxed form.
   \memberof pmath_span_array_t
   \param spans         A span-array. It can be obtained by
                        pmath_spans_from_string() or pmath_spans_from_boxes().
   \param string        The corresponding code to \a span. It wont be freed.
   \param parseable     Whether whitespace and comments should be skipped or
                        not.
   \param box_at_index  An optional function that returns the box at a given
                        position (indicated by the PMATH_CHAR_BOX character).
                        This function will be called (at most) one time for
                        every box and in their order of apperance.
   \param data          A pointer that will be provided as the last argument to
                        \a box_at_index.
   \return A pMath object representing the boxed form. It must be freed.

   \deprecated Use pmath_boxes_from_spans_ex() instead.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_boxes_from_spans(
  pmath_span_array_t   *spans,
  pmath_string_t        string, // wont be freed
  pmath_bool_t          parseable,
  pmath_t             (*box_at_index)(int, void*),
  void                 *data);

/**\class pmath_text_position_t
   \brief A string position with line and column number.
 */
struct pmath_text_position_t {
  /**\brief 0-based character index. */
  int index;

  /**\brief 0-based line number. */
  int line;

  /**\brief 0-based index of beginning of the line.
   */
  int line_start_index;
};

/**\class pmath_boxes_from_spans_ex_t
   \brief Settings for pmath_boxes_from_spans_ex()

   \see pmath_boxes_from_spans_ex
 */
struct pmath_boxes_from_spans_ex_t{
  /**\brief The structure size.
     Allways initialize this with \c sizeof(pmath_boxes_from_spans_ex_t).
   */
  size_t size;

  /**\brief Flags to control the processing.

     This may be zero or more of the following values:
      - \c PMATH_BFS_PARSEABLE If whitespace and comments should be skipped.
   */
  int flags;

  /**\brief A pointer that will be provided as the last argument to callbacks
   */
  void *data;

  /**\brief An optional function that returns the box at a given position.

     \param index The position of the box (i.e. of a PMATH_CHAR_BOX character)
     \param data  The \a data member.
     \return A new pMath object representing the box.

     This function is called to turn PMATH_CHAR_BOX characters into boxes.
     It will be called (at most) one time for every box and in their order of
     apperance.
   */
  pmath_t (*box_at_index)(int index, void *data);

  /**\brief An optional function that adds debug information to a box.

     \param token_or_span A pMath object that should be enriched with debug
                          information. The callback takes ownership of this.
     \param start The start index of the token or span.
     \param end   The index of the first character after the token or span.
                  I.e. the exclusive end.
     \param data  The \a data member
     \return The modified token_or_span.

     \see pmath_expr_get_debug_metadata
   */
  pmath_t (*add_debug_metadata)(
    pmath_t                             token_or_span,
    const struct pmath_text_position_t *start,
    const struct pmath_text_position_t *end,
    void                               *data);
};

/**\internal */
enum {
  PMATH_BFS_PARSEABLE    = 1 << 0,
  PMATH_BFS_USESTRINGBOX = 1 << 1
};

/**\brief Convert a span-array with the according code to boxed form.
   \memberof pmath_span_array_t
   \param spans     A span-array. It can be obtained by
                    pmath_spans_from_string() or pmath_spans_from_boxes().
   \param string    The corresponding code to \a span. It wont be freed.
   \param settings  Optional additional settings.

   \return A pMath object representing the boxed form. It must be freed.

 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_boxes_from_spans_ex(
  pmath_span_array_t                 *spans,
  pmath_string_t                      string, // wont be freed
  struct pmath_boxes_from_spans_ex_t *settings);

/**\brief Convert boxed form back to span-array and code.
   \memberof pmath_span_array_t
   \param boxes          A pMath object representing the boxed form.
   \param result_string  A pointer where the resulting code will go to. Its
                         previous value is ignored.
   \param make_box       A function that converts a boxed form (pMath object) to
                         an actual box. It must free this object (the second
                         argument).
   \param data           A pointer that will be provided to make_box as the last
                         argument.
   \return A span-array. It must be freed with pmath_span_array_free() when it
           is no longer needed.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_span_array_t *pmath_spans_from_boxes(
  pmath_t           boxes,         // will be freed;
  pmath_string_t   *result_string,
  void            (*make_box)(int, pmath_t, void*),          // frees 2nd arg
  void             *data);

/**\brief Expand a string that contains boxes to a list of Strings and Boxes
   \relates pmath_string_t
   \param s  The string to be expanded. It will be freed.
   \return A string if there is nothing to expand or an expression with head
           StringBox representing \arg s as boxes.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_string_expand_boxes(pmath_string_t s);

/**\brief Parse a string to an expression.
   \relates pmath_string_t
   \param code  A pMath String representing the code. It will be freed.
   \return A pMath object.

   This function returns ToExpression("code"), but
   does not evaluate this released result.
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_parse_string(pmath_string_t code);

/**\brief Parse a string with additional arguments to an expression.
   \relates pmath_string_t
   \param code    A pMath String representing the code. It will be freed.
   \param format  A format string for the arguments.
   \param ...     The additional arguments.
   \return A pMath object.

   This function is a short hand for
   -# assigning pmath_build_value(format, ...) to the symbol $ParserArguments
   -# then calling pmath_parse_string(PMATH_C_STRING(code)) and
   -# restoring the old value of $ParserArguments
   -# returning the result of the pmath_parse_string-call.

   \see pmath_build_value
   \see pmath_parse_string
 */
PMATH_API
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_parse_string_args(
  const char *code,
  const char *format,
  ...);

/**\brief Execute some pMath code.
   \param code The code as a C string (zero terminated).
 */
#define PMATH_RUN(code) \
  pmath_unref( \
               pmath_evaluate( \
                               pmath_parse_string( \
                                   PMATH_C_STRING(code))))

/**\brief Execute some pMath code with arguments.
   \param code    The code as a C string.
   \param format  The argument's type format string.
   \param ...     The arguments.

   See pmath_build_value() for the meaning of \a format and \a ...
 */
#define PMATH_RUN_ARGS(code, format, ...) \
  pmath_unref( \
    pmath_evaluate( \
      pmath_parse_string_args( \
        (code), (format), __VA_ARGS__)))

/**\brief Test whether an object is a string representing a valid pMath namespace.
 */
PMATH_API pmath_bool_t pmath_is_namespace(pmath_t name);

/**\brief Test whether an object is a list of valid pMath namespace names.
 */
PMATH_API pmath_bool_t pmath_is_namespace_list(pmath_t list);

/** @} */

#endif // __PMATH_LANGUAGE__SCANNER_H__
